/**
 * Dynamic Resolution Scaling (DRS) Engine
 * 
 * Автоматичне масштабування роздільної здатності для підтримки
 * стабільного FPS на ARM64 пристроях.
 * 
 * Особливості:
 * - Моніторинг GPU/CPU навантаження в реальному часі
 * - Адаптивне масштабування від 50% до 100% native resolution
 * - Інтеграція з FSR 3.1 для апскейлінгу
 * - Оптимізація для Snapdragon 8s Gen 3
 */

#ifndef RPCSX_DRS_ENGINE_H
#define RPCSX_DRS_ENGINE_H

#include <cstdint>
#include <atomic>

namespace rpcsx::drs {

/**
 * Режими DRS
 */
enum class DRSMode {
    DISABLED = 0,       // Вимкнено - фіксована роздільна здатність
    PERFORMANCE = 1,    // Пріоритет FPS - агресивне зниження
    BALANCED = 2,       // Баланс якості та продуктивності
    QUALITY = 3         // Пріоритет якості - мінімальне зниження
};

/**
 * Конфігурація DRS
 */
struct DRSConfig {
    DRSMode mode = DRSMode::BALANCED;
    
    // Цільовий FPS (30, 60, 120)
    uint32_t target_fps = 30;
    
    // Мінімальний масштаб роздільної здатності (0.5 = 50%)
    float min_scale = 0.5f;
    
    // Максимальний масштаб (1.0 = 100% native)
    float max_scale = 1.0f;
    
    // Поріг падіння FPS для зниження роздільної здатності (%)
    float downscale_threshold = 0.85f;  // < 85% target FPS
    
    // Поріг для підвищення роздільної здатності (%)
    float upscale_threshold = 0.95f;    // > 95% target FPS
    
    // Швидкість адаптації (0.0-1.0)
    float adaptation_speed = 0.1f;
    
    // Затримка перед зміною (кадри)
    uint32_t hysteresis_frames = 10;
    
    // Увімкнути FSR апскейлінг
    bool use_fsr_upscale = true;
};

/**
 * Статистика DRS
 */
struct DRSStats {
    float current_scale;           // Поточний масштаб (0.5-1.0)
    float current_fps;             // Поточний FPS
    float average_fps;             // Середній FPS за останні N кадрів
    uint32_t render_width;         // Поточна ширина рендерингу
    uint32_t render_height;        // Поточна висота рендерингу
    uint32_t output_width;         // Вихідна ширина (після апскейлу)
    uint32_t output_height;        // Вихідна висота
    uint64_t scale_changes;        // Кількість змін масштабу
    float gpu_utilization;         // Завантаження GPU (%)
    float cpu_utilization;         // Завантаження CPU (%)
    bool is_scaling_down;          // Чи зараз знижується роздільна здатність
};

/**
 * Ініціалізація DRS Engine
 */
bool InitializeDRS(uint32_t native_width, uint32_t native_height,
                   const DRSConfig& config = DRSConfig());

/**
 * Завершення роботи DRS
 */
void ShutdownDRS();

/**
 * Перевірка чи DRS активний
 */
bool IsDRSActive();

/**
 * Встановлення режиму DRS
 */
void SetDRSMode(DRSMode mode);

/**
 * Отримання поточного режиму
 */
DRSMode GetDRSMode();

/**
 * Встановлення цільового FPS
 */
void SetTargetFPS(uint32_t fps);

/**
 * Отримання цільового FPS
 */
uint32_t GetTargetFPS();

/**
 * Встановлення мінімального масштабу
 */
void SetMinScale(float scale);

/**
 * Встановлення максимального масштабу
 */
void SetMaxScale(float scale);

/**
 * Оновлення стану DRS (викликається кожен кадр)
 * @param frame_time_ms Час останнього кадру в мілісекундах
 * @return Рекомендований масштаб роздільної здатності (0.5-1.0)
 */
float UpdateDRS(float frame_time_ms);

/**
 * Отримання поточного масштабу
 */
float GetCurrentScale();

/**
 * Отримання поточної роздільної здатності рендерингу
 */
void GetCurrentRenderResolution(uint32_t* width, uint32_t* height);

/**
 * Отримання статистики DRS
 */
void GetDRSStats(DRSStats* stats);

/**
 * Скидання статистики
 */
void ResetDRSStats();

/**
 * Увімкнення/вимкнення FSR апскейлінгу
 */
void SetFSRUpscaling(bool enabled);

/**
 * Перевірка чи FSR апскейлінг увімкнено
 */
bool IsFSRUpscalingEnabled();

/**
 * Встановлення конфігурації DRS
 */
void SetDRSConfig(const DRSConfig& config);

/**
 * Отримання поточної конфігурації
 */
DRSConfig GetDRSConfig();

// Глобальний стан для швидкого доступу
extern std::atomic<float> g_current_scale;
extern std::atomic<bool> g_drs_active;

} // namespace rpcsx::drs

#endif // RPCSX_DRS_ENGINE_H
