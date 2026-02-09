/**
 * RPCSX Vulkan-AGVSOL Integration
 * 
 * Connects AGVSOL GPU optimization profiles to the Vulkan renderer.
 * Applies vendor-specific optimizations at runtime.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

#include "gpu_detector.h"
#include "agvsol_manager.h"

namespace rpcsx {
namespace vulkan {

// =============================================================================
// Vulkan Device Wrapper with AGVSOL Integration
// =============================================================================

struct VulkanDeviceInfo {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    
    // Device properties
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceMemoryProperties memory_properties{};
    
    // Queue families
    uint32_t graphics_queue_family = UINT32_MAX;
    uint32_t compute_queue_family = UINT32_MAX;
    uint32_t transfer_queue_family = UINT32_MAX;
    
    // Queues
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    VkQueue transfer_queue = VK_NULL_HANDLE;
    
    // Extensions
    std::vector<const char*> enabled_extensions;
    
    // AGVSOL integration
    gpu::GPUInfo gpu_info;
    agvsol::OptimizationProfile active_profile;
};

// =============================================================================
// Render Pass Optimization
// =============================================================================

struct OptimizedRenderPassInfo {
    VkRenderPass render_pass = VK_NULL_HANDLE;
    
    // Adreno-specific: FlexRender binning hints
    bool use_binning = false;
    uint32_t bin_width = 32;
    uint32_t bin_height = 32;
    
    // Mali-specific: AFBC hints
    bool use_afbc = false;
    bool transaction_elimination = false;
    
    // PowerVR-specific: HSR hints
    bool use_hsr = true;
    bool use_tbdr = true;
    
    // Common optimizations
    bool lazy_clear = true;
    bool store_op_dont_care = false;
};

// =============================================================================
// Pipeline Optimization
// =============================================================================

struct OptimizedPipelineInfo {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    
    // Shader optimization flags
    bool use_half_precision = true;
    bool use_relaxed_precision = true;
    
    // Vertex input optimization
    bool packed_vertex_format = false;
    
    // Rasterization hints
    bool conservative_raster = false;
    
    // Fragment optimization
    bool early_fragment_tests = true;
    bool depth_clamp = false;
};

// =============================================================================
// Memory Optimization
// =============================================================================

struct MemoryOptimizationHints {
    // Buffer allocation hints
    size_t preferred_staging_size = 64 * 1024 * 1024;  // 64MB
    size_t max_vertex_buffer_size = 128 * 1024 * 1024; // 128MB
    size_t max_index_buffer_size = 64 * 1024 * 1024;   // 64MB
    
    // Texture hints
    size_t texture_cache_size = 384 * 1024 * 1024;     // 384MB
    bool prefer_device_local = true;
    bool use_lazy_allocation = true;
    
    // Adreno: UBWC (Universal Bandwidth Compression)
    bool use_ubwc = true;
    
    // Mali: AFBC for textures
    bool use_afbc_textures = true;
    
    // Memory pooling
    bool use_memory_pools = true;
    size_t pool_block_size = 16 * 1024 * 1024;  // 16MB blocks
};

// =============================================================================
// Vulkan-AGVSOL Integration Class
// =============================================================================

class VulkanAGVSOLIntegration {
public:
    static VulkanAGVSOLIntegration& Instance();
    
    // Initialization
    bool Initialize(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    
    // Device info
    const VulkanDeviceInfo& GetDeviceInfo() const { return m_device_info; }
    
    // Apply AGVSOL profile to Vulkan settings
    bool ApplyProfile(const agvsol::OptimizationProfile& profile);
    bool ApplyActiveProfile();
    
    // Optimized object creation
    VkRenderPass CreateOptimizedRenderPass(const VkRenderPassCreateInfo& create_info);
    VkPipeline CreateOptimizedGraphicsPipeline(const VkGraphicsPipelineCreateInfo& create_info,
                                                VkPipelineCache cache = VK_NULL_HANDLE);
    VkPipeline CreateOptimizedComputePipeline(const VkComputePipelineCreateInfo& create_info,
                                               VkPipelineCache cache = VK_NULL_HANDLE);
    
    // Memory optimization
    VkDeviceMemory AllocateOptimizedMemory(VkMemoryRequirements requirements,
                                            VkMemoryPropertyFlags properties);
    VkBuffer CreateOptimizedBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                    VkMemoryPropertyFlags properties);
    VkImage CreateOptimizedImage(const VkImageCreateInfo& create_info,
                                  VkMemoryPropertyFlags properties);
    
    // Get optimization hints
    const MemoryOptimizationHints& GetMemoryHints() const { return m_memory_hints; }
    OptimizedRenderPassInfo GetRenderPassHints() const;
    OptimizedPipelineInfo GetPipelineHints() const;
    
    // Vendor-specific extensions
    bool IsExtensionEnabled(const char* extension_name) const;
    std::vector<const char*> GetRecommendedExtensions() const;
    
    // Performance queries
    float GetRecommendedResolutionScale() const;
    int GetRecommendedTargetFPS() const;
    int GetRecommendedAnisotropyLevel() const;
    
    // Runtime optimization
    void BeginFrame();
    void EndFrame();
    void OptimizeForScene(uint32_t triangle_count, uint32_t draw_call_count);
    
    // Vendor-specific helpers
    bool IsAdreno() const { return m_device_info.gpu_info.is_adreno(); }
    bool IsMali() const { return m_device_info.gpu_info.is_mali(); }
    bool IsPowerVR() const { return m_device_info.gpu_info.is_powervr(); }
    
    // Statistics
    struct Stats {
        uint64_t frames_rendered = 0;
        uint64_t pipelines_created = 0;
        uint64_t render_passes_created = 0;
        uint64_t memory_allocated = 0;
        float avg_frame_time_ms = 0.0f;
        float current_fps = 0.0f;
    };
    const Stats& GetStats() const { return m_stats; }

private:
    VulkanAGVSOLIntegration() = default;
    ~VulkanAGVSOLIntegration() = default;
    
    // Initialize vendor-specific extensions
    void InitializeAdrenoExtensions();
    void InitializeMaliExtensions();
    void InitializePowerVRExtensions();
    
    // Apply vendor-specific optimizations
    void ApplyAdrenoOptimizations(const agvsol::OptimizationProfile& profile);
    void ApplyMaliOptimizations(const agvsol::OptimizationProfile& profile);
    void ApplyPowerVROptimizations(const agvsol::OptimizationProfile& profile);
    
    // Update memory hints from AGVSOL profile
    void UpdateMemoryHints();
    
    // Memory type selection
    uint32_t FindOptimalMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
    
    bool m_initialized = false;
    VulkanDeviceInfo m_device_info;
    MemoryOptimizationHints m_memory_hints;
    Stats m_stats;
    
    // Frame timing
    uint64_t m_frame_start_time = 0;
    float m_frame_time_accumulator = 0.0f;
    uint32_t m_frame_time_samples = 0;
    
    // Extension function pointers (vendor-specific)
    // Adreno
    PFN_vkVoidFunction m_pfn_adreno_set_binning_hint = nullptr;
    
    // Mali
    PFN_vkVoidFunction m_pfn_mali_afbc_hint = nullptr;
    
    // Memory pools
    std::unordered_map<uint32_t, std::vector<VkDeviceMemory>> m_memory_pools;
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Get optimal image format for the current GPU
 */
VkFormat GetOptimalColorFormat(gpu::GPUVendor vendor);

/**
 * Get optimal depth format for the current GPU
 */
VkFormat GetOptimalDepthFormat(gpu::GPUVendor vendor);

/**
 * Get optimal sample count for the current GPU tier
 */
VkSampleCountFlagBits GetOptimalSampleCount(gpu::GPUTier tier);

/**
 * Check if format supports UBWC (Adreno) or AFBC (Mali)
 */
bool SupportsCompression(VkFormat format, gpu::GPUVendor vendor);

/**
 * Get recommended present mode for target FPS
 */
VkPresentModeKHR GetOptimalPresentMode(int target_fps, bool prefer_low_latency);

} // namespace vulkan
} // namespace rpcsx
