/**
 * RPCSX GPU Detector Implementation
 * 
 * Виявлення GPU через Android system properties та Vulkan API
 */

#include "gpu_detector.h"

#include <android/log.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <cstring>
#include <regex>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>

#define LOG_TAG "RPCSX-GPUDetector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace gpu {

// ============================================================================
// Global State
// ============================================================================

static std::mutex g_mutex;
static bool g_initialized = false;
static GPUInfo g_cached_info;
static std::vector<GPUDetectionCallback> g_callbacks;

// ============================================================================
// Android System Property Helpers
// ============================================================================

static std::string GetSystemProperty(const char* name) {
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get(name, value);
    return std::string(value);
}

static std::string ReadFileContent(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ============================================================================
// Vendor Name Mapping
// ============================================================================

const char* GetVendorName(GPUVendor vendor) {
    switch (vendor) {
        case GPUVendor::QUALCOMM_ADRENO: return "Qualcomm";
        case GPUVendor::ARM_MALI: return "ARM";
        case GPUVendor::IMAGINATION_POWERVR: return "Imagination";
        case GPUVendor::INTEL: return "Intel";
        case GPUVendor::NVIDIA: return "NVIDIA";
        case GPUVendor::AMD: return "AMD";
        default: return "Unknown";
    }
}

const char* GetTierName(GPUTier tier) {
    switch (tier) {
        case GPUTier::LOW_END: return "Low-End";
        case GPUTier::MID_RANGE: return "Mid-Range";
        case GPUTier::HIGH_END: return "High-End";
        case GPUTier::ULTRA: return "Ultra";
        default: return "Unknown";
    }
}

// ============================================================================
// Adreno Detection
// ============================================================================

// Adreno model database with performance tiers
struct AdrenoModelInfo {
    const char* pattern;
    AdrenoModel model;
    GPUTier tier;
    int compute_units;
    float tflops;
    const char* architecture;
};

static const AdrenoModelInfo g_adreno_db[] = {
    // Adreno 8xx (future)
    {"830", AdrenoModel::ADRENO_830, GPUTier::ULTRA, 6, 4.5f, "A8x"},
    
    // Adreno 7xx series
    {"760", AdrenoModel::ADRENO_760, GPUTier::ULTRA, 5, 3.8f, "A7x"},
    {"750", AdrenoModel::ADRENO_750, GPUTier::ULTRA, 4, 3.2f, "A7x"},
    {"740", AdrenoModel::ADRENO_740, GPUTier::HIGH_END, 4, 2.8f, "A7x"},
    {"730", AdrenoModel::ADRENO_730, GPUTier::HIGH_END, 4, 2.5f, "A7x"},
    {"720", AdrenoModel::ADRENO_720, GPUTier::HIGH_END, 3, 2.0f, "A7x"},
    {"710", AdrenoModel::ADRENO_710, GPUTier::MID_RANGE, 3, 1.6f, "A7x"},
    
    // Adreno 6xx series
    {"660", AdrenoModel::ADRENO_660, GPUTier::HIGH_END, 3, 1.7f, "A6x"},
    {"650", AdrenoModel::ADRENO_650, GPUTier::HIGH_END, 3, 1.5f, "A6x"},
    {"642", AdrenoModel::ADRENO_642L, GPUTier::MID_RANGE, 2, 1.0f, "A6x"},
    {"640", AdrenoModel::ADRENO_640, GPUTier::MID_RANGE, 2, 1.2f, "A6x"},
    {"630", AdrenoModel::ADRENO_630, GPUTier::MID_RANGE, 2, 0.9f, "A6x"},
    {"620", AdrenoModel::ADRENO_620, GPUTier::MID_RANGE, 2, 0.7f, "A6x"},
    {"619", AdrenoModel::ADRENO_619, GPUTier::LOW_END, 2, 0.5f, "A6x"},
    {"618", AdrenoModel::ADRENO_618, GPUTier::LOW_END, 2, 0.5f, "A6x"},
    {"616", AdrenoModel::ADRENO_616, GPUTier::LOW_END, 2, 0.4f, "A6x"},
    {"615", AdrenoModel::ADRENO_615, GPUTier::LOW_END, 2, 0.4f, "A6x"},
    {"612", AdrenoModel::ADRENO_612, GPUTier::LOW_END, 1, 0.3f, "A6x"},
    {"610", AdrenoModel::ADRENO_610, GPUTier::LOW_END, 1, 0.3f, "A6x"},
};

AdrenoModel DetectAdrenoModel(const std::string& renderer) {
    for (const auto& info : g_adreno_db) {
        if (renderer.find(info.pattern) != std::string::npos) {
            return info.model;
        }
    }
    return AdrenoModel::UNKNOWN;
}

static const AdrenoModelInfo* GetAdrenoModelInfo(AdrenoModel model) {
    for (const auto& info : g_adreno_db) {
        if (info.model == model) {
            return &info;
        }
    }
    return nullptr;
}

// ============================================================================
// Mali Detection
// ============================================================================

struct MaliModelInfo {
    const char* pattern;
    MaliModel model;
    GPUTier tier;
    int compute_units;
    float tflops;
    const char* architecture;
};

static const MaliModelInfo g_mali_db[] = {
    // Immortalis (Valhall Gen 5)
    {"G925", MaliModel::MALI_IMMORTALIS_G925, GPUTier::ULTRA, 14, 4.0f, "Valhall5"},
    {"Immortalis-G720", MaliModel::MALI_IMMORTALIS_G720, GPUTier::ULTRA, 12, 3.5f, "Valhall4"},
    {"Immortalis-G715", MaliModel::MALI_IMMORTALIS_G715, GPUTier::HIGH_END, 11, 2.8f, "Valhall"},
    
    // Mali-G7xx (Valhall)
    {"G725", MaliModel::MALI_G725, GPUTier::HIGH_END, 10, 2.5f, "Valhall"},
    {"G720", MaliModel::MALI_G720, GPUTier::HIGH_END, 10, 2.2f, "Valhall"},
    {"G715", MaliModel::MALI_G715, GPUTier::HIGH_END, 11, 2.0f, "Valhall"},
    {"G710", MaliModel::MALI_G710, GPUTier::HIGH_END, 10, 1.8f, "Valhall"},
    {"G610", MaliModel::MALI_G610, GPUTier::MID_RANGE, 6, 1.0f, "Valhall"},
    {"G510", MaliModel::MALI_G510, GPUTier::MID_RANGE, 4, 0.8f, "Valhall"},
    {"G310", MaliModel::MALI_G310, GPUTier::LOW_END, 2, 0.4f, "Valhall"},
    
    // Mali-G7x (Bifrost)
    {"G78", MaliModel::MALI_G78, GPUTier::HIGH_END, 14, 1.6f, "Bifrost"},
    {"G77", MaliModel::MALI_G77, GPUTier::HIGH_END, 11, 1.4f, "Bifrost"},
    {"G76", MaliModel::MALI_G76, GPUTier::MID_RANGE, 12, 1.1f, "Bifrost"},
    {"G72", MaliModel::MALI_G72, GPUTier::MID_RANGE, 12, 0.9f, "Bifrost"},
    {"G71", MaliModel::MALI_G71, GPUTier::MID_RANGE, 8, 0.7f, "Bifrost"},
    {"G52", MaliModel::MALI_G52, GPUTier::LOW_END, 2, 0.4f, "Bifrost"},
    {"G51", MaliModel::MALI_G51, GPUTier::LOW_END, 4, 0.3f, "Bifrost"},
};

MaliModel DetectMaliModel(const std::string& renderer) {
    for (const auto& info : g_mali_db) {
        if (renderer.find(info.pattern) != std::string::npos) {
            return info.model;
        }
    }
    return MaliModel::UNKNOWN;
}

static const MaliModelInfo* GetMaliModelInfo(MaliModel model) {
    for (const auto& info : g_mali_db) {
        if (info.model == model) {
            return &info;
        }
    }
    return nullptr;
}

// ============================================================================
// SoC Detection from System Properties
// ============================================================================

struct SoCInfo {
    const char* board_pattern;
    const char* hardware_pattern;
    const char* soc_name;
    GPUVendor expected_vendor;
};

static const SoCInfo g_soc_db[] = {
    // Qualcomm Snapdragon
    {"sm8650", "qcom", "Snapdragon 8 Gen 3", GPUVendor::QUALCOMM_ADRENO},
    {"sm8635", "qcom", "Snapdragon 8s Gen 3", GPUVendor::QUALCOMM_ADRENO},
    {"sm8550", "qcom", "Snapdragon 8 Gen 2", GPUVendor::QUALCOMM_ADRENO},
    {"sm8475", "qcom", "Snapdragon 8+ Gen 1", GPUVendor::QUALCOMM_ADRENO},
    {"sm8450", "qcom", "Snapdragon 8 Gen 1", GPUVendor::QUALCOMM_ADRENO},
    {"sm8350", "qcom", "Snapdragon 888", GPUVendor::QUALCOMM_ADRENO},
    {"sm8250", "qcom", "Snapdragon 865", GPUVendor::QUALCOMM_ADRENO},
    {"sm8150", "qcom", "Snapdragon 855", GPUVendor::QUALCOMM_ADRENO},
    {"sm7325", "qcom", "Snapdragon 778G", GPUVendor::QUALCOMM_ADRENO},
    {"sm7250", "qcom", "Snapdragon 765G", GPUVendor::QUALCOMM_ADRENO},
    {"taro", "qcom", "Snapdragon 8 Gen 1", GPUVendor::QUALCOMM_ADRENO},
    {"kalama", "qcom", "Snapdragon 8 Gen 2", GPUVendor::QUALCOMM_ADRENO},
    {"pineapple", "qcom", "Snapdragon 8 Gen 3", GPUVendor::QUALCOMM_ADRENO},
    {"crow", "qcom", "Snapdragon 8s Gen 3", GPUVendor::QUALCOMM_ADRENO},
    
    // Samsung Exynos
    {"s5e9945", "exynos", "Exynos 2400", GPUVendor::ARM_MALI},
    {"s5e9935", "exynos", "Exynos 2200", GPUVendor::ARM_MALI},
    {"s5e9925", "exynos", "Exynos 2100", GPUVendor::ARM_MALI},
    {"exynos990", "exynos", "Exynos 990", GPUVendor::ARM_MALI},
    {"exynos9820", "exynos", "Exynos 9820", GPUVendor::ARM_MALI},
    
    // MediaTek Dimensity
    {"mt6989", "mt6989", "Dimensity 9300", GPUVendor::ARM_MALI},
    {"mt6985", "mt6985", "Dimensity 9200", GPUVendor::ARM_MALI},
    {"mt6983", "mt6983", "Dimensity 9000", GPUVendor::ARM_MALI},
    {"mt6895", "mt6895", "Dimensity 8100", GPUVendor::ARM_MALI},
    {"mt6893", "mt6893", "Dimensity 1200", GPUVendor::ARM_MALI},
    
    // Google Tensor
    {"gs201", "tensor", "Google Tensor G2", GPUVendor::ARM_MALI},
    {"gs101", "tensor", "Google Tensor", GPUVendor::ARM_MALI},
    {"zuma", "tensor", "Google Tensor G3", GPUVendor::ARM_MALI},
    
    // NVIDIA
    {"tegra", "tegra", "NVIDIA Tegra", GPUVendor::NVIDIA},
    {"t210", "tegra", "Tegra X1", GPUVendor::NVIDIA},
};

static std::string DetectSoCName() {
    std::string board = GetSystemProperty("ro.board.platform");
    std::string hardware = GetSystemProperty("ro.hardware");
    
    std::transform(board.begin(), board.end(), board.begin(), ::tolower);
    std::transform(hardware.begin(), hardware.end(), hardware.begin(), ::tolower);
    
    for (const auto& soc : g_soc_db) {
        if (board.find(soc.board_pattern) != std::string::npos ||
            hardware.find(soc.hardware_pattern) != std::string::npos) {
            return soc.soc_name;
        }
    }
    
    return "Unknown SoC";
}

// ============================================================================
// Vulkan-based Detection
// ============================================================================

// Vulkan vendor IDs
constexpr uint32_t VK_VENDOR_AMD = 0x1002;
constexpr uint32_t VK_VENDOR_NVIDIA = 0x10DE;
constexpr uint32_t VK_VENDOR_INTEL = 0x8086;
constexpr uint32_t VK_VENDOR_QUALCOMM = 0x5143;
constexpr uint32_t VK_VENDOR_ARM = 0x13B5;
constexpr uint32_t VK_VENDOR_IMAGINATION = 0x1010;

static GPUVendor VendorIdToEnum(uint32_t vendor_id) {
    switch (vendor_id) {
        case VK_VENDOR_QUALCOMM: return GPUVendor::QUALCOMM_ADRENO;
        case VK_VENDOR_ARM: return GPUVendor::ARM_MALI;
        case VK_VENDOR_IMAGINATION: return GPUVendor::IMAGINATION_POWERVR;
        case VK_VENDOR_NVIDIA: return GPUVendor::NVIDIA;
        case VK_VENDOR_INTEL: return GPUVendor::INTEL;
        case VK_VENDOR_AMD: return GPUVendor::AMD;
        default: return GPUVendor::UNKNOWN;
    }
}

// Vulkan types (minimal definitions to avoid header dependency)
typedef uint32_t VkResult;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkInstance_T* VkInstance;

struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
    // ... limits and sparse properties omitted
};

typedef void (*PFN_vkGetPhysicalDeviceProperties)(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties* pProperties);

static GPUInfo DetectFromVulkan(void* vk_instance, void* vk_physical_device) {
    GPUInfo info;
    
    if (!vk_physical_device) {
        LOGW("No Vulkan physical device provided for detection");
        return info;
    }
    
    // Try to load Vulkan and get properties
    void* vulkan_lib = dlopen("libvulkan.so", RTLD_NOW);
    if (!vulkan_lib) {
        LOGW("Failed to load libvulkan.so: %s", dlerror());
        return info;
    }
    
    auto vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)
        dlsym(vulkan_lib, "vkGetPhysicalDeviceProperties");
    
    if (vkGetPhysicalDeviceProperties) {
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties((VkPhysicalDevice)vk_physical_device, &props);
        
        info.vendor_id = props.vendorID;
        info.device_id = props.deviceID;
        info.vendor = VendorIdToEnum(props.vendorID);
        info.renderer_name = props.deviceName;
        info.model = props.deviceName;
        
        // Parse Vulkan version
        uint32_t major = (props.apiVersion >> 22) & 0x3FF;
        uint32_t minor = (props.apiVersion >> 12) & 0x3FF;
        
        info.features.vulkan_1_0 = true;
        info.features.vulkan_1_1 = (major >= 1 && minor >= 1);
        info.features.vulkan_1_2 = (major >= 1 && minor >= 2);
        info.features.vulkan_1_3 = (major >= 1 && minor >= 3);
        
        // Driver version
        char driver_str[64];
        if (info.vendor == GPUVendor::QUALCOMM_ADRENO) {
            // Qualcomm uses packed version
            uint32_t d_major = props.driverVersion >> 24;
            uint32_t d_minor = (props.driverVersion >> 12) & 0xFFF;
            uint32_t d_patch = props.driverVersion & 0xFFF;
            snprintf(driver_str, sizeof(driver_str), "%u.%u.%u", d_major, d_minor, d_patch);
        } else {
            // Standard Vulkan version encoding
            uint32_t d_major = (props.driverVersion >> 22) & 0x3FF;
            uint32_t d_minor = (props.driverVersion >> 12) & 0x3FF;
            uint32_t d_patch = props.driverVersion & 0xFFF;
            snprintf(driver_str, sizeof(driver_str), "%u.%u.%u", d_major, d_minor, d_patch);
        }
        info.driver_version = driver_str;
        
        LOGI("Vulkan detection: %s (Vendor: 0x%X, Device: 0x%X)",
             props.deviceName, props.vendorID, props.deviceID);
    }
    
    dlclose(vulkan_lib);
    return info;
}

// ============================================================================
// System Property-based Detection (Fallback)
// ============================================================================

static GPUInfo DetectFromSystemProperties() {
    GPUInfo info;
    
    // Read system properties
    info.board_platform = GetSystemProperty("ro.board.platform");
    info.hardware = GetSystemProperty("ro.hardware");
    std::string gl_renderer = GetSystemProperty("ro.hardware.egl");
    
    LOGI("System properties: board=%s, hardware=%s, egl=%s",
         info.board_platform.c_str(), info.hardware.c_str(), gl_renderer.c_str());
    
    // Detect vendor from board platform
    std::string board_lower = info.board_platform;
    std::transform(board_lower.begin(), board_lower.end(), board_lower.begin(), ::tolower);
    
    if (board_lower.find("qcom") != std::string::npos ||
        board_lower.find("sdm") != std::string::npos ||
        board_lower.find("sm") != std::string::npos ||
        board_lower.find("msm") != std::string::npos ||
        board_lower.find("apq") != std::string::npos) {
        info.vendor = GPUVendor::QUALCOMM_ADRENO;
        info.vendor_name = "Qualcomm";
    }
    else if (board_lower.find("exynos") != std::string::npos ||
             board_lower.find("s5e") != std::string::npos ||
             board_lower.find("mt68") != std::string::npos ||
             board_lower.find("mt69") != std::string::npos) {
        info.vendor = GPUVendor::ARM_MALI;
        info.vendor_name = "ARM";
    }
    else if (board_lower.find("tegra") != std::string::npos) {
        info.vendor = GPUVendor::NVIDIA;
        info.vendor_name = "NVIDIA";
    }
    
    // Get SoC name
    info.soc_name = DetectSoCName();
    
    return info;
}

// ============================================================================
// Combined Detection Logic
// ============================================================================

static void FillGPUDetails(GPUInfo& info) {
    // Set vendor name
    info.vendor_name = GetVendorName(info.vendor);
    
    // Detect model and fill architecture info
    if (info.vendor == GPUVendor::QUALCOMM_ADRENO) {
        AdrenoModel model = DetectAdrenoModel(info.renderer_name);
        if (auto* model_info = GetAdrenoModelInfo(model)) {
            info.tier = model_info->tier;
            info.compute_units = model_info->compute_units;
            info.estimated_tflops = model_info->tflops;
            info.architecture = model_info->architecture;
            
            // Adreno-specific features
            info.features.adreno_binning = true;
            info.features.astc_ldr = true;
            info.features.etc2 = true;
        }
    }
    else if (info.vendor == GPUVendor::ARM_MALI) {
        MaliModel model = DetectMaliModel(info.renderer_name);
        if (auto* model_info = GetMaliModelInfo(model)) {
            info.tier = model_info->tier;
            info.compute_units = model_info->compute_units;
            info.estimated_tflops = model_info->tflops;
            info.architecture = model_info->architecture;
            
            // Mali-specific features
            info.features.mali_afbc = true;
            info.features.astc_ldr = true;
            info.features.etc2 = true;
            
            // Valhall supports more features
            if (info.architecture == "Valhall" || info.architecture == "Valhall4" || info.architecture == "Valhall5") {
                info.features.compute_shaders = true;
                info.features.shader_float16 = true;
            }
        }
    }
    else if (info.vendor == GPUVendor::IMAGINATION_POWERVR) {
        info.features.powervr_pvrtc = true;
        info.features.etc2 = true;
        info.tier = GPUTier::MID_RANGE; // Conservative estimate
    }
    
    // If tier still unknown, make educated guess
    if (info.tier == GPUTier::UNKNOWN) {
        info.tier = DetermineGPUTier(info);
    }
    
    // Set recommended settings based on tier
    switch (info.tier) {
        case GPUTier::ULTRA:
            info.recommended_resolution_x = 1920;
            info.recommended_resolution_y = 1080;
            info.recommended_fps = 60;
            break;
        case GPUTier::HIGH_END:
            info.recommended_resolution_x = 1600;
            info.recommended_resolution_y = 900;
            info.recommended_fps = 60;
            break;
        case GPUTier::MID_RANGE:
            info.recommended_resolution_x = 1280;
            info.recommended_resolution_y = 720;
            info.recommended_fps = 30;
            break;
        case GPUTier::LOW_END:
        default:
            info.recommended_resolution_x = 960;
            info.recommended_resolution_y = 540;
            info.recommended_fps = 30;
            break;
    }
    
    // Common features for all modern GPUs
    info.features.compute_shaders = true;
    info.features.device_local_memory = true;
    info.features.draw_indirect = true;
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool InitializeGPUDetector(void* vk_instance, void* vk_physical_device) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (g_initialized) {
        LOGI("GPU detector already initialized");
        return true;
    }
    
    LOGI("Initializing GPU Detector...");
    
    // Try Vulkan detection first
    if (vk_physical_device) {
        g_cached_info = DetectFromVulkan(vk_instance, vk_physical_device);
    }
    
    // Fallback or supplement with system properties
    if (g_cached_info.vendor == GPUVendor::UNKNOWN) {
        g_cached_info = DetectFromSystemProperties();
    } else {
        // Merge system property info
        GPUInfo sys_info = DetectFromSystemProperties();
        if (g_cached_info.soc_name.empty()) g_cached_info.soc_name = sys_info.soc_name;
        if (g_cached_info.board_platform.empty()) g_cached_info.board_platform = sys_info.board_platform;
        if (g_cached_info.hardware.empty()) g_cached_info.hardware = sys_info.hardware;
    }
    
    // Fill in detailed info
    FillGPUDetails(g_cached_info);
    
    LOGI("=== GPU Detection Results ===");
    LOGI("  Vendor: %s", g_cached_info.vendor_name.c_str());
    LOGI("  Model: %s", g_cached_info.model.c_str());
    LOGI("  Renderer: %s", g_cached_info.renderer_name.c_str());
    LOGI("  Architecture: %s", g_cached_info.architecture.c_str());
    LOGI("  Tier: %s", GetTierName(g_cached_info.tier));
    LOGI("  SoC: %s", g_cached_info.soc_name.c_str());
    LOGI("  Est. TFLOPS: %.2f", g_cached_info.estimated_tflops);
    LOGI("  Recommended: %dx%d @ %d FPS",
         g_cached_info.recommended_resolution_x,
         g_cached_info.recommended_resolution_y,
         g_cached_info.recommended_fps);
    LOGI("=============================");
    
    // Notify callbacks
    for (const auto& callback : g_callbacks) {
        callback(g_cached_info);
    }
    
    g_initialized = true;
    return g_cached_info.vendor != GPUVendor::UNKNOWN;
}

void ShutdownGPUDetector() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_initialized = false;
    g_cached_info = GPUInfo();
    g_callbacks.clear();
    LOGI("GPU Detector shutdown");
}

bool IsGPUDetectorInitialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_initialized;
}

GPUVendor DetectGPUVendor() {
    if (!g_initialized) {
        InitializeGPUDetector();
    }
    return g_cached_info.vendor;
}

std::string GetGPUModel() {
    if (!g_initialized) {
        InitializeGPUDetector();
    }
    return g_cached_info.model;
}

GPUInfo GetGPUInfo() {
    if (!g_initialized) {
        InitializeGPUDetector();
    }
    return g_cached_info;
}

const GPUInfo& GetCachedGPUInfo() {
    return g_cached_info;
}

void RefreshGPUInfo() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_initialized = false;
    InitializeGPUDetector();
}

GPUTier DetermineGPUTier(const GPUInfo& info) {
    // Based on estimated performance
    if (info.estimated_tflops >= 3.0f) {
        return GPUTier::ULTRA;
    } else if (info.estimated_tflops >= 1.5f) {
        return GPUTier::HIGH_END;
    } else if (info.estimated_tflops >= 0.8f) {
        return GPUTier::MID_RANGE;
    } else if (info.estimated_tflops > 0.0f) {
        return GPUTier::LOW_END;
    }
    
    // Fallback: guess based on SoC name
    std::string soc_lower = info.soc_name;
    std::transform(soc_lower.begin(), soc_lower.end(), soc_lower.begin(), ::tolower);
    
    if (soc_lower.find("gen 3") != std::string::npos ||
        soc_lower.find("9300") != std::string::npos ||
        soc_lower.find("2400") != std::string::npos) {
        return GPUTier::ULTRA;
    }
    else if (soc_lower.find("gen 2") != std::string::npos ||
             soc_lower.find("gen 1") != std::string::npos ||
             soc_lower.find("888") != std::string::npos ||
             soc_lower.find("9200") != std::string::npos ||
             soc_lower.find("2200") != std::string::npos) {
        return GPUTier::HIGH_END;
    }
    else if (soc_lower.find("865") != std::string::npos ||
             soc_lower.find("855") != std::string::npos ||
             soc_lower.find("778") != std::string::npos) {
        return GPUTier::MID_RANGE;
    }
    
    return GPUTier::LOW_END;
}

void RegisterGPUDetectionCallback(GPUDetectionCallback callback) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_callbacks.push_back(std::move(callback));
}

} // namespace gpu
} // namespace rpcsx
