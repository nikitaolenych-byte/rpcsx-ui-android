/**
 * Vulkan Pipeline State Object (PSO) Caching
 * 
 * Високопродуктивне кешування Vulkan pipeline states для
 * швидкого перемикання між станами рендерингу.
 * 
 * Особливості:
 * - LRU кеш з persistence на диск
 * - Попереднє створення pipelines в background
 * - Hash-based lookup для O(1) доступу
 * - Зменшення stuttering при першому рендері
 * - Підтримка VK_EXT_graphics_pipeline_library
 */

#ifndef RPCSX_PIPELINE_CACHE_H
#define RPCSX_PIPELINE_CACHE_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <string>
#include <functional>

namespace rpcsx::pipeline {

/**
 * Типи pipeline
 */
enum class PipelineType : uint32_t {
    GRAPHICS = 0,
    COMPUTE = 1,
    RAY_TRACING = 2  // Для майбутнього
};

/**
 * Стан завантаження pipeline
 */
enum class PipelineState : uint32_t {
    NOT_FOUND = 0,      // Не існує
    PENDING = 1,        // В черзі на компіляцію
    COMPILING = 2,      // Компілюється
    READY = 3,          // Готовий до використання
    ERROR = 4           // Помилка компіляції
};

/**
 * Конфігурація кешу
 */
struct PipelineCacheConfig {
    // Максимальна кількість pipeline в пам'яті
    uint32_t max_cached_pipelines = 2048;
    
    // Розмір Vulkan pipeline cache (байти)
    size_t vk_cache_size = 64 * 1024 * 1024; // 64 MB
    
    // Шлях для збереження кешу
    std::string cache_path;
    
    // Кількість потоків для фонової компіляції
    uint32_t compile_threads = 2;
    
    // Увімкнути VK_EXT_graphics_pipeline_library
    bool use_pipeline_library = true;
    
    // Увімкнути попередню компіляцію
    bool enable_precompilation = true;
    
    // Зберігати кеш на диск при виході
    bool persist_to_disk = true;
    
    // Максимальний час компіляції (мс) перед timeout
    uint32_t compile_timeout_ms = 5000;
};

/**
 * Статистика кешу
 */
struct PipelineCacheStats {
    uint64_t total_pipelines_created;     // Всього створених pipelines
    uint64_t cache_hits;                   // Попадань в кеш
    uint64_t cache_misses;                 // Промахів кешу
    uint64_t pipelines_compiled;           // Скомпільованих pipelines
    uint64_t pipelines_evicted;            // Видалених з кешу
    uint64_t compile_time_total_ms;        // Загальний час компіляції
    uint32_t pipelines_in_cache;           // Поточна кількість в кеші
    uint32_t pending_compilations;         // В черзі на компіляцію
    float average_compile_time_ms;         // Середній час компіляції
    float cache_hit_ratio;                 // Відсоток попадань
    size_t cache_size_bytes;               // Розмір кешу в байтах
    size_t disk_cache_size_bytes;          // Розмір на диску
};

/**
 * Дескриптор Graphics Pipeline State
 */
struct GraphicsPipelineDesc {
    // Vertex input state
    uint32_t vertex_binding_count;
    uint32_t vertex_attribute_count;
    
    // Input assembly
    uint32_t topology;           // VkPrimitiveTopology
    bool primitive_restart;
    
    // Rasterization
    uint32_t polygon_mode;       // VkPolygonMode
    uint32_t cull_mode;          // VkCullModeFlags
    uint32_t front_face;         // VkFrontFace
    bool depth_clamp;
    bool rasterizer_discard;
    float line_width;
    
    // Depth/Stencil
    bool depth_test_enable;
    bool depth_write_enable;
    uint32_t depth_compare_op;   // VkCompareOp
    bool stencil_test_enable;
    
    // Color blending
    uint32_t color_attachment_count;
    bool blend_enable;
    uint32_t src_color_blend;    // VkBlendFactor
    uint32_t dst_color_blend;
    uint32_t color_blend_op;     // VkBlendOp
    uint32_t src_alpha_blend;
    uint32_t dst_alpha_blend;
    uint32_t alpha_blend_op;
    uint32_t color_write_mask;
    
    // Multisampling
    uint32_t sample_count;       // VkSampleCountFlagBits
    bool sample_shading;
    float min_sample_shading;
    
    // Shader hashes (для ідентифікації)
    uint64_t vertex_shader_hash;
    uint64_t fragment_shader_hash;
    uint64_t geometry_shader_hash;
    
    // Render pass info
    uint64_t render_pass_hash;
    uint32_t subpass_index;
    
    // Calculate hash for lookup
    uint64_t CalculateHash() const;
};

/**
 * Дескриптор Compute Pipeline
 */
struct ComputePipelineDesc {
    uint64_t compute_shader_hash;
    uint32_t local_size_x;
    uint32_t local_size_y;
    uint32_t local_size_z;
    uint64_t pipeline_layout_hash;
    
    uint64_t CalculateHash() const;
};

/**
 * Handle для pipeline
 */
using PipelineHandle = uint64_t;
constexpr PipelineHandle INVALID_PIPELINE = 0;

// =============================================================================
// API функції
// =============================================================================

/**
 * Ініціалізація системи кешування
 */
bool InitializePipelineCache(void* vk_device, void* vk_physical_device,
                             const PipelineCacheConfig& config = PipelineCacheConfig());

/**
 * Завершення роботи
 */
void ShutdownPipelineCache();

/**
 * Перевірка чи кеш активний
 */
bool IsCacheActive();

/**
 * Отримання або створення graphics pipeline
 * Повертає INVALID_PIPELINE якщо pipeline ще компілюється
 */
PipelineHandle GetOrCreateGraphicsPipeline(const GraphicsPipelineDesc& desc);

/**
 * Отримання або створення compute pipeline
 */
PipelineHandle GetOrCreateComputePipeline(const ComputePipelineDesc& desc);

/**
 * Перевірка стану pipeline
 */
PipelineState GetPipelineState(PipelineHandle handle);

/**
 * Отримання Vulkan VkPipeline з handle
 */
void* GetVkPipeline(PipelineHandle handle);

/**
 * Запит на попередню компіляцію pipeline
 */
void RequestPrecompile(const GraphicsPipelineDesc& desc);
void RequestPrecompile(const ComputePipelineDesc& desc);

/**
 * Реєстрація shader module
 */
uint64_t RegisterShaderModule(const void* spirv_code, size_t code_size,
                              uint32_t stage); // VkShaderStageFlagBits

/**
 * Видалення shader module
 */
void UnregisterShaderModule(uint64_t shader_hash);

/**
 * Реєстрація render pass
 */
uint64_t RegisterRenderPass(void* vk_render_pass);

/**
 * Видалення render pass
 */
void UnregisterRenderPass(uint64_t render_pass_hash);

/**
 * Flush всіх pending compilations
 */
void FlushCompilations();

/**
 * Очищення кешу
 */
void ClearCache();

/**
 * Збереження кешу на диск
 */
bool SaveCacheToDisk(const char* path = nullptr);

/**
 * Завантаження кешу з диску
 */
bool LoadCacheFromDisk(const char* path = nullptr);

/**
 * Встановлення callback для завершення компіляції
 */
using CompileCallback = std::function<void(PipelineHandle handle, bool success)>;
void SetCompileCallback(CompileCallback callback);

/**
 * Оновлення системи (викликати кожен кадр для background compilation)
 */
void Update();

/**
 * Отримання статистики
 */
void GetStats(PipelineCacheStats* stats);

/**
 * Скидання статистики
 */
void ResetStats();

/**
 * Встановлення конфігурації
 */
void SetConfig(const PipelineCacheConfig& config);

/**
 * Отримання конфігурації
 */
PipelineCacheConfig GetConfig();

/**
 * Отримання Vulkan Pipeline Cache handle
 */
void* GetVkPipelineCache();

// Глобальні атомарні
extern std::atomic<bool> g_cache_active;
extern std::atomic<uint32_t> g_pipelines_in_cache;
extern std::atomic<uint32_t> g_pending_compilations;

} // namespace rpcsx::pipeline

#endif // RPCSX_PIPELINE_CACHE_H
