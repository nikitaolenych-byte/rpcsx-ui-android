#include "vulkan_renderer.h"
#include "signal_handler.h"
#include <android/log.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <atomic>
#include <thread>

#define LOG_TAG "RPCSX-Vulkan"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace rpcsx::vulkan {

namespace {

static constexpr uint32_t kPipelineCacheMetaVersion = 2;  // Bumped for ARMv9 changes

// Adreno 735 специфічні розширення
static const char* kAdrenoExtensions[] = {
    "VK_QCOM_render_pass_shader_resolve",
    "VK_QCOM_render_pass_transform",
    "VK_QCOM_rotated_copy_commands",
    "VK_QCOM_tile_properties",
    "VK_QCOM_fragment_density_map_offset",
    "VK_EXT_robustness2",
    "VK_EXT_shader_demote_to_helper_invocation",
    "VK_KHR_shader_float16_int8",
    "VK_KHR_16bit_storage",
    nullptr
};

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
    std::string result;
    for (const char* p : paths) {
        std::string v = Trim(ReadTextFile(p));
        if (!v.empty()) {
            if (!result.empty()) result += "|";
            result += v;
        }
    }
    if (result.empty()) result = "unknown";
    return result;
}

static std::string GetBuildId() {
    return std::string("rpcsx-armv9-") + __DATE__ + "-" + __TIME__;
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
    ss << "timestamp=" << time(nullptr) << "\n";
    (void)WriteTextFile(metaPath, ss.str());
}

/**
 * Очищення shader cache директорії
 */
static void PurgeShaderCacheDir(const std::string& cacheDir) {
    DIR* dir = opendir(cacheDir.c_str());
    if (!dir) return;
    
    int purged = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        
        std::string fullPath = cacheDir + "/" + entry->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                unlink(fullPath.c_str());
                purged++;
            }
        }
    }
    closedir(dir);
    
    if (purged > 0) {
        LOGI("Purged %d stale shader cache files from %s", purged, cacheDir.c_str());
    }
}

} // namespace

bool InitializeVulkan(const VulkanConfig& config) {
    LOGI("╔════════════════════════════════════════════════════════════╗");
    LOGI("║  Initializing Vulkan 1.3 for Adreno 735 (Turnip)          ║");
    LOGI("╚════════════════════════════════════════════════════════════╝");
    
    // Перевірка GPU
    const std::string gpuId = GetGpuIdentity();
    LOGI("GPU Identity: %s", gpuId.c_str());
    
    // Примусове очищення кешу при старті якщо потрібно
    if (config.purge_cache_on_start) {
        LOGI("Purging shader caches on startup...");
        if (config.cache_directory) {
            PurgeShaderCacheDir(config.cache_directory);
        }
    }
    
    if (config.enable_validation) {
        LOGI("Validation layers enabled (debug mode)");
    }
    
    // Логування підтримуваних Adreno розширень
    LOGI("Checking Adreno 735 specific extensions...");
    for (int i = 0; kAdrenoExtensions[i] != nullptr; i++) {
        LOGI("  - %s: requested", kAdrenoExtensions[i]);
    }
    
    return true;
}

VkDevice CreateOptimizedDevice(VkPhysicalDevice physical_device) {
    LOGI("Creating optimized Vulkan device for Adreno 735...");
    
    if (physical_device == VK_NULL_HANDLE) {
        LOGE("Invalid physical device!");
        return VK_NULL_HANDLE;
    }
    
    // Тут буде створення device з оптимальними налаштуваннями
    // для Adreno 735 + Turnip
    
    return VK_NULL_HANDLE;
}

/**
 * Примусове очищення всіх shader кешів
 */
void PurgeAllShaderCaches(const char* base_directory) {
    if (!base_directory) return;
    
    LOGI("Purging all shader caches in %s", base_directory);
    
    std::string shaderDir = std::string(base_directory) + "/shaders";
    std::string pipelineDir = std::string(base_directory) + "/pipeline";
    
    PurgeShaderCacheDir(shaderDir);
    PurgeShaderCacheDir(pipelineDir);
    
    // Видалення meta файлів
    std::string metaPath = std::string(base_directory) + "/pipeline_cache.meta";
    unlink(metaPath.c_str());
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

// ============================================================================
// PS3 RSX Graphics Engine - Multithreaded Implementation
// ============================================================================

// Global RSX engine instance
RSXGraphicsEngine* g_rsx_engine = nullptr;

RSXGraphicsEngine::RSXGraphicsEngine()
    : shutdown_flag_(false), vk_device_(nullptr), vk_queue_(nullptr),
      command_pool_(nullptr), total_commands_(0), total_draws_(0), total_clears_(0) {
    LOGI("RSX Graphics Engine created");
}

RSXGraphicsEngine::~RSXGraphicsEngine() {
    Shutdown();
}

bool RSXGraphicsEngine::Initialize(uint32_t num_threads, VkDevice vk_device, VkQueue vk_queue) {
    vk_device_ = vk_device;
    vk_queue_ = vk_queue;
    shutdown_flag_ = false;
    
    // Create command pool for worker threads
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(vk_device, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan command pool for RSX");
        return false;
    }
    
    // Create worker threads
    for (uint32_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back([this]() { WorkerThreadMain(); });
    }
    
    LOGI("RSX Graphics Engine initialized with %u worker threads", num_threads);
    return true;
}

void RSXGraphicsEngine::WorkerThreadMain() {
    LOGI("RSX worker thread started");
    
    while (!shutdown_flag_) {
        RSXCommand cmd;
        cmd.type = RSXCommand::Type::INVALID;
        
        // Wait for a command
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() { 
                return !command_queue_.empty() || shutdown_flag_; 
            });
            
            if (shutdown_flag_ && command_queue_.empty()) {
                break;
            }
            
            if (!command_queue_.empty()) {
                cmd = command_queue_.front();
                command_queue_.pop();
            }
        }
        
        // Process the command
        if (cmd.type != RSXCommand::Type::INVALID) {
            ProcessCommand(cmd);
            total_commands_++;
        }
    }
    
    LOGI("RSX worker thread exiting");
}

void RSXGraphicsEngine::SubmitCommand(const RSXCommand& cmd) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        command_queue_.push(cmd);
    }
    queue_cv_.notify_one();
}

void RSXGraphicsEngine::SubmitCommands(const RSXCommand* cmds, uint32_t count) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (uint32_t i = 0; i < count; ++i) {
            command_queue_.push(cmds[i]);
        }
    }
    queue_cv_.notify_all();
}

void RSXGraphicsEngine::ProcessCommand(const RSXCommand& cmd) {
    switch (cmd.type) {
        case RSXCommand::Type::DRAW_ARRAYS: {
            total_draws_++;
            // Get draw parameters from command data
            uint32_t first = cmd.data[0];
            uint32_t count = cmd.data[1];
            
            // In real implementation, would record Vulkan draw command
            // vkCmdDraw(cmd_buffer, count, 1, first, 0);
            break;
        }
        
        case RSXCommand::Type::DRAW_INDEXED: {
            total_draws_++;
            uint32_t index_count = cmd.data[0];
            uint32_t index_offset = cmd.data[1];
            int32_t vertex_offset = static_cast<int32_t>(cmd.data[2]);
            
            // In real implementation: vkCmdDrawIndexed(cmd_buffer, index_count, 1, index_offset, vertex_offset, 0);
            break;
        }
        
        case RSXCommand::Type::CLEAR: {
            total_clears_++;
            // Clear render target
            // In real implementation: would record clear commands
            break;
        }
        
        case RSXCommand::Type::SET_VIEWPORT: {
            float x = *reinterpret_cast<const float*>(&cmd.data[0]);
            float y = *reinterpret_cast<const float*>(&cmd.data[1]);
            float width = *reinterpret_cast<const float*>(&cmd.data[2]);
            float height = *reinterpret_cast<const float*>(&cmd.data[3]);
            
            VkViewport viewport{};
            viewport.x = x;
            viewport.y = y;
            viewport.width = width;
            viewport.height = height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            
            // In real implementation: vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
            break;
        }
        
        case RSXCommand::Type::SET_SCISSOR: {
            int32_t x = static_cast<int32_t>(cmd.data[0]);
            int32_t y = static_cast<int32_t>(cmd.data[1]);
            uint32_t width = cmd.data[2];
            uint32_t height = cmd.data[3];
            
            VkRect2D scissor{};
            scissor.offset = {x, y};
            scissor.extent = {width, height};
            
            // In real implementation: vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
            break;
        }
        
        case RSXCommand::Type::SYNC_POINT: {
            // Device synchronization point
            // All subsequent commands wait for GPU to catch up
            break;
        }
        
        case RSXCommand::Type::NOP:
        case RSXCommand::Type::INVALID:
        default:
            // No operation
            break;
    }
}

void RSXGraphicsEngine::SetRenderTarget(const RSXRenderTarget& rt) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    current_rt_ = rt;
}

void RSXGraphicsEngine::Flush() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this]() { return command_queue_.empty(); });
}

void RSXGraphicsEngine::GetGraphicsStats(GraphicsStats* stats) {
    if (!stats) return;
    
    stats->total_commands = total_commands_.load();
    stats->total_draws = total_draws_.load();
    stats->total_clears = total_clears_.load();
    stats->gpu_wait_cycles = 0;  // Would be populated by actual GPU driver
    stats->avg_queue_depth = 0.0;
}

void RSXGraphicsEngine::Shutdown() {
    LOGI("RSX Graphics Engine shutting down");
    
    // Signal shutdown to worker threads
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        shutdown_flag_ = true;
    }
    queue_cv_.notify_all();
    
    // Wait for all worker threads to complete
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Clean up Vulkan resources
    if (command_pool_ && vk_device_) {
        vkDestroyCommandPool(vk_device_, command_pool_, nullptr);
        command_pool_ = nullptr;
    }
    
    LOGI("RSX Graphics Engine shut down complete");
}

// ============================================================================
// Public RSX API
// ============================================================================

bool InitializeRSXEngine(VkDevice vk_device, VkQueue vk_queue, uint32_t num_threads) {
    if (g_rsx_engine) {
        LOGW("RSX Engine already initialized");
        return false;
    }
    
    g_rsx_engine = new RSXGraphicsEngine();
    if (!g_rsx_engine->Initialize(num_threads, vk_device, vk_queue)) {
        delete g_rsx_engine;
        g_rsx_engine = nullptr;
        return false;
    }
    
    LOGI("Global RSX Engine initialized");
    return true;
}

void RSXFlush() {
    if (g_rsx_engine) {
        g_rsx_engine->Flush();
    }
}

void ShutdownRSXEngine() {
    if (g_rsx_engine) {
        g_rsx_engine->Shutdown();
        delete g_rsx_engine;
        g_rsx_engine = nullptr;
        LOGI("Global RSX Engine shut down");
    }
}

} // namespace rpcsx::vulkan
