/**
 * RPCSX Vulkan Renderer with AGVSOL Integration
 * 
 * High-performance Vulkan renderer optimized for mobile GPUs.
 * Uses AGVSOL profiles for vendor-specific optimizations.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <array>
#include <functional>
#include <atomic>
#include <mutex>

#include "gpu_detector.h"
#include "agvsol_manager.h"
#include "vulkan_agvsol_integration.h"
#include "shader_compiler.h"

namespace rpcsx {
namespace vulkan {

// =============================================================================
// Forward Declarations
// =============================================================================

class SwapchainManager;
class PipelineManager;
class ResourceManager;
class CommandManager;

// =============================================================================
// Renderer Configuration
// =============================================================================

struct RendererConfig {
    // Window/Surface
    uint32_t width = 1920;
    uint32_t height = 1080;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    
    // Quality settings
    float resolution_scale = 1.0f;
    int msaa_samples = 1;
    bool enable_vsync = true;
    bool enable_hdr = false;
    
    // Features
    bool enable_async_compute = true;
    bool enable_async_transfer = true;
    bool enable_agvsol_optimization = true;
    
    // Buffer counts
    uint32_t swapchain_image_count = 3;
    uint32_t frame_in_flight_count = 2;
    
    // Memory budgets (MB)
    uint32_t texture_memory_budget = 512;
    uint32_t vertex_memory_budget = 128;
    uint32_t staging_memory_budget = 64;
};

// =============================================================================
// Frame Statistics
// =============================================================================

struct FrameStats {
    // Timing
    float frame_time_ms = 0.0f;
    float cpu_time_ms = 0.0f;
    float gpu_time_ms = 0.0f;
    float fps = 0.0f;
    
    // Draw calls
    uint32_t draw_calls = 0;
    uint32_t triangles = 0;
    uint32_t vertices = 0;
    
    // Memory
    uint64_t gpu_memory_used = 0;
    uint64_t staging_memory_used = 0;
    
    // Pipeline
    uint32_t pipeline_binds = 0;
    uint32_t descriptor_binds = 0;
    uint32_t buffer_uploads = 0;
    uint32_t texture_uploads = 0;
    
    // AGVSOL
    bool agvsol_active = false;
    std::string active_profile;
};

// =============================================================================
// Render Target
// =============================================================================

struct RenderTarget {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t samples = 1;
    
    // Optimization hints
    bool is_compressed = false;  // AFBC/UBWC
    bool is_transient = false;   // Lazily allocated
};

// =============================================================================
// Mesh Data
// =============================================================================

struct MeshData {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    VkDeviceMemory index_memory = VK_NULL_HANDLE;
    
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT16;
    
    // Vertex format
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

// =============================================================================
// Texture
// =============================================================================

struct Texture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_levels = 1;
    
    bool is_compressed = false;
};

// =============================================================================
// Uniform Buffer
// =============================================================================

struct UniformBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped_data = nullptr;
    size_t size = 0;
};

// =============================================================================
// Draw Command
// =============================================================================

struct DrawCommand {
    MeshData* mesh = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    
    std::vector<VkDescriptorSet> descriptor_sets;
    
    // Push constants
    const void* push_constant_data = nullptr;
    size_t push_constant_size = 0;
    
    // Instance data
    uint32_t instance_count = 1;
    uint32_t first_instance = 0;
    
    // Indexed drawing
    bool use_index_buffer = true;
    uint32_t index_count = 0;
    uint32_t first_index = 0;
    int32_t vertex_offset = 0;
};

// =============================================================================
// Vulkan Renderer Class
// =============================================================================

class VulkanRenderer {
public:
    static VulkanRenderer& Instance();
    
    // Lifecycle
    bool Initialize(const RendererConfig& config);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    
    // Frame management
    bool BeginFrame();
    void EndFrame();
    void Present();
    
    // Rendering commands
    void BeginRenderPass(const RenderTarget* color_target, 
                         const RenderTarget* depth_target = nullptr);
    void EndRenderPass();
    
    void SetViewport(float x, float y, float width, float height);
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);
    
    void BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point);
    void BindDescriptorSets(VkPipelineLayout layout, 
                            const std::vector<VkDescriptorSet>& sets,
                            VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);
    
    void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages,
                       uint32_t offset, uint32_t size, const void* data);
    
    void Draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0, uint32_t first_instance = 0);
    
    void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1,
                     uint32_t first_index = 0, int32_t vertex_offset = 0,
                     uint32_t first_instance = 0);
    
    void DrawMesh(const DrawCommand& cmd);
    
    // Compute
    void DispatchCompute(uint32_t group_count_x, uint32_t group_count_y, 
                         uint32_t group_count_z);
    
    // Resource creation (using AGVSOL optimizations)
    RenderTarget CreateRenderTarget(uint32_t width, uint32_t height, 
                                     VkFormat format, uint32_t samples = 1);
    void DestroyRenderTarget(RenderTarget& target);
    
    Texture CreateTexture(uint32_t width, uint32_t height, VkFormat format,
                          const void* data = nullptr, size_t data_size = 0);
    void DestroyTexture(Texture& texture);
    
    MeshData CreateMesh(const void* vertex_data, size_t vertex_size, uint32_t vertex_count,
                        const void* index_data, size_t index_size, uint32_t index_count);
    void DestroyMesh(MeshData& mesh);
    
    UniformBuffer CreateUniformBuffer(size_t size, bool host_visible = true);
    void UpdateUniformBuffer(UniformBuffer& buffer, const void* data, size_t size);
    void DestroyUniformBuffer(UniformBuffer& buffer);
    
    // Pipeline creation
    VkPipeline CreateGraphicsPipeline(
        VkRenderPass render_pass,
        const std::vector<VkPipelineShaderStageCreateInfo>& stages,
        const VkPipelineVertexInputStateCreateInfo* vertex_input,
        VkPipelineLayout layout,
        bool use_agvsol_hints = true);
    
    VkPipeline CreateComputePipeline(
        VkPipelineShaderStageCreateInfo compute_stage,
        VkPipelineLayout layout);
    
    // Getters
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
    VkInstance GetInstance() const { return m_instance; }
    VkCommandBuffer GetCurrentCommandBuffer() const;
    uint32_t GetCurrentFrame() const { return m_current_frame; }
    
    const FrameStats& GetFrameStats() const { return m_frame_stats; }
    const RendererConfig& GetConfig() const { return m_config; }
    
    // AGVSOL integration
    void SetAGVSOLProfile(const std::string& profile_name);
    void EnableAGVSOLOptimization(bool enable);
    std::string GetCurrentAGVSOLProfile() const;
    
    // Dynamic quality adjustment
    void SetTargetFPS(float target_fps);
    void EnableDynamicResolution(bool enable);
    float GetCurrentResolutionScale() const;

private:
    VulkanRenderer() = default;
    ~VulkanRenderer() = default;
    
    // Initialization helpers
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateRenderPasses();
    bool CreateFramebuffers();
    bool CreateCommandPools();
    bool CreateSyncObjects();
    bool InitializeAGVSOL();
    
    // Frame helpers
    void WaitForFrame();
    void AcquireSwapchainImage();
    void SubmitFrame();
    
    // Memory helpers
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
    VkDeviceMemory AllocateMemory(VkMemoryRequirements requirements, 
                                   VkMemoryPropertyFlags properties);
    
    // Dynamic adjustment
    void AdjustQualityForPerformance();
    
    bool m_initialized = false;
    RendererConfig m_config;
    
    // Vulkan core objects
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_compute_queue = VK_NULL_HANDLE;
    VkQueue m_transfer_queue = VK_NULL_HANDLE;
    uint32_t m_graphics_queue_family = 0;
    uint32_t m_compute_queue_family = 0;
    uint32_t m_transfer_queue_family = 0;
    
    // Swapchain
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;
    VkFormat m_swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchain_extent = {};
    
    // Render passes
    VkRenderPass m_main_render_pass = VK_NULL_HANDLE;
    VkRenderPass m_post_process_render_pass = VK_NULL_HANDLE;
    
    // Framebuffers
    std::vector<VkFramebuffer> m_framebuffers;
    RenderTarget m_depth_target;
    RenderTarget m_msaa_target;
    
    // Command pools and buffers
    VkCommandPool m_graphics_command_pool = VK_NULL_HANDLE;
    VkCommandPool m_compute_command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_command_buffers;
    
    // Synchronization
    std::vector<VkSemaphore> m_image_available_semaphores;
    std::vector<VkSemaphore> m_render_finished_semaphores;
    std::vector<VkFence> m_in_flight_fences;
    
    // Frame state
    uint32_t m_current_frame = 0;
    uint32_t m_image_index = 0;
    bool m_frame_started = false;
    bool m_render_pass_active = false;
    
    // Statistics
    FrameStats m_frame_stats;
    std::chrono::high_resolution_clock::time_point m_frame_start_time;
    
    // Dynamic resolution
    bool m_dynamic_resolution_enabled = false;
    float m_target_fps = 60.0f;
    float m_current_resolution_scale = 1.0f;
    std::array<float, 60> m_frame_time_history = {};
    uint32_t m_frame_time_index = 0;
    
    // AGVSOL integration
    bool m_agvsol_enabled = true;
    std::string m_current_profile;
    std::mutex m_render_mutex;
};

// =============================================================================
// Helper Macros
// =============================================================================

#define VK_CHECK(call) \
    do { \
        VkResult result = (call); \
        if (result != VK_SUCCESS) { \
            LOGE("Vulkan error: %d at %s:%d", result, __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)

#define VK_CHECK_VOID(call) \
    do { \
        VkResult result = (call); \
        if (result != VK_SUCCESS) { \
            LOGE("Vulkan error: %d at %s:%d", result, __FILE__, __LINE__); \
            return; \
        } \
    } while(0)

} // namespace vulkan
} // namespace rpcsx
