/**
 * NCE Engine заголовковий файл
 * Native Code Execution для ARMv9/Cortex-X4
 * PowerPC Cell PPU → ARM64 JIT компілятор для PS3 емуляції
 * 
 * NCE Native v2.0 - Your Phone IS PlayStation 3!
 * - Real PS3 memory map (256MB XDR + 256MB GDDR3)
 * - LV2 syscall translation (80+ syscalls)
 * - PPU → ARM64 JIT execution
 * - SPU → ARM NEON (6 threads)
 * - RSX → Vulkan rendering
 */

#ifndef RPCSX_NCE_ENGINE_H
#define RPCSX_NCE_ENGINE_H

#include <cstdint>
#include <cstddef>

// NCE Version - Native Edition
#define NCE_VERSION_MAJOR 2
#define NCE_VERSION_MINOR 0
#define NCE_VERSION_PATCH 0
#define NCE_VERSION_STRING "2.0.0-native"
#define NCE_CODENAME "Native"

namespace rpcsx::nce {

/**
 * Ініціалізація Native Code Execution engine
 * @return true якщо успішно
 */
bool InitializeNCE();

/**
 * Встановлення режиму NCE (для UI)
 * @param mode 0=Disabled, 1=Interpreter, 2=Recompiler, 3=NCE (activates NCE Native!)
 */
void SetNCEMode(int mode);

/**
 * Отримання поточного режиму NCE
 * @return Поточний режим (0-4)
 */
int GetNCEMode();

/**
 * Перевірка чи NCE/JIT активний
 * @return true якщо NCE/JIT включено і готове
 */
bool IsNCEActive();

/**
 * Перевірка чи NCE Native активний (Phone IS PS3)
 * @return true якщо NCE Native запущено
 */
bool IsNCENativeActive();

/**
 * Завантаження та запуск PS3 гри через NCE Native
 * @param game_path Шлях до ELF/SELF файлу
 * @return true якщо успішно
 */
bool LoadAndStartGame(const char* game_path);

/**
 * Зупинка запущеної гри
 */
void StopGame();

/**
 * Трансляція x86-64 коду в ARM64 через JIT
 * @param ppu_code Вхідний x86-64 код
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

/**
 * Отримання статистики JIT компілятора
 * @param cache_usage Використаний розмір кешу (байт)
 * @param block_count Кількість скомпільованих блоків
 * @param exec_count Кількість виконань
 */
void GetJITStats(size_t* cache_usage, size_t* block_count, uint64_t* exec_count);

/**
 * Запуск JIT циклу виконання
 * @param start_code Початковий код x86-64
 * @param start_addr Віртуальна адреса
 * @param max_instructions Максимум інструкцій
 * @return true якщо успішно
 */
bool RunJIT(const uint8_t* start_code, uint64_t start_addr, size_t max_instructions);

// ============================================================================
// NCE v8 Extensions
// ============================================================================

/**
 * Отримання версії NCE
 */
const char* GetNCEVersion();

/**
 * Ініціалізація NCE v8 з усіма оптимізаціями
 */
bool InitializeNCEv8();

/**
 * Примусовий tier-up для гарячого коду
 */
void ForceTierUp(uint64_t address);

/**
 * Отримання точності branch prediction
 */
double GetBranchPredictionAccuracy();

/**
 * Встановлення рівня компіляції
 * @param tier 0=Interpreter, 1=Baseline, 2=Optimizing, 3=Megamorphic
 */
void SetCompilationTier(int tier);

/**
 * Отримання поточного рівня компіляції
 */
int GetCompilationTier();

/**
 * Увімкнення/вимкнення спекулятивного виконання
 */
void SetSpeculativeExecution(bool enable);

/**
 * Увімкнення/вимкнення векторизації циклів
 */
void SetLoopVectorization(bool enable);

/**
 * Отримання детальної статистики NCE v8
 */
struct NCEv8Stats {
    uint64_t total_executions;
    uint64_t total_compilations;
    uint64_t tier_ups;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t branch_predictions;
    uint64_t branch_mispredictions;
    uint64_t loops_vectorized;
    double branch_accuracy;
};

void GetNCEv8Stats(NCEv8Stats* stats);

} // namespace rpcsx::nce

#endif // RPCSX_NCE_ENGINE_H
