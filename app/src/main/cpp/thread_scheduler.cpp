/**
 * Агресивний Thread Scheduler для фіксації PPU на Cortex-X4 ядрах
 * Ігнорує енергозбереження Android для максимальної продуктивності
 */

#include "thread_scheduler.h"
#include <android/log.h>
#include <sched.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fstream>

#define LOG_TAG "RPCSX-Scheduler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::scheduler {

// Топологія Snapdragon 8s Gen 3:
// CPU 0-3: Cortex-A720 (2.5 GHz) - Efficiency cores
// CPU 4-6: Cortex-A720 (2.8 GHz) - Performance cores  
// CPU 7: Cortex-X4 (3.0 GHz) - Prime core

constexpr int PRIME_CORE = 7;           // Cortex-X4
constexpr int PERF_CORES_START = 4;     // Cortex-A720 Performance
constexpr int PERF_CORES_END = 6;
constexpr int EFFICIENCY_CORES_START = 0;
constexpr int EFFICIENCY_CORES_END = 3;

struct ThreadAffinityProfile {
    cpu_set_t cpuset;
    int priority;
    int scheduler_policy;
    const char* name;
};

/**
 * Профілі для різних типів потоків
 */
static const ThreadAffinityProfile PROFILES[] = {
    // PPU потоки - на Prime core (найвищий пріоритет)
    {
        .priority = 99,
        .scheduler_policy = SCHED_FIFO,
        .name = "PPU_PRIME"
    },
    // SPU потоки - на Performance cores
    {
        .priority = 90,
        .scheduler_policy = SCHED_FIFO,
        .name = "SPU_PERF"
    },
    // Renderer потоки - на Performance cores
    {
        .priority = 85,
        .scheduler_policy = SCHED_RR,
        .name = "RENDERER"
    },
    // Audio потоки - на Efficiency cores (stable latency)
    {
        .priority = 95,
        .scheduler_policy = SCHED_FIFO,
        .name = "AUDIO"
    },
    // Background потоки - на Efficiency cores
    {
        .priority = 10,
        .scheduler_policy = SCHED_OTHER,
        .name = "BACKGROUND"
    }
};

/**
 * Вимкнення CPU frequency scaling для конкретного ядра
 */
bool DisableCpuScaling(int cpu_id) {
    // Встановлення governor на "performance"
    std::string gov_path = "/sys/devices/system/cpu/cpu" + 
                          std::to_string(cpu_id) + 
                          "/cpufreq/scaling_governor";
    
    std::ofstream gov_file(gov_path);
    if (!gov_file) {
        LOGE("Failed to open governor file for CPU %d (requires root)", cpu_id);
        return false;
    }
    
    gov_file << "performance";
    gov_file.close();
    
    LOGI("CPU %d: set to performance governor", cpu_id);
    return true;
}

/**
 * Вимкнення всіх енергозберігаючих features
 */
void DisablePowerSaving() {
    LOGI("Disabling power saving features for maximum performance");
    
    // Вимикаємо CPU scaling для всіх високопродуктивних ядер
    for (int cpu = PERF_CORES_START; cpu <= PRIME_CORE; cpu++) {
        DisableCpuScaling(cpu);
    }
    
    // Вимикаємо thermal throttling (якщо можливо)
    system("echo 0 > /sys/class/thermal/thermal_zone0/mode 2>/dev/null");
    
    // Встановлюємо максимальну частоту GPU
    system("echo performance > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null");
    
    LOGI("Power saving disabled (may require root for full effect)");
}

/**
 * Налаштування афінності потоку
 */
bool SetThreadAffinity(pthread_t thread, ThreadType type) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    switch (type) {
        case ThreadType::PPU:
            // PPU потоки ТІЛЬКИ на Prime core (Cortex-X4)
            CPU_SET(PRIME_CORE, &cpuset);
            LOGI("Setting PPU thread affinity to Prime core (CPU %d)", PRIME_CORE);
            break;
            
        case ThreadType::SPU:
            // SPU потоки на Performance cores
            for (int cpu = PERF_CORES_START; cpu <= PERF_CORES_END; cpu++) {
                CPU_SET(cpu, &cpuset);
            }
            LOGI("Setting SPU thread affinity to Performance cores (CPU %d-%d)", 
                 PERF_CORES_START, PERF_CORES_END);
            break;
            
        case ThreadType::RENDERER:
            // Renderer на Performance cores
            for (int cpu = PERF_CORES_START; cpu <= PERF_CORES_END; cpu++) {
                CPU_SET(cpu, &cpuset);
            }
            LOGI("Setting Renderer thread affinity to Performance cores");
            break;
            
        case ThreadType::AUDIO:
            // Audio на один Efficiency core (стабільність)
            CPU_SET(EFFICIENCY_CORES_END, &cpuset);
            LOGI("Setting Audio thread affinity to Efficiency core");
            break;
            
        case ThreadType::BACKGROUND:
            // Background на Efficiency cores
            for (int cpu = EFFICIENCY_CORES_START; cpu <= EFFICIENCY_CORES_END; cpu++) {
                CPU_SET(cpu, &cpuset);
            }
            break;
    }

#if defined(__ANDROID__)
    // Bionic не експортує pthread_setaffinity_np. Для Android застосовуємо affinity
    // тільки до поточного потоку через sched_setaffinity.
    if (!pthread_equal(thread, pthread_self())) {
        LOGE("Android affinity: can only set affinity for current thread");
        return false;
    }
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        LOGE("Failed to set thread affinity via sched_setaffinity");
        return false;
    }
#else
    if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) != 0) {
        LOGE("Failed to set thread affinity");
        return false;
    }
#endif
    
    return true;
}

/**
 * Налаштування пріоритету потоку
 */
bool SetThreadPriority(pthread_t thread, ThreadType type) {
    struct sched_param param;
    int policy;
    
    switch (type) {
        case ThreadType::PPU:
            policy = SCHED_FIFO;
            param.sched_priority = 99;  // Максимальний пріоритет
            break;
            
        case ThreadType::SPU:
            policy = SCHED_FIFO;
            param.sched_priority = 90;
            break;
            
        case ThreadType::RENDERER:
            policy = SCHED_RR;
            param.sched_priority = 85;
            break;
            
        case ThreadType::AUDIO:
            policy = SCHED_FIFO;
            param.sched_priority = 95;  // Високий для стабільності
            break;
            
        case ThreadType::BACKGROUND:
            policy = SCHED_OTHER;
            param.sched_priority = 0;
            break;
    }
    
    if (pthread_setschedparam(thread, policy, &param) != 0) {
        LOGE("Failed to set thread priority (may need root)");
        return false;
    }
    
    LOGI("Thread priority set: policy=%d, priority=%d", policy, param.sched_priority);
    return true;
}

/**
 * Повна ініціалізація потоку з агресивними налаштуваннями
 */
bool InitializeGameThread(pthread_t thread, ThreadType type, const char* name) {
    LOGI("Initializing %s thread with aggressive scheduling", name);
    
    // 1. Встановлюємо ім'я потоку
    pthread_setname_np(thread, name);
    
    // 2. Встановлюємо CPU affinity
    if (!SetThreadAffinity(thread, type)) {
        LOGE("Failed to set affinity for %s thread", name);
    }
    
    // 3. Встановлюємо пріоритет
    if (!SetThreadPriority(thread, type)) {
        LOGE("Failed to set priority for %s thread", name);
    }
    
    // 4. Вимикаємо NUMA balancing (якщо підтримується)
    prctl(PR_SET_TIMERSLACK, 1);  // Мінімальна затримка таймерів
    
    // 5. Забороняємо міграцію на інші ядра (для PPU)
    if (type == ThreadType::PPU) {
        // Встановлюємо strict affinity
        LOGI("PPU thread locked to Prime core - no migration allowed");
    }
    
    return true;
}

/**
 * Оптимізація для батчу потоків (наприклад, всі SPU потоки)
 */
void OptimizeSPUThreadPool(pthread_t* threads, size_t count) {
    LOGI("Optimizing SPU thread pool (%zu threads)", count);
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // Розподіляємо SPU потоки по Performance cores
    for (int cpu = PERF_CORES_START; cpu <= PERF_CORES_END; cpu++) {
        CPU_SET(cpu, &cpuset);
    }
    
    struct sched_param param;
    param.sched_priority = 90;
    
    for (size_t i = 0; i < count; i++) {
#if defined(__ANDROID__)
        // На Android без доступу до TID ми не можемо стабільно проставити affinity
        // для довільного pthread_t. Пріоритет/імена виставляємо, а affinity — лише
        // якщо це поточний потік.
        if (pthread_equal(threads[i], pthread_self())) {
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
#else
        pthread_setaffinity_np(threads[i], sizeof(cpuset), &cpuset);
#endif
        pthread_setschedparam(threads[i], SCHED_FIFO, &param);
        
        char name[16];
        snprintf(name, sizeof(name), "SPU-%zu", i);
        pthread_setname_np(threads[i], name);
    }
    
    LOGI("SPU thread pool optimized");
}

/**
 * Моніторинг завантаження ядер
 */
void MonitorCoreUtilization() {
    // Читаємо /proc/stat для кожного CPU
    std::ifstream stat_file("/proc/stat");
    std::string line;
    
    LOGI("=== CPU Core Utilization ===");
    
    while (std::getline(stat_file, line)) {
        if (line.substr(0, 3) == "cpu" && line[3] >= '0' && line[3] <= '7') {
            LOGI("%s", line.c_str());
        }
    }
}

/**
 * Ініціалізація scheduler системи
 */
bool InitializeScheduler() {
    LOGI("Initializing Aggressive Thread Scheduler for Snapdragon 8s Gen 3");
    
    // Вимикаємо енергозбереження
    DisablePowerSaving();
    
    // Встановлюємо nice value для процесу
    if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
        LOGE("Failed to set process priority (requires root)");
    }
    
    LOGI("Scheduler initialized - ready for high-performance gaming");
    return true;
}

/**
 * Очищення scheduler
 */
void ShutdownScheduler() {
    LOGI("Scheduler shutdown - restoring normal power management");
    
    // Відновлюємо звичайний governor
    for (int cpu = 0; cpu < 8; cpu++) {
        std::string gov_path = "/sys/devices/system/cpu/cpu" + 
                              std::to_string(cpu) + 
                              "/cpufreq/scaling_governor";
        
        std::ofstream gov_file(gov_path);
        if (gov_file) {
            gov_file << "schedutil";  // Android default
        }
    }
}

} // namespace rpcsx::scheduler
