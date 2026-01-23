/**
 * Vulkan 1.3 Integration with Mesa Turnip for Adreno 735
 * Optimized for maximum compatibility and performance
 * Multithreaded RSX Graphics Engine for PS3 Emulation
 */

#ifndef RPCSX_VULKAN_RENDERER_H
#define RPCSX_VULKAN_RENDERER_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <memory>

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
 * Shutdown Vulkan resources
 */
void ShutdownVulkan();

// ============================================================================
// PS3 RSX Graphics Engine - Multithreaded Renderer
// ============================================================================

/**
 * RSX Graphics Command Queue Entry
 * Represents a single graphics command from PS3 to be executed on GPU
 */
struct RSXCommand {
    enum class Type : uint32_t {
        INVALID = 0,
        DRAW_ARRAYS,
        DRAW_INDEXED,
        CLEAR,
        SET_VIEWPORT,
        SET_SCISSOR,
        BIND_PIPELINE,
        BIND_DESCRIPTORS,
        SYNC_POINT,
        NOP
    };
    
    Type type;
    uint32_t data[256];  // Flexible command data
    uint16_t data_size;
    uint16_t priority;   // For prioritization on ARM big.LITTLE
};

/**
 * RSX Render Target Configuration
 * Handles color/depth surfaces for PS3 emulation
 */
struct RSXRenderTarget {
    uint32_t width, height;
    VkFormat color_format;
    VkFormat depth_format;
    uint64_t color_offset;   // PS3 VRAM offset
    uint64_t depth_offset;   // PS3 VRAM offset
    VkImage color_image;
    VkImage depth_image;
    VkImageView color_view;
    VkImageView depth_view;
};

/**
 * Multithreaded RSX Graphics Renderer
 * Executes PS3 graphics commands on Cortex-X4 cores with Vulkan backend
 */
class RSXGraphicsEngine {
public:
    RSXGraphicsEngine();
    ~RSXGraphicsEngine();
    
    /**
     * Initialize the RSX engine with thread pool
     * @param num_threads Number of worker threads (recommended: 2-4 on Cortex-X4)
     * @param vk_device Vulkan device
     * @param vk_queue Graphics queue
     * @return true if initialized successfully
     */
    bool Initialize(uint32_t num_threads, VkDevice vk_device, VkQueue vk_queue);
    
    /**
     * Submit a graphics command to the command queue
     * Thread-safe: can be called from any thread
     */
    void SubmitCommand(const RSXCommand& cmd);
    
    /**
     * Submit multiple commands atomically
     * Useful for batch operations (e.g., multiple draw calls)
     */
    void SubmitCommands(const RSXCommand* cmds, uint32_t count);
    
    /**
     * Wait for all pending commands to complete
     */
    void Flush();
    
    /**
     * Process all queued commands (called by worker threads)
     * @param cmd The command to process
     */
    void ProcessCommand(const RSXCommand& cmd);
    
    /**
     * Set active render target
     */
    void SetRenderTarget(const RSXRenderTarget& rt);
    
    /**
     * Get graphics stats for performance monitoring
     */
    struct GraphicsStats {
        uint64_t total_commands;
        uint64_t total_draws;
        uint64_t total_clears;
        uint64_t gpu_wait_cycles;
        double avg_queue_depth;
    };
    void GetGraphicsStats(GraphicsStats* stats);
    
    /**
     * Shutdown graphics engine and wait for worker threads
     */
    void Shutdown();
    
private:
    // Command queue management
    std::queue<RSXCommand> command_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Worker threads (one per core on big clusters)
    std::vector<std::thread> worker_threads_;
    volatile bool shutdown_flag_;
    
    // Vulkan resources
    VkDevice vk_device_;
    VkQueue vk_queue_;
    VkCommandPool command_pool_;
    
    // Current render state
    RSXRenderTarget current_rt_;
    
    // Performance counters
    std::atomic<uint64_t> total_commands_;
    std::atomic<uint64_t> total_draws_;
    std::atomic<uint64_t> total_clears_;
    
    // Worker thread main loop
    void WorkerThreadMain();
};

/**
 * Global RSX graphics engine instance
 * Access from main thread and graphics callback threads
 */
extern RSXGraphicsEngine* g_rsx_engine;

/**
 * Create and initialize the RSX graphics engine
 */
bool InitializeRSXEngine(VkDevice vk_device, VkQueue vk_queue, uint32_t num_threads);

/**
 * Submit command to RSX engine
 * Can be called from any thread (e.g., PPU emulation thread)
 */
inline void RSXSubmitCommand(const RSXCommand& cmd) {
    if (g_rsx_engine) {
        g_rsx_engine->SubmitCommand(cmd);
    }
}

/**
 * Wait for RSX to complete current operations
 */
void RSXFlush();

/**
 * Shutdown RSX graphics engine
 */
void ShutdownRSXEngine();

} // namespace rpcsx::vulkan

#endif // RPCSX_VULKAN_RENDERER_H
