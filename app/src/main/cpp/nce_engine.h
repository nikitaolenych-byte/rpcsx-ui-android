/**
 * NCE Engine заголовковий файл
 */

#ifndef RPCSX_NCE_ENGINE_H
#define RPCSX_NCE_ENGINE_H

#include <cstdint>
#include <cstddef>

namespace rpcsx::nce {

/**
 * Ініціалізація Native Code Execution engine
 * @return true якщо успішно
 */
bool InitializeNCE();

/**
 * Трансляція PPU коду в ARM64+SVE2
 * @param ppu_code Вхідний PPU код
 * @param code_size Розмір коду
 * @return Вказівник на перекладений ARM код
 */
void* TranslatePPUToARM(const uint8_t* ppu_code, size_t code_size);

/**
 * Виконання на високопродуктивних ядрах
 * @param native_code Перекладений код
 */
void ExecuteOnGoldenCore(void* native_code);

/**
 * SPU емуляція через SVE2 вектори
 * @param spu_code SPU інструкції
 * @param registers Регістри
 */
void EmulateSPUWithSVE2(const void* spu_code, void* registers);

/**
 * Завершення роботи NCE
 */
void ShutdownNCE();

} // namespace rpcsx::nce

#endif // RPCSX_NCE_ENGINE_H
