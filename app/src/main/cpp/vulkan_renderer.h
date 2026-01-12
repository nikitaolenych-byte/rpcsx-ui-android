/**
 * Vulkan 1.3 Integration з Mesa Turnip для Adreno 735
 * Оптимізації для максимальної сумісності та продуктивності
 */

#ifndef RPCSX_VULKAN_RENDERER_H
#define RPCSX_VULKAN_RENDERER_H

#include <vulkan/vulkan.h>
#include <cstdint>

namespace rpcsx::vulkan {

struct VulkanConfig {
    bool enable_validation;
    bool enable_async_compute;
    bool enable_mesh_shaders;
    bool enable_ray_tracing;
    bool purge_cache_on_start;  // Примусове очищення кешу
    uint32_t max_frames_in_flight;
    const char* cache_directory;  // Директорія для shader cache
};

/**
 * Ініціалізація Vulkan 1.3 з Mesa Turnip
 */
bool InitializeVulkan(const VulkanConfig& config);

/**
 * Створення логічного пристрою з оптимізаціями для Adreno
 */
VkDevice CreateOptimizedDevice(VkPhysicalDevice physical_device);

/**
 * Примусове очищення всіх shader кешів
 */
void PurgeAllShaderCaches(const char* base_directory);

/**
 * Налаштування descriptor pools для емулятора
 */
VkDescriptorPool CreateDescriptorPool(VkDevice device, uint32_t max_sets);

/**
 * Command buffer allocation з оптимізаціями
 */
VkCommandBuffer AllocateCommandBuffer(VkDevice device, VkCommandPool pool);

/**
 * Створення swapchain для Android surface
 */
VkSwapchainKHR CreateSwapchain(VkDevice device, VkSurfaceKHR surface,
                               uint32_t width, uint32_t height);

/**
 * Async shader compilation
 */
void CompileShaderAsync(const uint8_t* spirv_code, size_t size,
                       VkShaderModule* out_module);

/**
 * Memory allocation з оптимізаціями для LPDDR5X
 */
VkDeviceMemory AllocateMemory(VkDevice device, VkDeviceSize size,
                              uint32_t memory_type_index);

/**
 * Pipeline cache management
 */
VkPipelineCache CreatePipelineCache(VkDevice device, const char* cache_file);
void SavePipelineCache(VkDevice device, VkPipelineCache cache, const char* cache_file);

/**
 * Очищення Vulkan ресурсів
 */
void ShutdownVulkan();

} // namespace rpcsx::vulkan

#endif // RPCSX_VULKAN_RENDERER_H
