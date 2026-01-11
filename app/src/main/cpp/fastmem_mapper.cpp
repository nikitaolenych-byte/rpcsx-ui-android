/**
 * Fastmem (Direct Memory Mapping) для миттєвого обміну між CPU та LPDDR5X
 * Оптимізовано для Snapdragon 8s Gen 3
 */

#include "fastmem_mapper.h"
#include <android/log.h>
#include <android/sharedmem.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cinttypes>

#define LOG_TAG "RPCSX-Fastmem"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::memory {

// PS4 має 8GB RAM, резервуємо достатньо для емуляції
constexpr size_t PS4_RAM_SIZE = 8ULL * 1024 * 1024 * 1024;
constexpr size_t VRAM_SIZE = 2ULL * 1024 * 1024 * 1024;  // 2GB для GPU

static size_t RoundUp(size_t value, size_t alignment) {
    if (alignment == 0) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

struct FastmemContext {
    void* base_address;
    size_t total_size;
    bool initialized;
    size_t locked_size;
};

static FastmemContext g_fastmem = {
    .base_address = nullptr,
    .total_size = PS4_RAM_SIZE + VRAM_SIZE,
    .initialized = false,
    .locked_size = 0
};

static void* MapAlignedAnonymousRW(size_t size, size_t alignment) {
    // Ensure alignment is power-of-two for bitmask align-up.
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    const size_t reserve_size = size + alignment;
    void* reserve = mmap(nullptr, reserve_size, PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserve == MAP_FAILED) {
        return nullptr;
    }

    const uintptr_t reserve_addr = reinterpret_cast<uintptr_t>(reserve);
    const uintptr_t aligned_addr = (reserve_addr + alignment - 1) & ~(alignment - 1);
    const size_t prefix = static_cast<size_t>(aligned_addr - reserve_addr);
    const size_t suffix = reserve_size - prefix - size;

    if (prefix) {
        munmap(reserve, prefix);
    }
    if (suffix) {
        munmap(reinterpret_cast<void*>(aligned_addr + size), suffix);
    }

    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
#ifdef MAP_NORESERVE
    flags |= MAP_NORESERVE;
#endif
    void* mapped = mmap(reinterpret_cast<void*>(aligned_addr), size,
                        PROT_READ | PROT_WRITE, flags, -1, 0);
    if (mapped == MAP_FAILED) {
        // Best effort cleanup of the reserved middle.
        munmap(reinterpret_cast<void*>(aligned_addr), size);
        return nullptr;
    }

    return mapped;
}

/**
 * Ініціалізація прямого відображення пам'яті
 */
bool InitializeFastmem() {
    if (g_fastmem.initialized && g_fastmem.base_address) {
        return true;
    }

    LOGI("Initializing Fastmem: Direct Memory Mapping for LPDDR5X");

    // На Android page size може бути 4K/16K/64K залежно від ядра/SoC.
    // Для NCE/JIT та коректної memory protection потрібно вирівнювати
    // mmap/розмір мінімум на розмір сторінки.
    const long page_size_long = sysconf(_SC_PAGESIZE);
    const size_t page_size = page_size_long > 0 ? static_cast<size_t>(page_size_long) : 4096;
    // Android on newer ARMv9 devices may use 16KB pages; some kernels can be 64KB.
    // For safe JIT/NCE memory protection patterns we align to at least 64KB.
    const size_t required_alignment = std::max(page_size, static_cast<size_t>(65536));

    // На практиці 10GB mapping може не влізти у віртуальний адресний простір/ліміти.
    // Пробуємо деградацію розміру замість hard-fail/segfault.
    const size_t candidates[] = {
        RoundUp(PS4_RAM_SIZE + VRAM_SIZE, required_alignment),
        RoundUp(6ULL * 1024 * 1024 * 1024, required_alignment),
        RoundUp(4ULL * 1024 * 1024 * 1024, required_alignment),
        RoundUp(2ULL * 1024 * 1024 * 1024, required_alignment),
    };

    LOGI("Fastmem page_size=%zu required_alignment=%zu", page_size, required_alignment);
    
    for (size_t size_candidate : candidates) {
        g_fastmem.total_size = size_candidate;

        // Використовуємо anonymous mapping з MAP_NORESERVE (якщо доступно),
        // щоб уникнути величезних upfront-алокацій і крашів на старті.
        g_fastmem.base_address = MapAlignedAnonymousRW(g_fastmem.total_size, required_alignment);
        if (!g_fastmem.base_address) {
            LOGE("Failed to map fastmem region (size=%zu)", g_fastmem.total_size);
            continue;
        }

        // Успіх
        break;
    }

    if (!g_fastmem.base_address) {
        LOGE("Fastmem init failed: could not allocate any candidate size");
        return false;
    }
    
    // Оптимізації (частина advise флагів може бути недоступна на Android)
#ifdef MADV_HUGEPAGE
    madvise(g_fastmem.base_address, g_fastmem.total_size, MADV_HUGEPAGE);
#endif
#ifdef MADV_SEQUENTIAL
    madvise(g_fastmem.base_address, g_fastmem.total_size, MADV_SEQUENTIAL);
#endif
    
    // 3. Lock в пам'яті для уникнення swap (часто не дозволено без root). Не фейлимось.
    g_fastmem.locked_size = std::min(g_fastmem.total_size, static_cast<size_t>(64 * 1024 * 1024));
    (void)mlock(g_fastmem.base_address, g_fastmem.locked_size);
    
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

    if (guest_address >= g_fastmem.total_size) {
        LOGE("Fastmem OOB guest_address=0x%" PRIx64 " (size=%zu)", guest_address, g_fastmem.total_size);
        return nullptr;
    }
    
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
    if (!g_fastmem.initialized || !g_fastmem.base_address) return 0;
    if (guest_addr >= g_fastmem.total_size) return 0;
    return reinterpret_cast<uint64_t>(g_fastmem.base_address) + guest_addr;
}

/**
 * Префетчінг для покращення cache hit rate
 */
void PrefetchMemory(uint64_t guest_address, size_t size) {
    if (!g_fastmem.initialized) return;
    
    void* addr = GetDirectPointer(guest_address);
    if (!addr) return;
    
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
    if (!addr) return;
    
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
    if (!addr) return;
    
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
        if (g_fastmem.locked_size) {
            munlock(g_fastmem.base_address, g_fastmem.locked_size);
        }
        munmap(g_fastmem.base_address, g_fastmem.total_size);
        g_fastmem.base_address = nullptr;
    }

    g_fastmem.locked_size = 0;
    
    g_fastmem.initialized = false;
    LOGI("Fastmem shutdown complete");
}

} // namespace rpcsx::memory
