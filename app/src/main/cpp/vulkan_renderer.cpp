#include "vulkan_renderer.h"
#include <android/log.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_TAG "RPCSX-Vulkan"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::vulkan {

namespace {

static constexpr uint32_t kPipelineCacheMetaVersion = 1;

static std::string ReadTextFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool WriteTextFile(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) return false;
    out << contents;
    return true;
}

static std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    if (i) s.erase(0, i);
    return s;
}

static std::string GetGpuIdentity() {
    const char* paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/kgsl/kgsl-3d0/gpu_chipid",
        "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",
    };
    for (const char* p : paths) {
        std::string v = Trim(ReadTextFile(p));
        if (!v.empty()) return std::string(p) + ":" + v;
    }
    return "unknown";
}

static std::string GetBuildId() {
    return std::string("native-") + __DATE__ + "-" + __TIME__;
}

static bool IsCacheCompatible(const std::string& metaPath,
                              const std::string& buildId,
                              const std::string& gpuId) {
    const std::string meta = ReadTextFile(metaPath);
    if (meta.empty()) return false;

    uint32_t version = 0;
    std::string build, gpu;
    std::istringstream iss(meta);
    std::string line;
    while (std::getline(iss, line)) {
        line = Trim(line);
        if (line.rfind("version=", 0) == 0) version = static_cast<uint32_t>(std::strtoul(line.c_str() + 8, nullptr, 10));
        if (line.rfind("build=", 0) == 0) build = line.substr(6);
        if (line.rfind("gpu=", 0) == 0) gpu = line.substr(4);
    }

    return version == kPipelineCacheMetaVersion && build == buildId && gpu == gpuId;
}

static void WriteCacheMeta(const std::string& metaPath,
                           const std::string& buildId,
                           const std::string& gpuId) {
    std::ostringstream ss;
    ss << "version=" << kPipelineCacheMetaVersion << "\n";
    ss << "build=" << buildId << "\n";
    ss << "gpu=" << gpuId << "\n";
    (void)WriteTextFile(metaPath, ss.str());
}

} // namespace

bool InitializeVulkan(const VulkanConfig& config) {
    LOGI("Initializing Vulkan 1.3 with Mesa Turnip support...");
    if (config.enable_validation) {
        LOGI("Validation layers enabled");
    }
    // Stub implementation
    return true;
}

VkDevice CreateOptimizedDevice(VkPhysicalDevice physical_device) {
    // Stub implementation
    LOGI("Creating optimized Vulkan device for Adreno 735...");
    return VK_NULL_HANDLE;
}

VkPipelineCache CreatePipelineCache(VkDevice device, const char* cache_file) {
    if (device == VK_NULL_HANDLE || cache_file == nullptr) return VK_NULL_HANDLE;

    const std::string buildId = GetBuildId();
    const std::string gpuId = GetGpuIdentity();
    const std::string metaPath = std::string(cache_file) + ".meta";

    // Purge stale cache if metadata does not match.
    if (!IsCacheCompatible(metaPath, buildId, gpuId)) {
        LOGI("Pipeline cache meta mismatch; purging %s", cache_file);
        unlink(cache_file);
        unlink(metaPath.c_str());
    }

    std::vector<char> cacheData;
    struct stat st {};
    if (stat(cache_file, &st) == 0 && st.st_size > 0) {
        cacheData.resize(static_cast<size_t>(st.st_size));
        std::ifstream in(cache_file, std::ios::binary);
        if (in) {
            in.read(cacheData.data(), cacheData.size());
            LOGI("Loaded existing pipeline cache (%zu bytes)", cacheData.size());
        } else {
            cacheData.clear();
        }
    }

    VkPipelineCacheCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.initialDataSize = cacheData.size();
    info.pInitialData = cacheData.empty() ? nullptr : cacheData.data();

    auto fpCreate = reinterpret_cast<PFN_vkCreatePipelineCache>(vkGetDeviceProcAddr(device, "vkCreatePipelineCache"));
    if (!fpCreate) return VK_NULL_HANDLE;

    VkPipelineCache cache = VK_NULL_HANDLE;
    VkResult res = fpCreate(device, &info, nullptr, &cache);
    if (res != VK_SUCCESS) {
        LOGE("vkCreatePipelineCache failed: %d", res);
        return VK_NULL_HANDLE;
    }

    // Record meta for future runs.
    WriteCacheMeta(metaPath, buildId, gpuId);
    return cache;
}

void SavePipelineCache(VkDevice device, VkPipelineCache cache, const char* cache_file) {
    if (device == VK_NULL_HANDLE || cache == VK_NULL_HANDLE || cache_file == nullptr) return;

    auto fpGetData = reinterpret_cast<PFN_vkGetPipelineCacheData>(vkGetDeviceProcAddr(device, "vkGetPipelineCacheData"));
    if (!fpGetData) return;

    size_t size = 0;
    if (fpGetData(device, cache, &size, nullptr) != VK_SUCCESS || size == 0) {
        return;
    }

    std::vector<char> data(size);
    if (fpGetData(device, cache, &size, data.data()) != VK_SUCCESS) {
        return;
    }

    std::ofstream out(cache_file, std::ios::binary | std::ios::out | std::ios::trunc);
    if (out) {
        out.write(data.data(), static_cast<std::streamsize>(size));
        LOGI("Saved pipeline cache (%zu bytes) to %s", size, cache_file);
        const std::string metaPath = std::string(cache_file) + ".meta";
        WriteCacheMeta(metaPath, GetBuildId(), GetGpuIdentity());
    }
}

} // namespace rpcsx::vulkan
