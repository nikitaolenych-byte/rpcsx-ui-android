/**
 * Adaptive Texture Streaming Cache
 * 
 * Інтелектуальне потокове завантаження текстур з адаптивним кешуванням
 * для оптимального використання пам'яті на мобільних пристроях.
 * 
 * Особливості:
 * - LRU кеш з пріоритетами на основі відстані до камери
 * - Асинхронне завантаження з пулом потоків
 * - Автоматичне mipmap streaming
 * - Стиснення ASTC/ETC2 для ARM
 * - Інтеграція з Vulkan для zero-copy transfer
 */

#ifndef RPCSX_TEXTURE_STREAMING_H
#define RPCSX_TEXTURE_STREAMING_H

#include <cstdint>
#include <atomic>
#include <string>
#include <functional>

namespace rpcsx::textures {

/**
 * Режими стрімінгу текстур
 */
enum class StreamingMode {
    DISABLED = 0,       // Усі текстури в пам'яті
    CONSERVATIVE = 1,   // Мінімальний стрімінг, максимальна якість
    BALANCED = 2,       // Баланс пам'яті та якості
    AGGRESSIVE = 3,     // Агресивний стрімінг для економії RAM
    ULTRA_LOW_MEM = 4   // Для пристроїв з < 4GB RAM
};

/**
 * Якість текстур
 */
enum class TextureQuality {
    ULTRA = 0,      // Повна роздільна здатність, без стиснення
    HIGH = 1,       // Повна роздільна здатність, ASTC 4x4
    MEDIUM = 2,     // 75% роздільної здатності, ASTC 6x6
    LOW = 3,        // 50% роздільної здатності, ASTC 8x8
    POTATO = 4      // 25% роздільної здатності, максимальне стиснення
};

/**
 * Конфігурація стрімінгу
 */
struct StreamingConfig {
    StreamingMode mode = StreamingMode::BALANCED;
    TextureQuality quality = TextureQuality::HIGH;
    
    // Максимальний розмір кешу текстур (в MB)
    uint32_t max_cache_size_mb = 512;
    
    // Розмір пулу для асинхронного завантаження
    uint32_t async_pool_size = 4;
    
    // Мінімальний mip рівень для стрімінгу (0 = повна якість)
    uint32_t min_mip_level = 0;
    
    // Максимальна затримка завантаження (мс)
    uint32_t max_load_delay_ms = 100;
    
    // Поріг відстані для prefetch (в юнітах гри)
    float prefetch_distance = 50.0f;
    
    // Увімкнути ASTC стиснення
    bool use_astc_compression = true;
    
    // Увімкнути анізотропну фільтрацію
    bool anisotropic_filtering = true;
    uint32_t anisotropy_level = 8;
};

/**
 * Статистика стрімінгу
 */
struct StreamingStats {
    uint64_t textures_loaded;         // Всього завантажених текстур
    uint64_t textures_streamed;       // Текстур через стрімінг
    uint64_t cache_hits;              // Попадань в кеш
    uint64_t cache_misses;            // Промахів кешу
    uint64_t bytes_streamed;          // Завантажено байт
    uint64_t bytes_cached;            // Закешовано байт
    uint32_t current_cache_size_mb;   // Поточний розмір кешу
    uint32_t pending_loads;           // Текстур в черзі
    float average_load_time_ms;       // Середній час завантаження
    float cache_hit_ratio;            // Відсоток попадань в кеш
};

/**
 * Дескриптор текстури для стрімінгу
 */
struct TextureDescriptor {
    uint64_t id;                      // Унікальний ID
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
    uint32_t array_layers;
    uint32_t format;                  // Vulkan format
    uint64_t gpu_address;             // Адреса в GPU пам'яті
    float priority;                   // Пріоритет завантаження (0-1)
    bool is_resident;                 // Чи в пам'яті
    uint32_t current_mip;             // Поточний завантажений mip
};

// =============================================================================
// API функції
// =============================================================================

/**
 * Ініціалізація системи стрімінгу
 */
bool InitializeTextureStreaming(const StreamingConfig& config = StreamingConfig());

/**
 * Завершення роботи
 */
void ShutdownTextureStreaming();

/**
 * Перевірка чи стрімінг активний
 */
bool IsStreamingActive();

/**
 * Встановлення режиму стрімінгу
 */
void SetStreamingMode(StreamingMode mode);
StreamingMode GetStreamingMode();

/**
 * Встановлення якості текстур
 */
void SetTextureQuality(TextureQuality quality);
TextureQuality GetTextureQuality();

/**
 * Встановлення максимального розміру кешу
 */
void SetMaxCacheSize(uint32_t size_mb);
uint32_t GetMaxCacheSize();

/**
 * Реєстрація текстури для стрімінгу
 */
uint64_t RegisterTexture(uint32_t width, uint32_t height, 
                         uint32_t mip_levels, uint32_t format,
                         const void* initial_data = nullptr);

/**
 * Видалення текстури
 */
void UnregisterTexture(uint64_t texture_id);

/**
 * Запит на завантаження текстури
 */
void RequestTextureLoad(uint64_t texture_id, float priority = 1.0f);

/**
 * Встановлення пріоритету текстури
 */
void SetTexturePriority(uint64_t texture_id, float priority);

/**
 * Оновлення системи стрімінгу (викликати кожен кадр)
 */
void UpdateStreaming(float camera_x, float camera_y, float camera_z);

/**
 * Примусове завантаження всіх текстур
 */
void FlushPendingLoads();

/**
 * Очищення кешу
 */
void ClearCache();

/**
 * Prefetch текстур для зони
 */
void PrefetchZone(float x, float y, float z, float radius);

/**
 * Отримання статистики
 */
void GetStreamingStats(StreamingStats* stats);

/**
 * Скидання статистики
 */
void ResetStreamingStats();

/**
 * Отримання конфігурації
 */
StreamingConfig GetStreamingConfig();

/**
 * Встановлення конфігурації
 */
void SetStreamingConfig(const StreamingConfig& config);

/**
 * Callback для завершення завантаження
 */
using TextureLoadCallback = std::function<void(uint64_t texture_id, bool success)>;
void SetTextureLoadCallback(TextureLoadCallback callback);

// Глобальні атомарні для швидкого доступу
extern std::atomic<bool> g_streaming_active;
extern std::atomic<uint32_t> g_current_cache_size_mb;

} // namespace rpcsx::textures

#endif // RPCSX_TEXTURE_STREAMING_H
