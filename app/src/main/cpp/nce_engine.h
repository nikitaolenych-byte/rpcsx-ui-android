/**
 * NCE Engine заголовковий файл
 * Native Code Execution для ARMv9/Cortex-X4
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
 * Встановлення режиму NCE (для UI)
 * @param mode 0=Disabled, 1=Interpreter, 2=Recompiler, 3=NCE/JIT
 */
void SetNCEMode(int mode);

/**
 * Отримання поточного режиму NCE
 * @return Поточний режим (0-3)
 */
int GetNCEMode();

/**
 * Перевірка чи NCE/JIT активний
 * @return true якщо NCE/JIT включено і готове
 */
bool IsNCEActive();

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
 * Інвалідація code cache
 */
void InvalidateCodeCache();

/**
 * Завершення роботи NCE
 */
void ShutdownNCE();

} // namespace rpcsx::nce

#endif // RPCSX_NCE_ENGINE_H
