/**
 * Fastmem заголовковий файл
 * Direct Memory Mapping для ARMv9/Cortex-X4
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
 * @return true якщо успішно
 */
bool InitializeFastmem();

/**
 * Отримання прямого вказівника на гостьову пам'ять
 * @param guest_address Адреса в гостьовому просторі
 * @return Хост вказівник або nullptr якщо OOB
 */
void* GetDirectPointer(uint64_t guest_address);

/**
 * Безпечний доступ з CrashGuard
 * @param guest_address Адреса в гостьовому просторі
 * @return Хост вказівник або nullptr при помилці
 */
void* GetDirectPointerSafe(uint64_t guest_address);

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
 * Отримання статистики доступів/помилок
 */
void GetFastmemStats(uint64_t* out_accesses, uint64_t* out_faults, size_t* out_size);

/**
 * Завершення роботи fastmem
 */
void ShutdownFastmem();

} // namespace rpcsx::memory

#endif // RPCSX_FASTMEM_MAPPER_H
