/**
 * RPCSX AGVSOL Manager Implementation
 * 
 * Автоматичне визначення GPU та застосування vendor-specific оптимізацій
 */

#include "agvsol_manager.h"

#include <android/log.h>
#include <fstream>
#include <sstream>
#include <mutex>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#define LOG_TAG "RPCSX-AGVSOL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace agvsol {

// ============================================================================
// Global State
// ============================================================================

static std::mutex g_mutex;
static bool g_initialized = false;
static AGVSOLConfig g_config;
static OptimizationProfile g_active_profile;
static std::vector<ShaderVariantSet> g_shader_sets;
static std::vector<ProfileChangeCallback> g_callbacks;
static std::vector<std::string> g_available_profiles;

// ============================================================================
// JSON Parsing Helpers (Simple Implementation)
// ============================================================================

// Simple JSON value extraction (no external library dependency)
static std::string ExtractJSONString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    
    size_t start = pos + 1;
    size_t end = json.find("\"", start);
    if (end == std::string::npos) return "";
    
    return json.substr(start, end - start);
}

static int ExtractJSONInt(const std::string& json, const std::string& key, int default_val = 0) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_val;
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return default_val;
    
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return default_val;
    }
}

static float ExtractJSONFloat(const std::string& json, const std::string& key, float default_val = 0.0f) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_val;
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return default_val;
    
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    try {
        return std::stof(json.substr(pos));
    } catch (...) {
        return default_val;
    }
}

static bool ExtractJSONBool(const std::string& json, const std::string& key, bool default_val = false) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return default_val;
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return default_val;
    
    // Find true or false
    size_t true_pos = json.find("true", pos);
    size_t false_pos = json.find("false", pos);
    
    if (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos)) {
        return true;
    }
    return false;
}

// ============================================================================
// File Utilities
// ============================================================================

static std::string ReadFileContent(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static bool WriteFileContent(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

static bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static void CreateDirectory(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

static std::vector<std::string> ListFiles(const std::string& directory, const std::string& extension) {
    std::vector<std::string> files;
    
    DIR* dir = opendir(directory.c_str());
    if (!dir) return files;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > extension.size() &&
            name.substr(name.size() - extension.size()) == extension) {
            files.push_back(directory + "/" + name);
        }
    }
    
    closedir(dir);
    return files;
}

// ============================================================================
// Built-in Profile Generators
// ============================================================================

OptimizationProfile GenerateAdrenoProfile(gpu::GPUTier tier) {
    OptimizationProfile profile;
    profile.name = "Adreno Optimized";
    profile.description = "Qualcomm Adreno GPU optimizations";
    profile.target_vendor = gpu::GPUVendor::QUALCOMM_ADRENO;
    profile.target_tier = tier;
    
    // Base settings for all Adreno GPUs
    profile.render.enable_texture_compression = true;
    profile.render.prefer_astc = true;
    profile.render.prefer_etc2 = true;
    
    profile.shader.use_half_precision = true;
    profile.shader.use_relaxed_precision = true;
    profile.shader.enable_vendor_extensions = true;
    profile.shader.enabled_extensions.push_back("VK_QCOM_render_pass_transform");
    profile.shader.enabled_extensions.push_back("VK_QCOM_render_pass_shader_resolve");
    
    // Adreno-specific: Enable binning (TBDR)
    profile.pipeline.enable_binning_optimization = true;
    profile.pipeline.bin_width = 32;
    profile.pipeline.bin_height = 32;
    
    // Vendor flags
    profile.vendor_flags["use_adreno_binning"] = true;
    profile.vendor_flags["enable_flexrender"] = true;
    profile.vendor_flags["prefer_ubwc"] = true;  // Universal Bandwidth Compression
    
    // Tier-specific settings
    switch (tier) {
        case gpu::GPUTier::ULTRA:
            profile.render.resolution_scale = 1.0f;
            profile.render.target_width = 1920;
            profile.render.target_height = 1080;
            profile.render.target_fps = 60;
            profile.render.anisotropic_level = 8;
            profile.render.aa_mode = RenderSettings::AAMode::TAA;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::HIGH;
            profile.render.shadow_map_size = 2048;
            profile.render.texture_quality = RenderSettings::TextureQuality::ULTRA;
            profile.render.enable_bloom = true;
            profile.render.enable_ambient_occlusion = true;
            profile.memory.texture_cache_size_mb = 512;
            profile.shader.precompile_thread_count = 4;
            break;
            
        case gpu::GPUTier::HIGH_END:
            profile.render.resolution_scale = 0.9f;
            profile.render.target_width = 1600;
            profile.render.target_height = 900;
            profile.render.target_fps = 60;
            profile.render.anisotropic_level = 4;
            profile.render.aa_mode = RenderSettings::AAMode::FXAA;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::MEDIUM;
            profile.render.shadow_map_size = 1024;
            profile.render.texture_quality = RenderSettings::TextureQuality::HIGH;
            profile.render.enable_bloom = true;
            profile.memory.texture_cache_size_mb = 384;
            profile.shader.precompile_thread_count = 3;
            break;
            
        case gpu::GPUTier::MID_RANGE:
            profile.render.resolution_scale = 0.75f;
            profile.render.target_width = 1280;
            profile.render.target_height = 720;
            profile.render.target_fps = 30;
            profile.render.anisotropic_level = 2;
            profile.render.aa_mode = RenderSettings::AAMode::FXAA;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::LOW;
            profile.render.shadow_map_size = 512;
            profile.render.texture_quality = RenderSettings::TextureQuality::MEDIUM;
            profile.memory.texture_cache_size_mb = 256;
            profile.shader.precompile_thread_count = 2;
            break;
            
        case gpu::GPUTier::LOW_END:
        default:
            profile.render.resolution_scale = 0.5f;
            profile.render.target_width = 960;
            profile.render.target_height = 540;
            profile.render.target_fps = 30;
            profile.render.anisotropic_level = 1;
            profile.render.aa_mode = RenderSettings::AAMode::NONE;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::OFF;
            profile.render.texture_quality = RenderSettings::TextureQuality::LOW;
            profile.render.enable_bloom = false;
            profile.memory.texture_cache_size_mb = 128;
            profile.shader.precompile_thread_count = 1;
            break;
    }
    
    return profile;
}

OptimizationProfile GenerateMaliProfile(gpu::GPUTier tier) {
    OptimizationProfile profile;
    profile.name = "Mali Optimized";
    profile.description = "ARM Mali GPU optimizations";
    profile.target_vendor = gpu::GPUVendor::ARM_MALI;
    profile.target_tier = tier;
    
    // Mali-specific settings
    profile.render.enable_texture_compression = true;
    profile.render.prefer_astc = true;
    profile.render.prefer_etc2 = true;
    
    profile.shader.use_half_precision = true;
    profile.shader.use_relaxed_precision = true;
    profile.shader.enable_vendor_extensions = true;
    
    // Enable AFBC (ARM Frame Buffer Compression)
    profile.pipeline.enable_afbc = true;
    
    // Vendor flags
    profile.vendor_flags["enable_afbc"] = true;
    profile.vendor_flags["enable_transaction_elimination"] = true;
    profile.vendor_flags["prefer_linear_textures"] = false;  // AFBC is better
    profile.vendor_flags["use_early_z"] = true;
    
    // Mali Valhall-specific
    profile.vendor_flags["enable_fma"] = true;
    profile.vendor_flags["use_vector_fp16"] = true;
    
    // Tier-specific settings (similar to Adreno but with Mali considerations)
    switch (tier) {
        case gpu::GPUTier::ULTRA:
            profile.render.resolution_scale = 1.0f;
            profile.render.target_width = 1920;
            profile.render.target_height = 1080;
            profile.render.target_fps = 60;
            profile.render.anisotropic_level = 8;
            profile.render.aa_mode = RenderSettings::AAMode::TAA;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::HIGH;
            profile.render.shadow_map_size = 2048;
            profile.render.texture_quality = RenderSettings::TextureQuality::ULTRA;
            profile.memory.texture_cache_size_mb = 512;
            // Mali Immortalis can handle more
            profile.vendor_ints["shader_cores"] = 14;
            break;
            
        case gpu::GPUTier::HIGH_END:
            profile.render.resolution_scale = 0.85f;
            profile.render.target_width = 1600;
            profile.render.target_height = 900;
            profile.render.target_fps = 60;
            profile.render.anisotropic_level = 4;
            profile.render.aa_mode = RenderSettings::AAMode::FXAA;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::MEDIUM;
            profile.render.shadow_map_size = 1024;
            profile.render.texture_quality = RenderSettings::TextureQuality::HIGH;
            profile.memory.texture_cache_size_mb = 384;
            break;
            
        case gpu::GPUTier::MID_RANGE:
            profile.render.resolution_scale = 0.7f;
            profile.render.target_width = 1280;
            profile.render.target_height = 720;
            profile.render.target_fps = 30;
            profile.render.anisotropic_level = 2;
            profile.render.aa_mode = RenderSettings::AAMode::FXAA;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::LOW;
            profile.render.shadow_map_size = 512;
            profile.render.texture_quality = RenderSettings::TextureQuality::MEDIUM;
            profile.memory.texture_cache_size_mb = 256;
            break;
            
        case gpu::GPUTier::LOW_END:
        default:
            profile.render.resolution_scale = 0.5f;
            profile.render.target_width = 960;
            profile.render.target_height = 540;
            profile.render.target_fps = 30;
            profile.render.anisotropic_level = 1;
            profile.render.aa_mode = RenderSettings::AAMode::NONE;
            profile.render.shadow_quality = RenderSettings::ShadowQuality::OFF;
            profile.render.texture_quality = RenderSettings::TextureQuality::LOW;
            profile.render.enable_bloom = false;
            profile.memory.texture_cache_size_mb = 128;
            // Disable AFBC on low-end to save bandwidth
            profile.pipeline.enable_afbc = false;
            break;
    }
    
    return profile;
}

OptimizationProfile GeneratePowerVRProfile(gpu::GPUTier tier) {
    OptimizationProfile profile;
    profile.name = "PowerVR Optimized";
    profile.description = "Imagination PowerVR GPU optimizations";
    profile.target_vendor = gpu::GPUVendor::IMAGINATION_POWERVR;
    profile.target_tier = tier;
    
    // PowerVR-specific settings
    profile.render.enable_texture_compression = true;
    profile.render.prefer_etc2 = true;  // PowerVR prefers ETC2
    
    profile.shader.use_half_precision = true;
    profile.shader.enable_vendor_extensions = true;
    
    // PowerVR is TBDR - optimize for that
    profile.pipeline.enable_binning_optimization = true;
    
    // Vendor flags
    profile.vendor_flags["enable_pvrtc"] = true;
    profile.vendor_flags["use_usc_cache"] = true;  // Unified Shading Cluster cache
    profile.vendor_flags["enable_hsr"] = true;     // Hidden Surface Removal
    
    // PowerVR tends to be mid-range, adjust settings accordingly
    profile.render.resolution_scale = 0.75f;
    profile.render.target_width = 1280;
    profile.render.target_height = 720;
    profile.render.target_fps = 30;
    profile.render.anisotropic_level = 2;
    profile.render.aa_mode = RenderSettings::AAMode::FXAA;
    profile.render.shadow_quality = RenderSettings::ShadowQuality::LOW;
    profile.render.texture_quality = RenderSettings::TextureQuality::MEDIUM;
    profile.memory.texture_cache_size_mb = 192;
    
    return profile;
}

OptimizationProfile GenerateGenericProfile(gpu::GPUTier tier) {
    OptimizationProfile profile;
    profile.name = "Generic";
    profile.description = "Generic GPU profile (no vendor optimizations)";
    profile.target_vendor = gpu::GPUVendor::UNKNOWN;
    profile.target_tier = tier;
    
    // Safe defaults
    profile.render.enable_texture_compression = true;
    profile.render.prefer_etc2 = true;
    
    profile.shader.use_half_precision = false;  // Conservative
    profile.shader.enable_vendor_extensions = false;
    
    switch (tier) {
        case gpu::GPUTier::HIGH_END:
        case gpu::GPUTier::ULTRA:
            profile.render.resolution_scale = 0.75f;
            profile.render.target_width = 1280;
            profile.render.target_height = 720;
            profile.render.target_fps = 30;
            break;
        default:
            profile.render.resolution_scale = 0.5f;
            profile.render.target_width = 960;
            profile.render.target_height = 540;
            profile.render.target_fps = 30;
            break;
    }
    
    return profile;
}

// ============================================================================
// Profile Loading
// ============================================================================

static bool ParseProfileFromJSON(const std::string& json, OptimizationProfile& profile) {
    if (json.empty()) return false;
    
    profile.name = ExtractJSONString(json, "name");
    profile.description = ExtractJSONString(json, "description");
    profile.version = ExtractJSONInt(json, "version", 1);
    profile.author = ExtractJSONString(json, "author");
    
    // Render settings
    profile.render.resolution_scale = ExtractJSONFloat(json, "resolution_scale", 1.0f);
    profile.render.target_width = ExtractJSONInt(json, "target_width", 1280);
    profile.render.target_height = ExtractJSONInt(json, "target_height", 720);
    profile.render.target_fps = ExtractJSONInt(json, "target_fps", 30);
    profile.render.anisotropic_level = ExtractJSONInt(json, "anisotropic_level", 4);
    profile.render.enable_bloom = ExtractJSONBool(json, "enable_bloom", true);
    profile.render.enable_dof = ExtractJSONBool(json, "enable_dof", false);
    profile.render.enable_motion_blur = ExtractJSONBool(json, "enable_motion_blur", false);
    
    // Shader settings
    profile.shader.use_half_precision = ExtractJSONBool(json, "use_half_precision", true);
    profile.shader.enable_precompilation = ExtractJSONBool(json, "enable_precompilation", true);
    profile.shader.precompile_thread_count = ExtractJSONInt(json, "precompile_thread_count", 2);
    
    // Memory settings
    profile.memory.texture_cache_size_mb = ExtractJSONInt(json, "texture_cache_size_mb", 256);
    profile.memory.enable_texture_streaming = ExtractJSONBool(json, "enable_texture_streaming", true);
    
    // Pipeline settings
    profile.pipeline.enable_pipeline_cache = ExtractJSONBool(json, "enable_pipeline_cache", true);
    profile.pipeline.enable_async_compilation = ExtractJSONBool(json, "enable_async_compilation", true);
    
    // Vendor-specific
    profile.vendor_flags["use_adreno_binning"] = ExtractJSONBool(json, "use_adreno_binning", false);
    profile.vendor_flags["enable_afbc"] = ExtractJSONBool(json, "enable_afbc", false);
    
    return !profile.name.empty();
}

bool LoadProfileFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    std::string json = ReadFileContent(path);
    if (json.empty()) {
        LOGE("Failed to read profile file: %s", path.c_str());
        return false;
    }
    
    OptimizationProfile profile;
    if (!ParseProfileFromJSON(json, profile)) {
        LOGE("Failed to parse profile: %s", path.c_str());
        return false;
    }
    
    g_active_profile = profile;
    LOGI("Loaded profile: %s from %s", profile.name.c_str(), path.c_str());
    
    return true;
}

bool LoadProfileForVendor(gpu::GPUVendor vendor) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    const auto& gpu_info = gpu::GetCachedGPUInfo();
    gpu::GPUTier tier = gpu_info.tier;
    if (tier == gpu::GPUTier::UNKNOWN) {
        tier = gpu::GPUTier::MID_RANGE;  // Safe default
    }
    
    // Try to load from file first
    std::string profile_path;
    switch (vendor) {
        case gpu::GPUVendor::QUALCOMM_ADRENO:
            profile_path = g_config.profiles_directory + "/adreno_profile.json";
            break;
        case gpu::GPUVendor::ARM_MALI:
            profile_path = g_config.profiles_directory + "/mali_profile.json";
            break;
        case gpu::GPUVendor::IMAGINATION_POWERVR:
            profile_path = g_config.profiles_directory + "/powervr_profile.json";
            break;
        default:
            profile_path = g_config.profiles_directory + "/generic_profile.json";
            break;
    }
    
    if (FileExists(profile_path)) {
        std::string json = ReadFileContent(profile_path);
        OptimizationProfile loaded;
        if (ParseProfileFromJSON(json, loaded)) {
            g_active_profile = loaded;
            LOGI("Loaded profile from file: %s", profile_path.c_str());
            return true;
        }
    }
    
    // Generate built-in profile
    LOGI("Generating built-in profile for vendor: %s", gpu::GetVendorName(vendor));
    
    switch (vendor) {
        case gpu::GPUVendor::QUALCOMM_ADRENO:
            g_active_profile = GenerateAdrenoProfile(tier);
            break;
        case gpu::GPUVendor::ARM_MALI:
            g_active_profile = GenerateMaliProfile(tier);
            break;
        case gpu::GPUVendor::IMAGINATION_POWERVR:
            g_active_profile = GeneratePowerVRProfile(tier);
            break;
        default:
            g_active_profile = GenerateGenericProfile(tier);
            break;
    }
    
    return true;
}

// ============================================================================
// Profile Application
// ============================================================================

bool ApplyActiveProfile() {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (g_active_profile.name.empty()) {
        LOGW("No active profile to apply");
        return false;
    }
    
    LOGI("Applying profile: %s", g_active_profile.name.c_str());
    LOGI("  Resolution: %dx%d @ %.0f%%", 
         g_active_profile.render.target_width,
         g_active_profile.render.target_height,
         g_active_profile.render.resolution_scale * 100.0f);
    LOGI("  Target FPS: %d", g_active_profile.render.target_fps);
    LOGI("  Texture cache: %zu MB", g_active_profile.memory.texture_cache_size_mb);
    LOGI("  Half precision: %s", g_active_profile.shader.use_half_precision ? "yes" : "no");
    
    // Notify callbacks
    for (const auto& callback : g_callbacks) {
        callback(g_active_profile);
    }
    
    return true;
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool InitializeAGVSOL(const AGVSOLConfig& config) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (g_initialized) {
        LOGI("AGVSOL already initialized");
        return true;
    }
    
    LOGI("===========================================");
    LOGI("AGVSOL - GPU Vendor-Specific Optimization");
    LOGI("===========================================");
    
    g_config = config;
    
    // Create directories
    CreateDirectory(config.profiles_directory);
    CreateDirectory(config.shaders_directory);
    CreateDirectory(config.cache_directory);
    
    // Initialize GPU detector
    if (config.auto_detect_gpu) {
        if (!gpu::InitializeGPUDetector()) {
            LOGW("GPU detection failed, using generic profile");
        }
    }
    
    const auto& gpu_info = gpu::GetCachedGPUInfo();
    
    LOGI("Detected GPU:");
    LOGI("  Vendor: %s", gpu_info.vendor_name.c_str());
    LOGI("  Model: %s", gpu_info.model.c_str());
    LOGI("  Tier: %s", gpu::GetTierName(gpu_info.tier));
    LOGI("  SoC: %s", gpu_info.soc_name.c_str());
    
    // Load appropriate profile
    if (config.auto_apply_profile) {
        LoadProfileForVendor(gpu_info.vendor);
        ApplyActiveProfile();
    }
    
    // Scan for available profiles
    g_available_profiles = ListFiles(config.profiles_directory, ".json");
    LOGI("Found %zu profile files", g_available_profiles.size());
    
    LOGI("===========================================");
    LOGI("AGVSOL Initialized Successfully");
    LOGI("===========================================");
    
    g_initialized = true;
    return true;
}

bool InitializeAGVSOL(const std::string& cache_dir) {
    AGVSOLConfig config;
    config.profiles_directory = cache_dir + "/agvsol/profiles";
    config.shaders_directory = cache_dir + "/agvsol/shaders";
    config.cache_directory = cache_dir + "/agvsol/cache";
    config.auto_detect_gpu = true;
    config.auto_apply_profile = true;
    return InitializeAGVSOL(config);
}

void ShutdownAGVSOL() {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (!g_initialized) return;
    
    gpu::ShutdownGPUDetector();
    
    g_active_profile = OptimizationProfile();
    g_shader_sets.clear();
    g_callbacks.clear();
    g_available_profiles.clear();
    g_initialized = false;
    
    LOGI("AGVSOL shutdown complete");
}

bool IsAGVSOLInitialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_initialized;
}

const gpu::GPUInfo& GetGPUInfo() {
    return gpu::GetCachedGPUInfo();
}

const OptimizationProfile& GetActiveProfile() {
    return g_active_profile;
}

const RenderSettings& GetRenderSettings() {
    return g_active_profile.render;
}

const ShaderSettings& GetShaderSettings() {
    return g_active_profile.shader;
}

const MemorySettings& GetMemorySettings() {
    return g_active_profile.memory;
}

const PipelineSettings& GetPipelineSettings() {
    return g_active_profile.pipeline;
}

bool GetVendorFlag(const std::string& key, bool default_value) {
    auto it = g_active_profile.vendor_flags.find(key);
    return (it != g_active_profile.vendor_flags.end()) ? it->second : default_value;
}

int GetVendorInt(const std::string& key, int default_value) {
    auto it = g_active_profile.vendor_ints.find(key);
    return (it != g_active_profile.vendor_ints.end()) ? it->second : default_value;
}

float GetVendorFloat(const std::string& key, float default_value) {
    auto it = g_active_profile.vendor_floats.find(key);
    return (it != g_active_profile.vendor_floats.end()) ? it->second : default_value;
}

std::string GetVendorString(const std::string& key, const std::string& default_value) {
    auto it = g_active_profile.vendor_strings.find(key);
    return (it != g_active_profile.vendor_strings.end()) ? it->second : default_value;
}

void RegisterProfileChangeCallback(ProfileChangeCallback callback) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callbacks.push_back(std::move(callback));
}

std::vector<std::string> GetAvailableProfiles() {
    return g_available_profiles;
}

AGVSOLStats GetStats() {
    AGVSOLStats stats;
    const auto& gpu_info = gpu::GetCachedGPUInfo();
    stats.detected_vendor = gpu_info.vendor;
    stats.detected_tier = gpu_info.tier;
    stats.active_profile_name = g_active_profile.name;
    stats.shaders_loaded = static_cast<int>(g_shader_sets.size());
    stats.profiles_available = static_cast<int>(g_available_profiles.size());
    stats.is_optimized = (gpu_info.vendor != gpu::GPUVendor::UNKNOWN);
    return stats;
}

std::string ExportProfileToJSON() {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << g_active_profile.name << "\",\n";
    ss << "  \"description\": \"" << g_active_profile.description << "\",\n";
    ss << "  \"version\": " << g_active_profile.version << ",\n";
    ss << "  \"resolution_scale\": " << g_active_profile.render.resolution_scale << ",\n";
    ss << "  \"target_width\": " << g_active_profile.render.target_width << ",\n";
    ss << "  \"target_height\": " << g_active_profile.render.target_height << ",\n";
    ss << "  \"target_fps\": " << g_active_profile.render.target_fps << ",\n";
    ss << "  \"anisotropic_level\": " << g_active_profile.render.anisotropic_level << ",\n";
    ss << "  \"enable_bloom\": " << (g_active_profile.render.enable_bloom ? "true" : "false") << ",\n";
    ss << "  \"use_half_precision\": " << (g_active_profile.shader.use_half_precision ? "true" : "false") << ",\n";
    ss << "  \"enable_precompilation\": " << (g_active_profile.shader.enable_precompilation ? "true" : "false") << ",\n";
    ss << "  \"texture_cache_size_mb\": " << g_active_profile.memory.texture_cache_size_mb << ",\n";
    ss << "  \"enable_pipeline_cache\": " << (g_active_profile.pipeline.enable_pipeline_cache ? "true" : "false") << "\n";
    ss << "}\n";
    return ss.str();
}

bool ImportProfileFromJSON(const std::string& json) {
    OptimizationProfile profile;
    if (!ParseProfileFromJSON(json, profile)) {
        return false;
    }
    g_active_profile = profile;
    return ApplyActiveProfile();
}

int LoadShaderVariants() {
    // TODO: Implement shader variant loading
    return 0;
}

const ShaderVariant* GetShaderVariant(const std::string& shader_name) {
    const auto& gpu_info = gpu::GetCachedGPUInfo();
    
    for (const auto& set : g_shader_sets) {
        if (set.name == shader_name) {
            return set.GetVariant(gpu_info.vendor);
        }
    }
    
    return nullptr;
}

void SetRenderSetting(const std::string& key, float value) {
    if (key == "resolution_scale") g_active_profile.render.resolution_scale = value;
}

void SetRenderSetting(const std::string& key, int value) {
    if (key == "target_width") g_active_profile.render.target_width = value;
    else if (key == "target_height") g_active_profile.render.target_height = value;
    else if (key == "target_fps") g_active_profile.render.target_fps = value;
    else if (key == "anisotropic_level") g_active_profile.render.anisotropic_level = value;
}

void SetRenderSetting(const std::string& key, bool value) {
    if (key == "enable_bloom") g_active_profile.render.enable_bloom = value;
    else if (key == "enable_dof") g_active_profile.render.enable_dof = value;
    else if (key == "enable_motion_blur") g_active_profile.render.enable_motion_blur = value;
}

} // namespace agvsol
} // namespace rpcsx
