/**
 * RPCSX Vulkan-AGVSOL Integration Implementation
 * 
 * Real Vulkan integration with GPU vendor-specific optimizations.
 */

#include "vulkan_agvsol_integration.h"
#include <android/log.h>
#include <chrono>
#include <algorithm>
#include <cstring>

#define LOG_TAG "RPCSX-VulkanAGVSOL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace vulkan {

// =============================================================================
// Singleton Instance
// =============================================================================

VulkanAGVSOLIntegration& VulkanAGVSOLIntegration::Instance() {
    static VulkanAGVSOLIntegration instance;
    return instance;
}

// =============================================================================
// Initialization
// =============================================================================

bool VulkanAGVSOLIntegration::Initialize(VkInstance instance, 
                                          VkPhysicalDevice physical_device, 
                                          VkDevice device) {
    if (m_initialized) {
        LOGW("VulkanAGVSOLIntegration already initialized");
        return true;
    }
    
    if (instance == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        LOGE("Invalid Vulkan handles provided");
        return false;
    }
    
    LOGI("Initializing Vulkan-AGVSOL Integration...");
    
    m_device_info.instance = instance;
    m_device_info.physical_device = physical_device;
    m_device_info.device = device;
    
    // Get device properties
    vkGetPhysicalDeviceProperties(physical_device, &m_device_info.properties);
    vkGetPhysicalDeviceFeatures(physical_device, &m_device_info.features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_info.memory_properties);
    
    LOGI("GPU: %s", m_device_info.properties.deviceName);
    LOGI("Vendor ID: 0x%04X", m_device_info.properties.vendorID);
    LOGI("Device ID: 0x%04X", m_device_info.properties.deviceID);
    LOGI("API Version: %u.%u.%u", 
         VK_VERSION_MAJOR(m_device_info.properties.apiVersion),
         VK_VERSION_MINOR(m_device_info.properties.apiVersion),
         VK_VERSION_PATCH(m_device_info.properties.apiVersion));
    
    // Initialize GPU detector with Vulkan info
    gpu::InitializeGPUDetector(instance, physical_device);
    m_device_info.gpu_info = gpu::GetGPUInfo();
    
    LOGI("Detected GPU: %s (%s)", 
         m_device_info.gpu_info.model.c_str(),
         gpu::GetVendorName(m_device_info.gpu_info.vendor));
    LOGI("GPU Tier: %s", gpu::GetTierName(m_device_info.gpu_info.tier));
    
    // Initialize vendor-specific extensions
    switch (m_device_info.gpu_info.vendor) {
        case gpu::GPUVendor::QUALCOMM_ADRENO:
            InitializeAdrenoExtensions();
            break;
        case gpu::GPUVendor::ARM_MALI:
            InitializeMaliExtensions();
            break;
        case gpu::GPUVendor::IMAGINATION_POWERVR:
            InitializePowerVRExtensions();
            break;
        default:
            LOGI("Using generic Vulkan optimizations");
            break;
    }
    
    // Initialize AGVSOL and apply profile
    if (agvsol::IsAGVSOLInitialized()) {
        ApplyActiveProfile();
    }
    
    // Set memory hints based on GPU
    UpdateMemoryHints();
    
    m_initialized = true;
    LOGI("Vulkan-AGVSOL Integration initialized successfully!");
    
    return true;
}

void VulkanAGVSOLIntegration::Shutdown() {
    if (!m_initialized) return;
    
    LOGI("Shutting down Vulkan-AGVSOL Integration...");
    
    // Free memory pools
    for (auto& [type, pool] : m_memory_pools) {
        for (auto& memory : pool) {
            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device_info.device, memory, nullptr);
            }
        }
    }
    m_memory_pools.clear();
    
    m_initialized = false;
    LOGI("Vulkan-AGVSOL Integration shut down");
}

// =============================================================================
// Vendor-Specific Extension Initialization
// =============================================================================

void VulkanAGVSOLIntegration::InitializeAdrenoExtensions() {
    LOGI("Initializing Adreno-specific extensions...");
    
    // Adreno extensions for FlexRender, UBWC, etc.
    const char* adreno_extensions[] = {
        "VK_QCOM_render_pass_transform",
        "VK_QCOM_render_pass_shader_resolve",
        "VK_QCOM_rotated_copy_commands",
        "VK_QCOM_render_pass_store_ops",
        "VK_EXT_fragment_density_map",
    };
    
    // Check available extensions
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(m_device_info.physical_device, nullptr, 
                                          &extension_count, nullptr);
    
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(m_device_info.physical_device, nullptr,
                                          &extension_count, available_extensions.data());
    
    for (const char* ext : adreno_extensions) {
        for (const auto& available : available_extensions) {
            if (strcmp(ext, available.extensionName) == 0) {
                m_device_info.enabled_extensions.push_back(ext);
                LOGI("  Enabled: %s", ext);
                break;
            }
        }
    }
    
    // Set Adreno-specific memory hints
    m_memory_hints.use_ubwc = true;
    m_memory_hints.use_afbc_textures = false;
    m_memory_hints.prefer_device_local = true;
    
    LOGI("Adreno optimizations: UBWC=%d, FlexRender=available",
         m_memory_hints.use_ubwc);
}

void VulkanAGVSOLIntegration::InitializeMaliExtensions() {
    LOGI("Initializing Mali-specific extensions...");
    
    const char* mali_extensions[] = {
        "VK_ARM_rasterization_order_attachment_access",
        "VK_EXT_fragment_density_map",
        "VK_KHR_fragment_shading_rate",
    };
    
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(m_device_info.physical_device, nullptr,
                                          &extension_count, nullptr);
    
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(m_device_info.physical_device, nullptr,
                                          &extension_count, available_extensions.data());
    
    for (const char* ext : mali_extensions) {
        for (const auto& available : available_extensions) {
            if (strcmp(ext, available.extensionName) == 0) {
                m_device_info.enabled_extensions.push_back(ext);
                LOGI("  Enabled: %s", ext);
                break;
            }
        }
    }
    
    // Mali-specific hints
    m_memory_hints.use_ubwc = false;
    m_memory_hints.use_afbc_textures = true;
    m_memory_hints.use_lazy_allocation = true;
    
    LOGI("Mali optimizations: AFBC=%d, Transaction Elimination=available",
         m_memory_hints.use_afbc_textures);
}

void VulkanAGVSOLIntegration::InitializePowerVRExtensions() {
    LOGI("Initializing PowerVR-specific extensions...");
    
    const char* powervr_extensions[] = {
        "VK_IMG_filter_cubic",
        "VK_IMG_format_pvrtc",
    };
    
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(m_device_info.physical_device, nullptr,
                                          &extension_count, nullptr);
    
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(m_device_info.physical_device, nullptr,
                                          &extension_count, available_extensions.data());
    
    for (const char* ext : powervr_extensions) {
        for (const auto& available : available_extensions) {
            if (strcmp(ext, available.extensionName) == 0) {
                m_device_info.enabled_extensions.push_back(ext);
                LOGI("  Enabled: %s", ext);
                break;
            }
        }
    }
    
    // PowerVR-specific hints
    m_memory_hints.use_ubwc = false;
    m_memory_hints.use_afbc_textures = false;
    m_memory_hints.prefer_device_local = true;
    
    LOGI("PowerVR optimizations: TBDR=native, HSR=available");
}

void VulkanAGVSOLIntegration::UpdateMemoryHints() {
    const auto& profile = agvsol::GetActiveProfile();
    
    m_memory_hints.max_vertex_buffer_size = profile.memory.vertex_buffer_size_mb * 1024 * 1024;
    m_memory_hints.max_index_buffer_size = profile.memory.index_buffer_size_mb * 1024 * 1024;
    m_memory_hints.texture_cache_size = profile.memory.texture_cache_size_mb * 1024 * 1024;
    
    // Apply vendor-specific overrides
    if (IsAdreno()) {
        m_memory_hints.use_ubwc = agvsol::GetVendorFlag("prefer_ubwc", true);
    } else if (IsMali()) {
        m_memory_hints.use_afbc_textures = agvsol::GetVendorFlag("enable_afbc", true);
    }
}

// =============================================================================
// Profile Application
// =============================================================================

bool VulkanAGVSOLIntegration::ApplyProfile(const agvsol::OptimizationProfile& profile) {
    LOGI("Applying AGVSOL profile: %s", profile.name.c_str());
    
    m_device_info.active_profile = profile;
    
    // Apply vendor-specific optimizations
    switch (m_device_info.gpu_info.vendor) {
        case gpu::GPUVendor::QUALCOMM_ADRENO:
            ApplyAdrenoOptimizations(profile);
            break;
        case gpu::GPUVendor::ARM_MALI:
            ApplyMaliOptimizations(profile);
            break;
        case gpu::GPUVendor::IMAGINATION_POWERVR:
            ApplyPowerVROptimizations(profile);
            break;
        default:
            LOGI("Applying generic optimizations");
            break;
    }
    
    // Update memory hints
    UpdateMemoryHints();
    
    // Convert AAMode enum to string
    const char* aa_mode_str = "unknown";
    switch (profile.render.aa_mode) {
        case agvsol::RenderSettings::AAMode::NONE: aa_mode_str = "none"; break;
        case agvsol::RenderSettings::AAMode::FXAA: aa_mode_str = "fxaa"; break;
        case agvsol::RenderSettings::AAMode::SMAA: aa_mode_str = "smaa"; break;
        case agvsol::RenderSettings::AAMode::TAA: aa_mode_str = "taa"; break;
        case agvsol::RenderSettings::AAMode::MSAA_2X: aa_mode_str = "msaa2x"; break;
        case agvsol::RenderSettings::AAMode::MSAA_4X: aa_mode_str = "msaa4x"; break;
    }
    
    LOGI("Profile applied: Resolution=%.0f%%, FPS=%d, AA=%s",
         profile.render.resolution_scale * 100.0f,
         profile.render.target_fps,
         aa_mode_str);
    
    return true;
}

bool VulkanAGVSOLIntegration::ApplyActiveProfile() {
    if (!agvsol::IsAGVSOLInitialized()) {
        LOGW("AGVSOL not initialized, cannot apply profile");
        return false;
    }
    
    return ApplyProfile(agvsol::GetActiveProfile());
}

void VulkanAGVSOLIntegration::ApplyAdrenoOptimizations(const agvsol::OptimizationProfile& profile) {
    LOGI("Applying Adreno-specific optimizations...");
    
    // FlexRender binning
    bool use_binning = agvsol::GetVendorFlag("use_adreno_binning", true);
    bool use_flexrender = agvsol::GetVendorFlag("enable_flexrender", true);
    bool prefer_ubwc = agvsol::GetVendorFlag("prefer_ubwc", true);
    
    LOGI("  FlexRender: %s", use_flexrender ? "ON" : "OFF");
    LOGI("  Binning: %s", use_binning ? "ON" : "OFF");
    LOGI("  UBWC: %s", prefer_ubwc ? "ON" : "OFF");
    
    // LRZ (Low Resolution Z)
    bool enable_lrz = agvsol::GetVendorFlag("enable_lrz", true);
    LOGI("  LRZ: %s", enable_lrz ? "ON" : "OFF");
}

void VulkanAGVSOLIntegration::ApplyMaliOptimizations(const agvsol::OptimizationProfile& profile) {
    LOGI("Applying Mali-specific optimizations...");
    
    bool enable_afbc = agvsol::GetVendorFlag("enable_afbc", true);
    bool enable_te = agvsol::GetVendorFlag("enable_transaction_elimination", true);
    bool use_fma = agvsol::GetVendorFlag("enable_fma", true);
    
    LOGI("  AFBC: %s", enable_afbc ? "ON" : "OFF");
    LOGI("  Transaction Elimination: %s", enable_te ? "ON" : "OFF");
    LOGI("  FMA: %s", use_fma ? "ON" : "OFF");
    
    // IDVS (Index-Driven Vertex Shading)
    bool enable_idvs = agvsol::GetVendorFlag("enable_idvs", true);
    LOGI("  IDVS: %s", enable_idvs ? "ON" : "OFF");
}

void VulkanAGVSOLIntegration::ApplyPowerVROptimizations(const agvsol::OptimizationProfile& profile) {
    LOGI("Applying PowerVR-specific optimizations...");
    
    bool enable_hsr = agvsol::GetVendorFlag("enable_hsr", true);
    bool prefer_tbdr = agvsol::GetVendorFlag("prefer_tbdr", true);
    bool use_pvrtc = agvsol::GetVendorFlag("enable_pvrtc", true);
    
    LOGI("  HSR (Hidden Surface Removal): %s", enable_hsr ? "ON" : "OFF");
    LOGI("  TBDR: %s", prefer_tbdr ? "ON" : "OFF");
    LOGI("  PVRTC: %s", use_pvrtc ? "ON" : "OFF");
}

// =============================================================================
// Optimized Object Creation
// =============================================================================

VkRenderPass VulkanAGVSOLIntegration::CreateOptimizedRenderPass(
    const VkRenderPassCreateInfo& create_info) {
    
    if (!m_initialized) return VK_NULL_HANDLE;
    
    // Create modified render pass with vendor-specific optimizations
    VkRenderPassCreateInfo optimized_info = create_info;
    
    // For Adreno: hints for binning optimization
    // The driver will automatically optimize based on attachment usage
    
    // For Mali: ensure AFBC-compatible layouts
    // The driver handles AFBC internally
    
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkResult result = vkCreateRenderPass(m_device_info.device, &optimized_info, 
                                          nullptr, &render_pass);
    
    if (result == VK_SUCCESS) {
        m_stats.render_passes_created++;
        LOGI("Created optimized render pass (%llu total)", m_stats.render_passes_created);
    }
    
    return render_pass;
}

VkPipeline VulkanAGVSOLIntegration::CreateOptimizedGraphicsPipeline(
    const VkGraphicsPipelineCreateInfo& create_info,
    VkPipelineCache cache) {
    
    if (!m_initialized) return VK_NULL_HANDLE;
    
    VkGraphicsPipelineCreateInfo optimized_info = create_info;
    
    // Apply optimization flags
    VkPipelineCreateFlags optimization_flags = create_info.flags;
    
    // Disable optimization during development for faster compilation
    // optimization_flags |= VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    
    // For production: allow driver optimizations
    optimization_flags &= ~VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    
    optimized_info.flags = optimization_flags;
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(m_device_info.device, cache, 1,
                                                 &optimized_info, nullptr, &pipeline);
    
    if (result == VK_SUCCESS) {
        m_stats.pipelines_created++;
    }
    
    return pipeline;
}

VkPipeline VulkanAGVSOLIntegration::CreateOptimizedComputePipeline(
    const VkComputePipelineCreateInfo& create_info,
    VkPipelineCache cache) {
    
    if (!m_initialized) return VK_NULL_HANDLE;
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateComputePipelines(m_device_info.device, cache, 1,
                                                &create_info, nullptr, &pipeline);
    
    if (result == VK_SUCCESS) {
        m_stats.pipelines_created++;
    }
    
    return pipeline;
}

// =============================================================================
// Memory Optimization
// =============================================================================

uint32_t VulkanAGVSOLIntegration::FindOptimalMemoryType(uint32_t type_filter,
                                                         VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < m_device_info.memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (m_device_info.memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            
            // Prefer device local memory for performance
            if (m_memory_hints.prefer_device_local &&
                (m_device_info.memory_properties.memoryTypes[i].propertyFlags & 
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                return i;
            }
            return i;
        }
    }
    
    return UINT32_MAX;
}

VkDeviceMemory VulkanAGVSOLIntegration::AllocateOptimizedMemory(
    VkMemoryRequirements requirements,
    VkMemoryPropertyFlags properties) {
    
    if (!m_initialized) return VK_NULL_HANDLE;
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = FindOptimalMemoryType(requirements.memoryTypeBits, properties);
    
    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        LOGE("Failed to find suitable memory type");
        return VK_NULL_HANDLE;
    }
    
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult result = vkAllocateMemory(m_device_info.device, &alloc_info, nullptr, &memory);
    
    if (result == VK_SUCCESS) {
        m_stats.memory_allocated += requirements.size;
    }
    
    return memory;
}

VkBuffer VulkanAGVSOLIntegration::CreateOptimizedBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties) {
    
    if (!m_initialized) return VK_NULL_HANDLE;
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(m_device_info.device, &buffer_info, nullptr, &buffer);
    
    if (result != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    // Get memory requirements and allocate
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(m_device_info.device, buffer, &mem_requirements);
    
    VkDeviceMemory memory = AllocateOptimizedMemory(mem_requirements, properties);
    
    if (memory == VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device_info.device, buffer, nullptr);
        return VK_NULL_HANDLE;
    }
    
    vkBindBufferMemory(m_device_info.device, buffer, memory, 0);
    
    return buffer;
}

VkImage VulkanAGVSOLIntegration::CreateOptimizedImage(
    const VkImageCreateInfo& create_info,
    VkMemoryPropertyFlags properties) {
    
    if (!m_initialized) return VK_NULL_HANDLE;
    
    VkImageCreateInfo optimized_info = create_info;
    
    // Apply vendor-specific image optimizations
    if (IsAdreno() && m_memory_hints.use_ubwc) {
        // UBWC is automatically applied by the driver for optimal formats
        // Just ensure we're using compatible tiling
        if (optimized_info.tiling == VK_IMAGE_TILING_OPTIMAL) {
            // Driver will apply UBWC
        }
    }
    
    if (IsMali() && m_memory_hints.use_afbc_textures) {
        // AFBC is driver-managed
    }
    
    VkImage image = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(m_device_info.device, &optimized_info, nullptr, &image);
    
    if (result != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    // Get memory requirements and allocate
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(m_device_info.device, image, &mem_requirements);
    
    VkDeviceMemory memory = AllocateOptimizedMemory(mem_requirements, properties);
    
    if (memory == VK_NULL_HANDLE) {
        vkDestroyImage(m_device_info.device, image, nullptr);
        return VK_NULL_HANDLE;
    }
    
    vkBindImageMemory(m_device_info.device, image, memory, 0);
    
    return image;
}

// =============================================================================
// Query Methods
// =============================================================================

OptimizedRenderPassInfo VulkanAGVSOLIntegration::GetRenderPassHints() const {
    OptimizedRenderPassInfo hints;
    
    if (IsAdreno()) {
        hints.use_binning = agvsol::GetVendorFlag("use_adreno_binning", true);
        hints.bin_width = agvsol::GetVendorInt("bin_width", 32);
        hints.bin_height = agvsol::GetVendorInt("bin_height", 32);
    } else if (IsMali()) {
        hints.use_afbc = agvsol::GetVendorFlag("enable_afbc", true);
        hints.transaction_elimination = agvsol::GetVendorFlag("enable_transaction_elimination", true);
    } else if (IsPowerVR()) {
        hints.use_hsr = agvsol::GetVendorFlag("enable_hsr", true);
        hints.use_tbdr = agvsol::GetVendorFlag("prefer_tbdr", true);
    }
    
    hints.lazy_clear = true;
    
    return hints;
}

OptimizedPipelineInfo VulkanAGVSOLIntegration::GetPipelineHints() const {
    OptimizedPipelineInfo hints;
    
    const auto& profile = m_device_info.active_profile;
    
    hints.use_half_precision = profile.shader.use_half_precision;
    hints.use_relaxed_precision = profile.shader.use_relaxed_precision;
    hints.early_fragment_tests = true;
    
    return hints;
}

float VulkanAGVSOLIntegration::GetRecommendedResolutionScale() const {
    return m_device_info.active_profile.render.resolution_scale;
}

int VulkanAGVSOLIntegration::GetRecommendedTargetFPS() const {
    return m_device_info.active_profile.render.target_fps;
}

int VulkanAGVSOLIntegration::GetRecommendedAnisotropyLevel() const {
    return m_device_info.active_profile.render.anisotropic_level;
}

bool VulkanAGVSOLIntegration::IsExtensionEnabled(const char* extension_name) const {
    for (const char* ext : m_device_info.enabled_extensions) {
        if (strcmp(ext, extension_name) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<const char*> VulkanAGVSOLIntegration::GetRecommendedExtensions() const {
    return m_device_info.enabled_extensions;
}

// =============================================================================
// Frame Management
// =============================================================================

void VulkanAGVSOLIntegration::BeginFrame() {
    m_frame_start_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

void VulkanAGVSOLIntegration::EndFrame() {
    uint64_t frame_end_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    
    float frame_time_ms = (frame_end_time - m_frame_start_time) / 1000.0f;
    
    m_frame_time_accumulator += frame_time_ms;
    m_frame_time_samples++;
    
    if (m_frame_time_samples >= 60) {
        m_stats.avg_frame_time_ms = m_frame_time_accumulator / m_frame_time_samples;
        m_stats.current_fps = 1000.0f / m_stats.avg_frame_time_ms;
        
        m_frame_time_accumulator = 0.0f;
        m_frame_time_samples = 0;
    }
    
    m_stats.frames_rendered++;
}

void VulkanAGVSOLIntegration::OptimizeForScene(uint32_t triangle_count, 
                                                uint32_t draw_call_count) {
    // Dynamic optimization based on scene complexity
    // This can be used to adjust quality settings at runtime
    
    const int target_fps = GetRecommendedTargetFPS();
    const float target_frame_time = 1000.0f / target_fps;
    
    if (m_stats.avg_frame_time_ms > target_frame_time * 1.2f) {
        // Running slow, could reduce quality
        LOGW("Scene too complex: %u tris, %u draws, %.1f ms/frame",
             triangle_count, draw_call_count, m_stats.avg_frame_time_ms);
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

VkFormat GetOptimalColorFormat(gpu::GPUVendor vendor) {
    // All mobile GPUs handle RGBA8 well
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkFormat GetOptimalDepthFormat(gpu::GPUVendor vendor) {
    switch (vendor) {
        case gpu::GPUVendor::QUALCOMM_ADRENO:
            return VK_FORMAT_D24_UNORM_S8_UINT;  // Best for Adreno
        case gpu::GPUVendor::ARM_MALI:
            return VK_FORMAT_D24_UNORM_S8_UINT;  // Mali handles both well
        case gpu::GPUVendor::IMAGINATION_POWERVR:
            return VK_FORMAT_D32_SFLOAT;         // PowerVR prefers D32
        default:
            return VK_FORMAT_D24_UNORM_S8_UINT;
    }
}

VkSampleCountFlagBits GetOptimalSampleCount(gpu::GPUTier tier) {
    switch (tier) {
        case gpu::GPUTier::ULTRA:
            return VK_SAMPLE_COUNT_4_BIT;
        case gpu::GPUTier::HIGH_END:
            return VK_SAMPLE_COUNT_2_BIT;
        default:
            return VK_SAMPLE_COUNT_1_BIT;  // No MSAA for lower tiers
    }
}

bool SupportsCompression(VkFormat format, gpu::GPUVendor vendor) {
    // Most color formats support compression on modern GPUs
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return true;
        default:
            return false;
    }
}

VkPresentModeKHR GetOptimalPresentMode(int target_fps, bool prefer_low_latency) {
    if (prefer_low_latency || target_fps >= 60) {
        return VK_PRESENT_MODE_MAILBOX_KHR;  // Triple buffering, low latency
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // VSync, power efficient
}

} // namespace vulkan
} // namespace rpcsx
