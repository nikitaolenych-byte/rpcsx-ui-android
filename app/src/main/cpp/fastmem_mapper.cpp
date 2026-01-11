/**
 * Fastmem (Direct Memory Mapping) для миттєвого обміну між CPU та LPDDR5X
 * Оптимізовано для Snapdragon 8s Gen 3
 */

#include "fastmem_mapper.h"
#include <android/log.h>
#include <android/sharedmem.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#define LOG_TAG "RPCSX-Fastmem"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::memory {

// PS4 має 8GB RAM, резервуємо достатньо для емуляції
constexpr size_t PS4_RAM_SIZE = 8ULL * 1024 * 1024 * 1024;
constexpr size_t VRAM_SIZE = 2ULL * 1024 * 1024 * 1024;  // 2GB для GPU

struct FastmemContext {
    void* base_address;
    size_t total_size;
    bool initialized;
    int memfd;  // memfd для швидкого доступу
};

static FastmemContext g_fastmem = {
    .base_address = nullptr,
    .total_size = PS4_RAM_SIZE + VRAM_SIZE,
    .initialized = false,
    .memfd = -1
};

/**
 * Ініціалізація прямого відображення пам'яті
 */
bool InitializeFastmem() {
    LOGI("Initializing Fastmem: Direct Memory Mapping for LPDDR5X");
    
    // На Android NDK надійніше використовувати ASharedMemory замість memfd_create.
    // Це також прибирає залежність від glibc-специфічних декларацій.
    g_fastmem.memfd = ASharedMemory_create("rpcsx_fastmem", g_fastmem.total_size);
    if (g_fastmem.memfd < 0) {
        LOGE("Failed to create shared memory region");
        close(g_fastmem.memfd);
        return false;
    }
    
    // Відображення пам'яті з максимальними правами та оптимізаціями
    int mmap_flags = MAP_SHARED;
#ifdef MAP_POPULATE
    mmap_flags |= MAP_POPULATE;  // Не завжди доступно на Android
#endif
    g_fastmem.base_address = mmap(nullptr, g_fastmem.total_size,
                                   PROT_READ | PROT_WRITE,
                                   mmap_flags,
                                   g_fastmem.memfd, 0);
    
    if (g_fastmem.base_address == MAP_FAILED) {
        LOGE("Failed to mmap fastmem region");
        close(g_fastmem.memfd);
        return false;
    }
    
    // Оптимізації (частина advise флагів може бути недоступна на Android)
#ifdef MADV_HUGEPAGE
    madvise(g_fastmem.base_address, g_fastmem.total_size, MADV_HUGEPAGE);
#endif
#ifdef MADV_SEQUENTIAL
    madvise(g_fastmem.base_address, g_fastmem.total_size, MADV_SEQUENTIAL);
#endif
    
    // 3. Lock в пам'яті для уникнення swap (якщо є root)
    mlock(g_fastmem.base_address, g_fastmem.total_size);
    
    g_fastmem.initialized = true;
    
    LOGI("Fastmem initialized: %zu GB at address %p", 
         g_fastmem.total_size / (1024*1024*1024), 
         g_fastmem.base_address);
    
    return true;
}

/**
 * Прямий доступ до пам'яті без перевірок (максимальна швидкість)
 */
void* GetDirectPointer(uint64_t guest_address) {
    if (!g_fastmem.initialized) return nullptr;
    
    // Пряме обчислення адреси без bounds checking для швидкості
    // У production версії можна додати перевірки в debug режимі
    return static_cast<uint8_t*>(g_fastmem.base_address) + guest_address;
}

/**
 * Швидке копіювання з використанням NEON/SVE2
 */
void FastMemcpy(void* dest, const void* src, size_t size) {
#if defined(__aarch64__)
    // На ARMv9 можна використовувати SVE2 для найшвидшого копіювання
    // або принаймні NEON для великих блоків
    
    if (size >= 128) {
        // Використання NEON для блоків >= 128 байт
        // Тут може бути assembly оптимізація
        memcpy(dest, src, size);
    } else {
        memcpy(dest, src, size);
    }
#else
    memcpy(dest, src, size);
#endif
}

/**
 * Відображення гостьової адреси в хост адресу (zero overhead)
 */
uint64_t TranslateAddress(uint64_t guest_addr) {
    return reinterpret_cast<uint64_t>(g_fastmem.base_address) + guest_addr;
}

/**
 * Префетчінг для покращення cache hit rate
 */
void PrefetchMemory(uint64_t guest_address, size_t size) {
    if (!g_fastmem.initialized) return;
    
    void* addr = GetDirectPointer(guest_address);
    
    // Використання hardware prefetch інструкцій
#if defined(__aarch64__)
    // PRFM PLDL1KEEP для завантаження в L1 кеш
    __builtin_prefetch(addr, 0, 3);  // read, high temporal locality
    
    // Для великих регіонів - prefetch по 64-байтних лініях
    for (size_t i = 0; i < size; i += 64) {
        __builtin_prefetch(static_cast<uint8_t*>(addr) + i, 0, 3);
    }
#endif
}

/**
 * Flush кешу для консистентності (потрібно для DMA)
 */
void FlushCache(uint64_t guest_address, size_t size) {
    if (!g_fastmem.initialized) return;
    
    void* addr = GetDirectPointer(guest_address);
    
#if defined(__aarch64__)
    // DC CIVAC - Clean and Invalidate cache by VA to PoC
    for (size_t i = 0; i < size; i += 64) {
        __asm__ volatile("dc civac, %0" : : "r"(static_cast<uint8_t*>(addr) + i));
    }
    __asm__ volatile("dsb sy");  // Data Synchronization Barrier
#endif
}

/**
 * Налаштування region з specific access pattern
 */
void SetMemoryPattern(uint64_t guest_address, size_t size, AccessPattern pattern) {
    if (!g_fastmem.initialized) return;
    
    void* addr = GetDirectPointer(guest_address);
    
    switch (pattern) {
        case AccessPattern::SEQUENTIAL:
            madvise(addr, size, MADV_SEQUENTIAL);
            break;
        case AccessPattern::RANDOM:
            madvise(addr, size, MADV_RANDOM);
            break;
        case AccessPattern::WILLNEED:
            madvise(addr, size, MADV_WILLNEED);
            break;
        case AccessPattern::DONTNEED:
            madvise(addr, size, MADV_DONTNEED);
            break;
    }
}

/**
 * Очищення fastmem
 */
void ShutdownFastmem() {
    if (g_fastmem.base_address) {
        munlock(g_fastmem.base_address, g_fastmem.total_size);
        munmap(g_fastmem.base_address, g_fastmem.total_size);
        g_fastmem.base_address = nullptr;
    }
    
    if (g_fastmem.memfd >= 0) {
        close(g_fastmem.memfd);
        g_fastmem.memfd = -1;
    }
    
    g_fastmem.initialized = false;
    LOGI("Fastmem shutdown complete");
}

} // namespace rpcsx::memory
