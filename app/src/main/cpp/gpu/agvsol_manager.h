/**
 * RPCSX AGVSOL Manager - Automatic GPU Vendor-Specific Optimization Layer
 * 
 * Головний менеджер, який ініціалізується на початку, викликає детектор GPU,
 * завантажує відповідний профіль оптимізацій та активує потрібні шейдери.
 * 
 * Підтримує:
 * - Qualcomm Adreno (Snapdragon)
 * - ARM Mali (Exynos, MediaTek, Tensor)
 * - Imagination PowerVR
 * - Автоматичне визначення та застосування оптимізацій
 */

#pragma once

#include "gpu_detector.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace rpcsx {
namespace agvsol {

// ============================================================================
// Optimization Profile Structure
// ============================================================================

/**
 * Render settings for GPU-specific optimization
 */
struct RenderSettings {
    // Resolution scaling
    float resolution_scale = 1.0f;
    int target_width = 1280;
    int target_height = 720;
    
    // Frame rate
    int target_fps = 30;
    bool enable_vsync = true;
    bool enable_triple_buffer = false;
    
    // Anisotropic filtering
    int anisotropic_level = 4;  // 1, 2, 4, 8, 16
    
    // Anti-aliasing
    enum class AAMode { NONE, FXAA, SMAA, TAA, MSAA_2X, MSAA_4X } aa_mode = AAMode::FXAA;
    
    // Shadow quality
    enum class ShadowQuality { OFF, LOW, MEDIUM, HIGH, ULTRA } shadow_quality = ShadowQuality::MEDIUM;
    int shadow_map_size = 1024;
    
    // Texture settings
    enum class TextureQuality { LOW, MEDIUM, HIGH, ULTRA } texture_quality = TextureQuality::HIGH;
    bool enable_texture_compression = true;
    bool prefer_astc = true;
    bool prefer_etc2 = true;
    
    // Effects
    bool enable_bloom = true;
    bool enable_dof = false;
    bool enable_motion_blur = false;
    bool enable_ambient_occlusion = false;
    bool enable_reflections = true;
};

/**
 * Shader compilation settings
 */
struct ShaderSettings {
    // Shader optimization level
    enum class OptLevel { NONE, SIZE, PERFORMANCE, AGGRESSIVE } optimization_level = OptLevel::PERFORMANCE;
    
    // Shader precision
    bool use_half_precision = true;
    bool use_relaxed_precision = true;
    
    // Precompilation
    bool enable_precompilation = true;
    int precompile_thread_count = 2;
    
    // Cache
    bool enable_shader_cache = true;
    std::string cache_directory;
    size_t max_cache_size_mb = 256;
    
    // Vendor-specific
    bool enable_vendor_extensions = true;
    std::vector<std::string> enabled_extensions;
};

/**
 * Memory settings
 */
struct MemorySettings {
    // Buffer sizes
    size_t vertex_buffer_size_mb = 64;
    size_t index_buffer_size_mb = 32;
    size_t uniform_buffer_size_mb = 16;
    size_t staging_buffer_size_mb = 64;
    
    // Texture memory
    size_t texture_cache_size_mb = 256;
    bool enable_texture_streaming = true;
    int streaming_mip_bias = 0;
    
    // Memory allocation strategy
    enum class AllocStrategy { DEFAULT, SUBALLOCATE, DEDICATED } alloc_strategy = AllocStrategy::SUBALLOCATE;
    
    // Garbage collection
    size_t gc_threshold_mb = 512;
    int gc_interval_frames = 300;
};

/**
 * Pipeline settings
 */
struct PipelineSettings {
    // Pipeline cache
    bool enable_pipeline_cache = true;
    std::string cache_path;
    
    // Async compilation
    bool enable_async_compilation = true;
    int async_compile_threads = 2;
    
    // State optimization
    bool enable_state_sorting = true;
    bool minimize_state_changes = true;
    
    // Binning (tile-based rendering)
    bool enable_binning_optimization = false;  // Adreno-specific
    int bin_width = 32;
    int bin_height = 32;
    
    // AFBC (ARM Frame Buffer Compression)
    bool enable_afbc = false;  // Mali-specific
};

/**
 * Complete GPU optimization profile
 */
struct OptimizationProfile {
    std::string name;
    std::string description;
    gpu::GPUVendor target_vendor = gpu::GPUVendor::UNKNOWN;
    gpu::GPUTier target_tier = gpu::GPUTier::UNKNOWN;
    
    RenderSettings render;
    ShaderSettings shader;
    MemorySettings memory;
    PipelineSettings pipeline;
    
    // Vendor-specific flags
    std::unordered_map<std::string, bool> vendor_flags;
    std::unordered_map<std::string, int> vendor_ints;
    std::unordered_map<std::string, float> vendor_floats;
    std::unordered_map<std::string, std::string> vendor_strings;
    
    // Profile metadata
    int version = 1;
    std::string author;
    std::string last_updated;
};

// ============================================================================
// Shader Variant System
// ============================================================================

/**
 * Shader variant for specific GPU
 */
struct ShaderVariant {
    std::string name;
    std::string vertex_path;
    std::string fragment_path;
    std::string compute_path;
    gpu::GPUVendor target_vendor;
    std::vector<std::string> defines;
    std::vector<uint32_t> spirv_vertex;
    std::vector<uint32_t> spirv_fragment;
    std::vector<uint32_t> spirv_compute;
    bool is_loaded = false;
};

/**
 * Shader variant set (all variants for one logical shader)
 */
struct ShaderVariantSet {
    std::string name;
    std::string default_variant;
    std::unordered_map<gpu::GPUVendor, ShaderVariant> variants;
    
    const ShaderVariant* GetVariant(gpu::GPUVendor vendor) const {
        auto it = variants.find(vendor);
        if (it != variants.end()) {
            return &it->second;
        }
        // Fallback to unknown/default
        it = variants.find(gpu::GPUVendor::UNKNOWN);
        return (it != variants.end()) ? &it->second : nullptr;
    }
};

// ============================================================================
// AGVSOL Manager Configuration
// ============================================================================

struct AGVSOLConfig {
    // Paths
    std::string profiles_directory;
    std::string shaders_directory;
    std::string cache_directory;
    
    // Behavior
    bool auto_detect_gpu = true;
    bool auto_apply_profile = true;
    bool allow_manual_override = true;
    
    // Logging
    bool verbose_logging = false;
    bool log_shader_compilation = false;
    bool log_profile_loading = true;
};

// ============================================================================
// AGVSOL Manager API
// ============================================================================

/**
 * Initialize AGVSOL manager
 * @param config Configuration options
 * @return true if initialization succeeded
 */
bool InitializeAGVSOL(const AGVSOLConfig& config);

/**
 * Initialize with default configuration
 * @param cache_dir Cache directory path
 * @return true if initialization succeeded
 */
bool InitializeAGVSOL(const std::string& cache_dir);

/**
 * Shutdown AGVSOL manager
 */
void ShutdownAGVSOL();

/**
 * Check if AGVSOL is initialized
 */
bool IsAGVSOLInitialized();

/**
 * Get current GPU info (from detector)
 */
const gpu::GPUInfo& GetGPUInfo();

/**
 * Get currently active optimization profile
 */
const OptimizationProfile& GetActiveProfile();

/**
 * Load optimization profile for specific vendor
 * @param vendor GPU vendor to load profile for
 * @return true if profile was loaded
 */
bool LoadProfileForVendor(gpu::GPUVendor vendor);

/**
 * Load optimization profile from file
 * @param path Path to JSON profile file
 * @return true if profile was loaded
 */
bool LoadProfileFromFile(const std::string& path);

/**
 * Apply currently loaded profile
 * @return true if profile was applied successfully
 */
bool ApplyActiveProfile();

/**
 * Get shader variant for current GPU
 * @param shader_name Name of shader set
 * @return Pointer to shader variant or nullptr
 */
const ShaderVariant* GetShaderVariant(const std::string& shader_name);

/**
 * Load all shader variants for current GPU
 * @return Number of shaders loaded
 */
int LoadShaderVariants();

/**
 * Get render settings from active profile
 */
const RenderSettings& GetRenderSettings();

/**
 * Get shader settings from active profile
 */
const ShaderSettings& GetShaderSettings();

/**
 * Get memory settings from active profile
 */
const MemorySettings& GetMemorySettings();

/**
 * Get pipeline settings from active profile
 */
const PipelineSettings& GetPipelineSettings();

/**
 * Override specific render setting
 */
void SetRenderSetting(const std::string& key, float value);
void SetRenderSetting(const std::string& key, int value);
void SetRenderSetting(const std::string& key, bool value);

/**
 * Get vendor-specific flag from profile
 */
bool GetVendorFlag(const std::string& key, bool default_value = false);
int GetVendorInt(const std::string& key, int default_value = 0);
float GetVendorFloat(const std::string& key, float default_value = 0.0f);
std::string GetVendorString(const std::string& key, const std::string& default_value = "");

/**
 * Register callback for profile changes
 */
using ProfileChangeCallback = std::function<void(const OptimizationProfile&)>;
void RegisterProfileChangeCallback(ProfileChangeCallback callback);

/**
 * Get list of available profiles
 */
std::vector<std::string> GetAvailableProfiles();

/**
 * Get statistics
 */
struct AGVSOLStats {
    gpu::GPUVendor detected_vendor;
    gpu::GPUTier detected_tier;
    std::string active_profile_name;
    int shaders_loaded;
    int profiles_available;
    bool is_optimized;
};
AGVSOLStats GetStats();

/**
 * Export current profile to JSON string
 */
std::string ExportProfileToJSON();

/**
 * Import profile from JSON string
 */
bool ImportProfileFromJSON(const std::string& json);

// ============================================================================
// Built-in Profile Generators
// ============================================================================

/**
 * Generate default Adreno profile
 */
OptimizationProfile GenerateAdrenoProfile(gpu::GPUTier tier);

/**
 * Generate default Mali profile
 */
OptimizationProfile GenerateMaliProfile(gpu::GPUTier tier);

/**
 * Generate default PowerVR profile
 */
OptimizationProfile GeneratePowerVRProfile(gpu::GPUTier tier);

/**
 * Generate generic profile for unknown GPU
 */
OptimizationProfile GenerateGenericProfile(gpu::GPUTier tier);

} // namespace agvsol
} // namespace rpcsx
