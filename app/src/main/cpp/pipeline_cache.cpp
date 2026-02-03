/**
 * Vulkan Pipeline State Object (PSO) Caching Implementation
 */

#include "pipeline_cache.h"
#include <android/log.h>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <chrono>
#include <cstring>
#include <vector>
#include <cstdint>

#define LOG_TAG "RPCSX-Pipeline"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::pipeline {

// Глобальні атомарні
std::atomic<bool> g_cache_active{false};
std::atomic<uint32_t> g_pipelines_in_cache{0};
std::atomic<uint32_t> g_pending_compilations{0};

// =============================================================================
// Hash функції
// =============================================================================

static uint64_t HashBytes(const void* data, size_t size) {
    // FNV-1a hash
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

uint64_t GraphicsPipelineDesc::CalculateHash() const {
    return HashBytes(this, sizeof(*this));
}

uint64_t ComputePipelineDesc::CalculateHash() const {
    return HashBytes(this, sizeof(*this));
}

// =============================================================================
// Внутрішні структури
// =============================================================================

struct CachedPipeline {
    PipelineHandle handle;
    PipelineType type;
    PipelineState state;
    void* vk_pipeline;  // VkPipeline
    uint64_t hash;
    uint64_t last_access_time;
    uint64_t creation_time_ms;
};

struct ShaderModule {
    uint64_t hash;
    void* vk_module;    // VkShaderModule
    uint32_t stage;
    std::vector<uint8_t> spirv;
};

struct CompileRequest {
    uint64_t hash;
    PipelineType type;
    std::vector<uint8_t> desc_data;
    float priority;
    
    bool operator<(const CompileRequest& other) const {
        return priority < other.priority;
    }
};

class PipelineCacheSystem {
public:
    PipelineCacheConfig config;
    PipelineCacheStats stats;
    
    void* vk_device = nullptr;
    void* vk_physical_device = nullptr;
    void* vk_pipeline_cache = nullptr;
    
    std::unordered_map<uint64_t, CachedPipeline> pipeline_cache;
    std::unordered_map<uint64_t, ShaderModule> shader_modules;
    std::unordered_map<uint64_t, void*> render_passes;
    
    std::priority_queue<CompileRequest> compile_queue;
    
    std::mutex cache_mutex;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    
    std::vector<std::thread> compile_threads;
    std::atomic<bool> running{false};
    
    uint64_t next_handle{1};
    uint64_t frame_time{0};
    
    CompileCallback compile_callback;
    
    bool Initialize(void* device, void* phys_device, const PipelineCacheConfig& cfg) {
        vk_device = device;
        vk_physical_device = phys_device;
        config = cfg;
        stats = {};
        running = true;
        
        // Створення Vulkan Pipeline Cache
        // В реальній реалізації тут був би виклик vkCreatePipelineCache
        vk_pipeline_cache = nullptr; // Placeholder
        
        // Спроба завантаження кешу з диску
        if (!config.cache_path.empty()) {
            LoadCacheFromDisk(config.cache_path.c_str());
        }
        
        // Запуск потоків компіляції
        for (uint32_t i = 0; i < config.compile_threads; ++i) {
            compile_threads.emplace_back([this]() { CompileThread(); });
        }
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║      Vulkan Pipeline State Object Cache                    ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Max Pipelines: %5u                                       ║", config.max_cached_pipelines);
        LOGI("║  VK Cache Size: %3zu MB                                     ║", config.vk_cache_size / (1024*1024));
        LOGI("║  Compile Threads: %u                                        ║", config.compile_threads);
        LOGI("║  Pipeline Library: %-8s                              ║", config.use_pipeline_library ? "Enabled" : "Disabled");
        LOGI("║  Precompilation: %-10s                              ║", config.enable_precompilation ? "Enabled" : "Disabled");
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        g_cache_active.store(true);
        return true;
    }
    
    void Shutdown() {
        running = false;
        queue_cv.notify_all();
        
        for (auto& thread : compile_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        compile_threads.clear();
        
        // Збереження кешу
        if (config.persist_to_disk && !config.cache_path.empty()) {
            SaveCacheToDisk(config.cache_path.c_str());
        }
        
        // Очищення pipelines
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            // В реальності тут були б виклики vkDestroyPipeline
            pipeline_cache.clear();
            shader_modules.clear();
            render_passes.clear();
        }
        
        // Знищення VkPipelineCache
        // vkDestroyPipelineCache(...)
        
        g_cache_active.store(false);
        g_pipelines_in_cache.store(0);
        
        LOGI("Pipeline Cache shutdown complete");
        LOGI("Stats: Created=%llu, Hits=%llu, Misses=%llu, HitRatio=%.1f%%",
             (unsigned long long)stats.total_pipelines_created,
             (unsigned long long)stats.cache_hits,
             (unsigned long long)stats.cache_misses,
             stats.cache_hit_ratio * 100.0f);
    }
    
    PipelineHandle GetOrCreateGraphics(const GraphicsPipelineDesc& desc) {
        uint64_t hash = desc.CalculateHash();
        
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            
            auto it = pipeline_cache.find(hash);
            if (it != pipeline_cache.end()) {
                it->second.last_access_time = frame_time;
                stats.cache_hits++;
                UpdateHitRatio();
                return it->second.handle;
            }
        }
        
        stats.cache_misses++;
        UpdateHitRatio();
        
        // Створення нового pipeline
        return CreateGraphicsPipeline(desc, hash);
    }
    
    PipelineHandle GetOrCreateCompute(const ComputePipelineDesc& desc) {
        uint64_t hash = desc.CalculateHash();
        
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            
            auto it = pipeline_cache.find(hash);
            if (it != pipeline_cache.end()) {
                it->second.last_access_time = frame_time;
                stats.cache_hits++;
                UpdateHitRatio();
                return it->second.handle;
            }
        }
        
        stats.cache_misses++;
        UpdateHitRatio();
        
        return CreateComputePipeline(desc, hash);
    }
    
    void RequestPrecompileGraphics(const GraphicsPipelineDesc& desc) {
        if (!config.enable_precompilation) return;
        
        uint64_t hash = desc.CalculateHash();
        
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            if (pipeline_cache.find(hash) != pipeline_cache.end()) {
                return; // Already exists
            }
        }
        
        CompileRequest req;
        req.hash = hash;
        req.type = PipelineType::GRAPHICS;
        req.desc_data.resize(sizeof(desc));
        memcpy(req.desc_data.data(), &desc, sizeof(desc));
        req.priority = 0.5f;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            compile_queue.push(req);
            g_pending_compilations++;
            stats.pending_compilations++;
        }
        queue_cv.notify_one();
    }
    
    PipelineState GetState(PipelineHandle handle) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        for (const auto& [hash, pipeline] : pipeline_cache) {
            if (pipeline.handle == handle) {
                return pipeline.state;
            }
        }
        return PipelineState::NOT_FOUND;
    }
    
    void* GetVkPipeline(PipelineHandle handle) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        for (const auto& [hash, pipeline] : pipeline_cache) {
            if (pipeline.handle == handle && pipeline.state == PipelineState::READY) {
                return pipeline.vk_pipeline;
            }
        }
        return nullptr;
    }
    
    uint64_t RegisterShader(const void* spirv, size_t size, uint32_t stage) {
        uint64_t hash = HashBytes(spirv, size);
        
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        if (shader_modules.find(hash) != shader_modules.end()) {
            return hash;
        }
        
        ShaderModule module;
        module.hash = hash;
        module.stage = stage;
        module.spirv.resize(size);
        memcpy(module.spirv.data(), spirv, size);
        // В реальності: vkCreateShaderModule
        module.vk_module = nullptr;
        
        shader_modules[hash] = std::move(module);
        return hash;
    }
    
    uint64_t RegisterRenderPass(void* vk_rp) {
        uint64_t hash = reinterpret_cast<uint64_t>(vk_rp);
        
        std::lock_guard<std::mutex> lock(cache_mutex);
        render_passes[hash] = vk_rp;
        return hash;
    }
    
    void Update() {
        frame_time++;
        
        // Перевірка на необхідність eviction
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        if (pipeline_cache.size() > config.max_cached_pipelines) {
            EvictOldest();
        }
        
        g_pipelines_in_cache.store(pipeline_cache.size());
        stats.pipelines_in_cache = pipeline_cache.size();
    }
    
    bool SaveCache(const char* path) {
        std::string filepath = path ? path : config.cache_path;
        if (filepath.empty()) return false;
        
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            LOGE("Failed to open cache file for writing: %s", filepath.c_str());
            return false;
        }
        
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        // Записуємо кількість pipelines
        uint32_t count = pipeline_cache.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        // Записуємо кожен pipeline
        for (const auto& [hash, pipeline] : pipeline_cache) {
            file.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
            file.write(reinterpret_cast<const char*>(&pipeline.type), sizeof(pipeline.type));
        }
        
        stats.disk_cache_size_bytes = file.tellp();
        
        LOGI("Saved %u pipelines to cache: %s", count, filepath.c_str());
        return true;
    }
    
    bool LoadCache(const char* path) {
        std::string filepath = path ? path : config.cache_path;
        if (filepath.empty()) return false;
        
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            LOGI("No cache file found: %s", filepath.c_str());
            return false;
        }
        
        uint32_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        
        LOGI("Loading %u pipelines from cache: %s", count, filepath.c_str());
        
        // В реальності тут було б відновлення pipelines
        // Поки що просто читаємо дані
        for (uint32_t i = 0; i < count; ++i) {
            uint64_t hash;
            PipelineType type;
            file.read(reinterpret_cast<char*>(&hash), sizeof(hash));
            file.read(reinterpret_cast<char*>(&type), sizeof(type));
        }
        
        return true;
    }
    
    void ClearAll() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        stats.pipelines_evicted += pipeline_cache.size();
        pipeline_cache.clear();
        
        g_pipelines_in_cache.store(0);
        
        LOGI("Pipeline cache cleared");
    }
    
private:
    PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, uint64_t hash) {
        auto start = std::chrono::steady_clock::now();
        
        CachedPipeline pipeline;
        pipeline.handle = next_handle++;
        pipeline.type = PipelineType::GRAPHICS;
        pipeline.hash = hash;
        pipeline.last_access_time = frame_time;
        
        // В реальності тут був би виклик vkCreateGraphicsPipelines
        // Симуляція компіляції
        pipeline.vk_pipeline = reinterpret_cast<void*>(hash); // Placeholder
        pipeline.state = PipelineState::READY;
        
        auto end = std::chrono::steady_clock::now();
        pipeline.creation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            pipeline_cache[hash] = pipeline;
            
            stats.total_pipelines_created++;
            stats.pipelines_compiled++;
            stats.compile_time_total_ms += pipeline.creation_time_ms;
            stats.average_compile_time_ms = static_cast<float>(stats.compile_time_total_ms) / 
                                            stats.pipelines_compiled;
            
            g_pipelines_in_cache.store(pipeline_cache.size());
        }
        
        return pipeline.handle;
    }
    
    PipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc, uint64_t hash) {
        auto start = std::chrono::steady_clock::now();
        
        CachedPipeline pipeline;
        pipeline.handle = next_handle++;
        pipeline.type = PipelineType::COMPUTE;
        pipeline.hash = hash;
        pipeline.last_access_time = frame_time;
        
        // vkCreateComputePipelines
        pipeline.vk_pipeline = reinterpret_cast<void*>(hash);
        pipeline.state = PipelineState::READY;
        
        auto end = std::chrono::steady_clock::now();
        pipeline.creation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            pipeline_cache[hash] = pipeline;
            
            stats.total_pipelines_created++;
            stats.pipelines_compiled++;
            stats.compile_time_total_ms += pipeline.creation_time_ms;
            
            g_pipelines_in_cache.store(pipeline_cache.size());
        }
        
        return pipeline.handle;
    }
    
    void CompileThread() {
        while (running) {
            CompileRequest request;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this]() {
                    return !running || !compile_queue.empty();
                });
                
                if (!running) break;
                if (compile_queue.empty()) continue;
                
                request = compile_queue.top();
                compile_queue.pop();
                g_pending_compilations--;
            }
            
            // Компіляція
            bool success = false;
            
            if (request.type == PipelineType::GRAPHICS) {
                GraphicsPipelineDesc desc;
                memcpy(&desc, request.desc_data.data(), sizeof(desc));
                PipelineHandle handle = CreateGraphicsPipeline(desc, request.hash);
                success = (handle != INVALID_PIPELINE);
                
                if (compile_callback) {
                    compile_callback(handle, success);
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                stats.pending_compilations = g_pending_compilations.load();
            }
        }
    }
    
    void EvictOldest() {
        if (pipeline_cache.empty()) return;
        
        uint64_t oldest_hash = 0;
        uint64_t oldest_time = UINT64_MAX;
        
        for (const auto& [hash, pipeline] : pipeline_cache) {
            if (pipeline.last_access_time < oldest_time) {
                oldest_time = pipeline.last_access_time;
                oldest_hash = hash;
            }
        }
        
        if (oldest_hash != 0) {
            // В реальності: vkDestroyPipeline
            pipeline_cache.erase(oldest_hash);
            stats.pipelines_evicted++;
        }
    }
    
    void UpdateHitRatio() {
        uint64_t total = stats.cache_hits + stats.cache_misses;
        if (total > 0) {
            stats.cache_hit_ratio = static_cast<float>(stats.cache_hits) / total;
        }
    }
};

static PipelineCacheSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializePipelineCache(void* vk_device, void* vk_physical_device,
                             const PipelineCacheConfig& config) {
    return g_system.Initialize(vk_device, vk_physical_device, config);
}

void ShutdownPipelineCache() {
    g_system.Shutdown();
}

bool IsCacheActive() {
    return g_cache_active.load();
}

PipelineHandle GetOrCreateGraphicsPipeline(const GraphicsPipelineDesc& desc) {
    return g_system.GetOrCreateGraphics(desc);
}

PipelineHandle GetOrCreateComputePipeline(const ComputePipelineDesc& desc) {
    return g_system.GetOrCreateCompute(desc);
}

PipelineState GetPipelineState(PipelineHandle handle) {
    return g_system.GetState(handle);
}

void* GetVkPipeline(PipelineHandle handle) {
    return g_system.GetVkPipeline(handle);
}

void RequestPrecompile(const GraphicsPipelineDesc& desc) {
    g_system.RequestPrecompileGraphics(desc);
}

void RequestPrecompile(const ComputePipelineDesc& desc) {
    // Similar implementation for compute
}

uint64_t RegisterShaderModule(const void* spirv_code, size_t code_size, uint32_t stage) {
    return g_system.RegisterShader(spirv_code, code_size, stage);
}

void UnregisterShaderModule(uint64_t shader_hash) {
    // Remove from shader_modules map
}

uint64_t RegisterRenderPass(void* vk_render_pass) {
    return g_system.RegisterRenderPass(vk_render_pass);
}

void UnregisterRenderPass(uint64_t render_pass_hash) {
    // Remove from render_passes map
}

void FlushCompilations() {
    while (g_pending_compilations.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ClearCache() {
    g_system.ClearAll();
}

bool SaveCacheToDisk(const char* path) {
    return g_system.SaveCache(path);
}

bool LoadCacheFromDisk(const char* path) {
    return g_system.LoadCache(path);
}

void SetCompileCallback(CompileCallback callback) {
    g_system.compile_callback = std::move(callback);
}

void Update() {
    g_system.Update();
}

void GetStats(PipelineCacheStats* stats) {
    if (stats) {
        std::lock_guard<std::mutex> lock(g_system.cache_mutex);
        *stats = g_system.stats;
    }
}

void ResetStats() {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    auto pipelines = g_system.stats.pipelines_in_cache;
    g_system.stats = {};
    g_system.stats.pipelines_in_cache = pipelines;
}

void SetConfig(const PipelineCacheConfig& config) {
    g_system.config = config;
}

PipelineCacheConfig GetConfig() {
    return g_system.config;
}

void* GetVkPipelineCache() {
    return g_system.vk_pipeline_cache;
}

} // namespace rpcsx::pipeline
