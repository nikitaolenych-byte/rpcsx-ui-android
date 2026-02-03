/**
 * RPCSX GPU Detector
 * 
 * Визначає виробника та модель GPU через системні властивості Android
 * або Vulkan API для застосування vendor-specific оптимізацій.
 * 
 * Підтримувані GPU:
 * - Qualcomm Adreno (Snapdragon)
 * - ARM Mali (Exynos, MediaTek, Kirin)
 * - Imagination PowerVR (Apple A-series legacy, MediaTek older)
 * - Intel (x86 Android devices)
 * - NVIDIA (Shield devices)
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace rpcsx {
namespace gpu {

// ============================================================================
// GPU Vendor Enumeration
// ============================================================================

enum class GPUVendor : uint32_t {
    UNKNOWN = 0,
    QUALCOMM_ADRENO,    // Snapdragon devices
    ARM_MALI,           // Samsung Exynos, MediaTek Dimensity, Kirin
    IMAGINATION_POWERVR, // Older MediaTek, Apple (legacy)
    INTEL,              // x86 Android devices
    NVIDIA,             // Shield devices
    AMD,                // Rare Android devices
    
    COUNT
};

// ============================================================================
// GPU Generation/Tier
// ============================================================================

enum class GPUTier : uint32_t {
    UNKNOWN = 0,
    LOW_END,        // Entry-level, <30 FPS target
    MID_RANGE,      // Mid-tier, 30 FPS target
    HIGH_END,       // Flagship, 60 FPS target
    ULTRA,          // Latest flagship, 60+ FPS target
    
    COUNT
};

// ============================================================================
// GPU Architecture Features
// ============================================================================

struct GPUFeatures {
    // Vulkan features
    bool vulkan_1_0 = false;
    bool vulkan_1_1 = false;
    bool vulkan_1_2 = false;
    bool vulkan_1_3 = false;
    
    // Compute features
    bool compute_shaders = false;
    bool storage_buffer_16bit = false;
    bool shader_float16 = false;
    bool shader_int8 = false;
    bool shader_int16 = false;
    
    // Memory features
    bool device_local_memory = false;
    bool host_visible_coherent = false;
    bool lazy_allocation = false;
    uint64_t max_memory_allocation = 0;
    uint64_t heap_budget = 0;
    
    // Texture features
    bool astc_ldr = false;
    bool astc_hdr = false;
    bool etc2 = false;
    bool bc_compression = false;
    uint32_t max_texture_size = 4096;
    uint32_t max_texture_array_layers = 256;
    
    // Rendering features
    bool multiview = false;
    bool draw_indirect = false;
    bool multi_draw_indirect = false;
    bool tessellation = false;
    bool geometry_shaders = false;
    bool fragment_shading_rate = false;
    
    // Vendor-specific extensions
    bool adreno_binning = false;         // Qualcomm tile-based binning
    bool mali_afbc = false;              // ARM Frame Buffer Compression
    bool powervr_pvrtc = false;          // PowerVR Texture Compression
    bool nvidia_cuda_interop = false;    // NVIDIA CUDA
};

// ============================================================================
// GPU Information Structure
// ============================================================================

struct GPUInfo {
    GPUVendor vendor = GPUVendor::UNKNOWN;
    GPUTier tier = GPUTier::UNKNOWN;
    GPUFeatures features;
    
    // Identification
    std::string vendor_name;        // "Qualcomm", "ARM", "Imagination", etc.
    std::string renderer_name;      // "Adreno (TM) 740", "Mali-G715", etc.
    std::string model;              // "Adreno 740", "Mali-G715 MC11", etc.
    std::string driver_version;     // Driver version string
    uint32_t vendor_id = 0;         // Vulkan vendor ID
    uint32_t device_id = 0;         // Vulkan device ID
    
    // Architecture info
    std::string architecture;       // "A7x", "Valhall", "Series6XT", etc.
    int generation = 0;             // GPU generation number
    int compute_units = 0;          // Number of shader cores/CUs
    int max_clock_mhz = 0;          // Max GPU clock in MHz
    
    // Performance estimates
    float estimated_tflops = 0.0f;  // Estimated TFLOPS
    int recommended_resolution_x = 1280;
    int recommended_resolution_y = 720;
    int recommended_fps = 30;
    
    // System info
    std::string soc_name;           // "Snapdragon 8s Gen 3", "Dimensity 9200", etc.
    std::string board_platform;     // ro.board.platform value
    std::string hardware;           // ro.hardware value
    
    // Validation
    bool is_valid() const { return vendor != GPUVendor::UNKNOWN; }
    bool is_adreno() const { return vendor == GPUVendor::QUALCOMM_ADRENO; }
    bool is_mali() const { return vendor == GPUVendor::ARM_MALI; }
    bool is_powervr() const { return vendor == GPUVendor::IMAGINATION_POWERVR; }
};

// ============================================================================
// Adreno-Specific Models
// ============================================================================

enum class AdrenoModel : uint32_t {
    UNKNOWN = 0,
    
    // Adreno 6xx series (Snapdragon 8xx, 7xx)
    ADRENO_610,     // SD 665, 662
    ADRENO_612,     // SD 460
    ADRENO_615,     // SD 675
    ADRENO_616,     // SD 710
    ADRENO_618,     // SD 720G
    ADRENO_619,     // SD 680, 695
    ADRENO_620,     // SD 765G
    ADRENO_630,     // SD 845
    ADRENO_640,     // SD 855
    ADRENO_642L,    // SD 778G
    ADRENO_650,     // SD 865
    ADRENO_660,     // SD 888
    
    // Adreno 7xx series (Snapdragon 8 Gen 1+)
    ADRENO_710,     // SD 7 Gen 1
    ADRENO_720,     // SD 7+ Gen 2
    ADRENO_730,     // SD 8 Gen 1
    ADRENO_740,     // SD 8 Gen 2
    ADRENO_750,     // SD 8s Gen 3
    ADRENO_760,     // SD 8 Gen 3
    ADRENO_830,     // SD 8 Elite (future)
};

// ============================================================================
// Mali-Specific Models
// ============================================================================

enum class MaliModel : uint32_t {
    UNKNOWN = 0,
    
    // Bifrost architecture (older)
    MALI_G51,
    MALI_G52,
    MALI_G71,
    MALI_G72,
    MALI_G76,
    MALI_G77,
    MALI_G78,
    
    // Valhall architecture (current)
    MALI_G310,
    MALI_G510,
    MALI_G610,
    MALI_G710,
    MALI_G715,
    MALI_G720,
    MALI_G725,
    
    // Immortalis (high-end Valhall)
    MALI_IMMORTALIS_G715,
    MALI_IMMORTALIS_G720,
    MALI_IMMORTALIS_G925,
};

// ============================================================================
// GPU Detection Functions
// ============================================================================

/**
 * Detect GPU vendor from Vulkan or system properties
 * @return GPU vendor enum
 */
GPUVendor DetectGPUVendor();

/**
 * Get full GPU model string
 * @return Model string like "Adreno 740" or "Mali-G715 MC11"
 */
std::string GetGPUModel();

/**
 * Get complete GPU information
 * @return GPUInfo structure with all details
 */
GPUInfo GetGPUInfo();

/**
 * Get vendor name as string
 */
const char* GetVendorName(GPUVendor vendor);

/**
 * Get tier name as string
 */
const char* GetTierName(GPUTier tier);

/**
 * Detect Adreno model from renderer string
 */
AdrenoModel DetectAdrenoModel(const std::string& renderer);

/**
 * Detect Mali model from renderer string
 */
MaliModel DetectMaliModel(const std::string& renderer);

/**
 * Get GPU tier based on model
 */
GPUTier DetermineGPUTier(const GPUInfo& info);

/**
 * Initialize GPU detector with Vulkan instance (optional)
 * @param vk_instance Vulkan instance handle (can be nullptr)
 * @param vk_physical_device Physical device handle (can be nullptr)
 * @return true if detection succeeded
 */
bool InitializeGPUDetector(void* vk_instance = nullptr, void* vk_physical_device = nullptr);

/**
 * Shutdown GPU detector
 */
void ShutdownGPUDetector();

/**
 * Check if GPU detector is initialized
 */
bool IsGPUDetectorInitialized();

/**
 * Force re-detection of GPU
 */
void RefreshGPUInfo();

/**
 * Get cached GPU info (faster than GetGPUInfo)
 */
const GPUInfo& GetCachedGPUInfo();

/**
 * Register callback for GPU detection events
 */
using GPUDetectionCallback = std::function<void(const GPUInfo&)>;
void RegisterGPUDetectionCallback(GPUDetectionCallback callback);

} // namespace gpu
} // namespace rpcsx
