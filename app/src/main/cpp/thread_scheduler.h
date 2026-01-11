/**
 * Thread Scheduler заголовковий файл
 */

#ifndef RPCSX_THREAD_SCHEDULER_H
#define RPCSX_THREAD_SCHEDULER_H

#include <pthread.h>
#include <cstddef>

namespace rpcsx::scheduler {

enum class ThreadType {
    PPU,        // PowerPC Processing Unit - найвищий пріоритет
    SPU,        // Synergistic Processing Unit - векторна обробка
    RENDERER,   // Graphics rendering threads
    AUDIO,      // Audio processing - критична латентність
    BACKGROUND  // Background tasks
};

/**
 * Ініціалізація scheduler системи
 */
bool InitializeScheduler();

/**
 * Вимкнення енергозбереження
 */
void DisablePowerSaving();

/**
 * Налаштування афінності потоку
 */
bool SetThreadAffinity(pthread_t thread, ThreadType type);

/**
 * Налаштування пріоритету потоку
 */
bool SetThreadPriority(pthread_t thread, ThreadType type);

/**
 * Повна ініціалізація потоку
 */
bool InitializeGameThread(pthread_t thread, ThreadType type, const char* name);

/**
 * Оптимізація пулу SPU потоків
 */
void OptimizeSPUThreadPool(pthread_t* threads, size_t count);

/**
 * Моніторинг завантаження ядер
 */
void MonitorCoreUtilization();

/**
 * Завершення роботи scheduler
 */
void ShutdownScheduler();

/**
 * Вимкнення CPU frequency scaling
 */
bool DisableCpuScaling(int cpu_id);

} // namespace rpcsx::scheduler

#endif // RPCSX_THREAD_SCHEDULER_H
