/**
 * Dynamic Resolution Scaling (DRS) Engine Implementation
 * 
 * Адаптивне масштабування роздільної здатності для стабільного FPS
 */

#include "drs_engine.h"
#include "fsr31/fsr31.h"
#include <android/log.h>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <deque>
#include <mutex>

#define LOG_TAG "RPCSX-DRS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::drs {

// Глобальні атомарні змінні для швидкого доступу
std::atomic<float> g_current_scale{1.0f};
std::atomic<bool> g_drs_active{false};

// Внутрішній стан DRS
struct DRSState {
    std::mutex mutex;
    
    DRSConfig config;
    DRSStats stats;
    
    uint32_t native_width = 1920;
    uint32_t native_height = 1080;
    
    float current_scale = 1.0f;
    float target_scale = 1.0f;
    
    // Історія FPS для аналізу
    std::deque<float> fps_history;
    static constexpr size_t FPS_HISTORY_SIZE = 60;
    
    // Лічильник кадрів для hysteresis
    uint32_t stable_frames = 0;
    bool was_scaling_down = false;
    
    // Час для розрахунку FPS
    std::chrono::steady_clock::time_point last_update;
    uint64_t frame_count = 0;
    
    bool initialized = false;
};

static DRSState g_state;

// Допоміжні функції
static float CalculateAverageFPS() {
    if (g_state.fps_history.empty()) return 0.0f;
    
    float sum = 0.0f;
    for (float fps : g_state.fps_history) {
        sum += fps;
    }
    return sum / g_state.fps_history.size();
}

static float CalculateScaleForMode(DRSMode mode, float current_fps, float target_fps) {
    float fps_ratio = current_fps / target_fps;
    float scale_adjustment = 0.0f;
    
    switch (mode) {
        case DRSMode::PERFORMANCE:
            // Агресивне зниження - швидко знижуємо при падінні FPS
            if (fps_ratio < 0.9f) {
                scale_adjustment = -0.15f;  // Швидке зниження
            } else if (fps_ratio < 0.95f) {
                scale_adjustment = -0.08f;
            } else if (fps_ratio > 1.0f) {
                scale_adjustment = 0.02f;   // Повільне підвищення
            }
            break;
            
        case DRSMode::BALANCED:
            // Збалансований підхід
            if (fps_ratio < 0.85f) {
                scale_adjustment = -0.10f;
            } else if (fps_ratio < 0.92f) {
                scale_adjustment = -0.05f;
            } else if (fps_ratio > 0.98f) {
                scale_adjustment = 0.03f;
            }
            break;
            
        case DRSMode::QUALITY:
            // Пріоритет якості - мінімальне зниження
            if (fps_ratio < 0.75f) {
                scale_adjustment = -0.08f;
            } else if (fps_ratio < 0.85f) {
                scale_adjustment = -0.03f;
            } else if (fps_ratio > 0.95f) {
                scale_adjustment = 0.05f;
            }
            break;
            
        case DRSMode::DISABLED:
        default:
            return 1.0f;
    }
    
    return scale_adjustment;
}

bool InitializeDRS(uint32_t native_width, uint32_t native_height,
                   const DRSConfig& config) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    
    if (g_state.initialized) {
        LOGW("DRS already initialized, reinitializing...");
        ShutdownDRS();
    }
    
    LOGI("╔════════════════════════════════════════════════════════════╗");
    LOGI("║       Dynamic Resolution Scaling (DRS) Engine              ║");
    LOGI("╠════════════════════════════════════════════════════════════╣");
    LOGI("║  Native Resolution: %4ux%-4u                              ║", native_width, native_height);
    LOGI("║  Target FPS: %u                                            ║", config.target_fps);
    LOGI("║  Min Scale: %.0f%%                                          ║", config.min_scale * 100);
    LOGI("║  Max Scale: %.0f%%                                         ║", config.max_scale * 100);
    LOGI("║  Mode: %s                                              ║", 
         config.mode == DRSMode::PERFORMANCE ? "PERFORMANCE" :
         config.mode == DRSMode::BALANCED ? "BALANCED" :
         config.mode == DRSMode::QUALITY ? "QUALITY" : "DISABLED");
    LOGI("╚════════════════════════════════════════════════════════════╝");
    
    g_state.native_width = native_width;
    g_state.native_height = native_height;
    g_state.config = config;
    g_state.current_scale = config.max_scale;
    g_state.target_scale = config.max_scale;
    g_state.fps_history.clear();
    g_state.stable_frames = 0;
    g_state.was_scaling_down = false;
    g_state.frame_count = 0;
    g_state.last_update = std::chrono::steady_clock::now();
    
    // Ініціалізація статистики
    g_state.stats = {};
    g_state.stats.current_scale = config.max_scale;
    g_state.stats.render_width = native_width;
    g_state.stats.render_height = native_height;
    g_state.stats.output_width = native_width;
    g_state.stats.output_height = native_height;
    
    // Оновлення глобальних атомарних змінних
    g_current_scale.store(config.max_scale);
    g_drs_active.store(config.mode != DRSMode::DISABLED);
    
    g_state.initialized = true;
    
    LOGI("DRS Engine initialized successfully");
    return true;
}

void ShutdownDRS() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    
    if (!g_state.initialized) return;
    
    LOGI("Shutting down DRS Engine...");
    
    g_state.fps_history.clear();
    g_state.initialized = false;
    g_drs_active.store(false);
    g_current_scale.store(1.0f);
    
    LOGI("DRS Engine shutdown complete");
}

bool IsDRSActive() {
    return g_drs_active.load();
}

void SetDRSMode(DRSMode mode) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    
    g_state.config.mode = mode;
    g_drs_active.store(mode != DRSMode::DISABLED);
    
    LOGI("DRS mode set to: %s", 
         mode == DRSMode::PERFORMANCE ? "PERFORMANCE" :
         mode == DRSMode::BALANCED ? "BALANCED" :
         mode == DRSMode::QUALITY ? "QUALITY" : "DISABLED");
    
    if (mode == DRSMode::DISABLED) {
        g_state.current_scale = 1.0f;
        g_state.target_scale = 1.0f;
        g_current_scale.store(1.0f);
    }
}

DRSMode GetDRSMode() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.config.mode;
}

void SetTargetFPS(uint32_t fps) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.config.target_fps = fps;
    LOGI("DRS target FPS set to: %u", fps);
}

uint32_t GetTargetFPS() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.config.target_fps;
}

void SetMinScale(float scale) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.config.min_scale = std::clamp(scale, 0.25f, 1.0f);
    LOGI("DRS min scale set to: %.0f%%", g_state.config.min_scale * 100);
}

void SetMaxScale(float scale) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.config.max_scale = std::clamp(scale, 0.25f, 1.0f);
    LOGI("DRS max scale set to: %.0f%%", g_state.config.max_scale * 100);
}

float UpdateDRS(float frame_time_ms) {
    if (!g_drs_active.load()) {
        return 1.0f;
    }
    
    std::lock_guard<std::mutex> lock(g_state.mutex);
    
    if (!g_state.initialized || g_state.config.mode == DRSMode::DISABLED) {
        return 1.0f;
    }
    
    // Розрахунок поточного FPS
    float current_fps = (frame_time_ms > 0.0f) ? (1000.0f / frame_time_ms) : 60.0f;
    
    // Додавання до історії
    g_state.fps_history.push_back(current_fps);
    if (g_state.fps_history.size() > DRSState::FPS_HISTORY_SIZE) {
        g_state.fps_history.pop_front();
    }
    
    float average_fps = CalculateAverageFPS();
    float target_fps = static_cast<float>(g_state.config.target_fps);
    
    // Розрахунок необхідного масштабу
    float scale_adjustment = CalculateScaleForMode(g_state.config.mode, average_fps, target_fps);
    
    // Визначення напрямку масштабування
    bool should_scale_down = (scale_adjustment < 0);
    
    // Hysteresis - затримка перед зміною напрямку
    if (should_scale_down != g_state.was_scaling_down) {
        g_state.stable_frames++;
        if (g_state.stable_frames < g_state.config.hysteresis_frames) {
            // Ще не час змінювати напрямок
            scale_adjustment = 0;
        } else {
            g_state.was_scaling_down = should_scale_down;
            g_state.stable_frames = 0;
        }
    } else {
        g_state.stable_frames = 0;
    }
    
    // Плавна інтерполяція масштабу
    g_state.target_scale += scale_adjustment;
    g_state.target_scale = std::clamp(g_state.target_scale, 
                                       g_state.config.min_scale, 
                                       g_state.config.max_scale);
    
    // Плавний перехід до цільового масштабу
    float adaptation = g_state.config.adaptation_speed;
    g_state.current_scale = g_state.current_scale + 
                           (g_state.target_scale - g_state.current_scale) * adaptation;
    
    // Оновлення глобального масштабу
    g_current_scale.store(g_state.current_scale);
    
    // Оновлення статистики
    g_state.stats.current_scale = g_state.current_scale;
    g_state.stats.current_fps = current_fps;
    g_state.stats.average_fps = average_fps;
    g_state.stats.render_width = static_cast<uint32_t>(g_state.native_width * g_state.current_scale);
    g_state.stats.render_height = static_cast<uint32_t>(g_state.native_height * g_state.current_scale);
    g_state.stats.output_width = g_state.native_width;
    g_state.stats.output_height = g_state.native_height;
    g_state.stats.is_scaling_down = should_scale_down;
    
    // Лічильник змін масштабу
    static float last_logged_scale = 1.0f;
    if (std::abs(g_state.current_scale - last_logged_scale) > 0.05f) {
        g_state.stats.scale_changes++;
        last_logged_scale = g_state.current_scale;
        LOGI("DRS scale changed to %.0f%% (FPS: %.1f/%.0f)", 
             g_state.current_scale * 100, average_fps, target_fps);
    }
    
    g_state.frame_count++;
    
    return g_state.current_scale;
}

float GetCurrentScale() {
    return g_current_scale.load();
}

void GetCurrentRenderResolution(uint32_t* width, uint32_t* height) {
    float scale = g_current_scale.load();
    
    std::lock_guard<std::mutex> lock(g_state.mutex);
    
    if (width) *width = static_cast<uint32_t>(g_state.native_width * scale);
    if (height) *height = static_cast<uint32_t>(g_state.native_height * scale);
}

void GetDRSStats(DRSStats* stats) {
    if (!stats) return;
    
    std::lock_guard<std::mutex> lock(g_state.mutex);
    *stats = g_state.stats;
}

void ResetDRSStats() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    
    g_state.stats.scale_changes = 0;
    g_state.fps_history.clear();
    g_state.stable_frames = 0;
    
    LOGI("DRS stats reset");
}

void SetFSRUpscaling(bool enabled) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.config.use_fsr_upscale = enabled;
    LOGI("DRS FSR upscaling %s", enabled ? "enabled" : "disabled");
}

bool IsFSRUpscalingEnabled() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.config.use_fsr_upscale;
}

void SetDRSConfig(const DRSConfig& config) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.config = config;
    g_drs_active.store(config.mode != DRSMode::DISABLED);
    LOGI("DRS config updated");
}

DRSConfig GetDRSConfig() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.config;
}

} // namespace rpcsx::drs
