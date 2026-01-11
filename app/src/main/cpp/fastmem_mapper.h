/**
 * Fastmem заголовковий файл
 */

#ifndef RPCSX_FASTMEM_MAPPER_H
#define RPCSX_FASTMEM_MAPPER_H

#include <cstdint>
#include <cstddef>

namespace rpcsx::memory {

enum class AccessPattern {
    SEQUENTIAL,
    RANDOM,
    WILLNEED,
    DONTNEED
};

/**
 * Ініціалізація Direct Memory Mapping
 */
bool InitializeFastmem();

/**
 * Отримання прямого вказівника на гостьову пам'ять
 */
void* GetDirectPointer(uint64_t guest_address);

/**
 * Оптимізоване копіювання з NEON/SVE2
 */
void FastMemcpy(void* dest, const void* src, size_t size);

/**
 * Трансляція гостьової адреси в хост адресу
 */
uint64_t TranslateAddress(uint64_t guest_addr);

/**
 * Префетчінг пам'яті
 */
void PrefetchMemory(uint64_t guest_address, size_t size);

/**
 * Скидання кешу
 */
void FlushCache(uint64_t guest_address, size_t size);

/**
 * Налаштування access pattern для пам'яті
 */
void SetMemoryPattern(uint64_t guest_address, size_t size, AccessPattern pattern);

/**
 * Завершення роботи fastmem
 */
void ShutdownFastmem();

} // namespace rpcsx::memory

#endif // RPCSX_FASTMEM_MAPPER_H
