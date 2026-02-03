/**
 * Adaptive Texture Streaming Cache Implementation
 */

#include "texture_streaming.h"
#include <android/log.h>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <cmath>
#include <chrono>

#define LOG_TAG "RPCSX-TexStream"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::textures {

// Глобальні атомарні змінні
std::atomic<bool> g_streaming_active{false};
std::atomic<uint32_t> g_current_cache_size_mb{0};

// =============================================================================
// Внутрішні структури
// =============================================================================

struct CachedTexture {
    TextureDescriptor descriptor;
    std::vector<uint8_t> data;
    uint64_t last_access_time;
    float distance_to_camera;
    bool is_loading;
};

struct LoadRequest {
    uint64_t texture_id;
    float priority;
    
    bool operator<(const LoadRequest& other) const {
        return priority < other.priority; // Max-heap
    }
};

class TextureStreamingSystem {
public:
    StreamingConfig config;
    StreamingStats stats;
    
    std::unordered_map<uint64_t, CachedTexture> texture_cache;
    std::priority_queue<LoadRequest> load_queue;
    
    std::mutex cache_mutex;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    
    std::vector<std::thread> worker_threads;
    std::atomic<bool> running{false};
    
    uint64_t next_texture_id{1};
    uint64_t frame_time{0};
    
    float camera_x{0}, camera_y{0}, camera_z{0};
    
    TextureLoadCallback load_callback;
    
    bool Initialize(const StreamingConfig& cfg) {
        config = cfg;
        stats = {};
        running = true;
        
        // Запуск робочих потоків
        for (uint32_t i = 0; i < config.async_pool_size; ++i) {
            worker_threads.emplace_back([this]() { WorkerThread(); });
        }
        
        LOGI("╔════════════════════════════════════════════════════════════╗");
        LOGI("║      Adaptive Texture Streaming Cache                      ║");
        LOGI("╠════════════════════════════════════════════════════════════╣");
        LOGI("║  Mode: %-20s                              ║", GetModeName(config.mode));
        LOGI("║  Quality: %-17s                              ║", GetQualityName(config.quality));
        LOGI("║  Max Cache: %4u MB                                        ║", config.max_cache_size_mb);
        LOGI("║  Worker Threads: %u                                        ║", config.async_pool_size);
        LOGI("║  ASTC Compression: %-8s                              ║", config.use_astc_compression ? "Enabled" : "Disabled");
        LOGI("╚════════════════════════════════════════════════════════════╝");
        
        g_streaming_active.store(true);
        return true;
    }
    
    void Shutdown() {
        running = false;
        queue_cv.notify_all();
        
        for (auto& thread : worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads.clear();
        
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            texture_cache.clear();
        }
        
        g_streaming_active.store(false);
        g_current_cache_size_mb.store(0);
        
        LOGI("Texture Streaming System shutdown complete");
    }
    
    uint64_t RegisterTexture(uint32_t width, uint32_t height,
                             uint32_t mip_levels, uint32_t format,
                             const void* initial_data) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        uint64_t id = next_texture_id++;
        
        CachedTexture tex;
        tex.descriptor.id = id;
        tex.descriptor.width = width;
        tex.descriptor.height = height;
        tex.descriptor.mip_levels = mip_levels;
        tex.descriptor.format = format;
        tex.descriptor.priority = 0.5f;
        tex.descriptor.is_resident = false;
        tex.descriptor.current_mip = mip_levels; // Найнижча якість
        tex.last_access_time = frame_time;
        tex.distance_to_camera = 1000.0f;
        tex.is_loading = false;
        
        if (initial_data) {
            // Копіюємо початкові дані
            size_t data_size = CalculateTextureSize(width, height, mip_levels, format);
            tex.data.resize(data_size);
            memcpy(tex.data.data(), initial_data, data_size);
            tex.descriptor.is_resident = true;
            tex.descriptor.current_mip = 0;
            
            stats.bytes_cached += data_size;
            UpdateCacheSize();
        }
        
        texture_cache[id] = std::move(tex);
        stats.textures_loaded++;
        
        return id;
    }
    
    void UnregisterTexture(uint64_t id) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        auto it = texture_cache.find(id);
        if (it != texture_cache.end()) {
            stats.bytes_cached -= it->second.data.size();
            texture_cache.erase(it);
            UpdateCacheSize();
        }
    }
    
    void RequestLoad(uint64_t id, float priority) {
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto it = texture_cache.find(id);
            if (it == texture_cache.end() || it->second.is_loading) {
                return;
            }
            it->second.is_loading = true;
            it->second.descriptor.priority = priority;
        }
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            load_queue.push({id, priority});
            stats.pending_loads++;
        }
        queue_cv.notify_one();
    }
    
    void Update(float cx, float cy, float cz) {
        camera_x = cx;
        camera_y = cy;
        camera_z = cz;
        frame_time++;
        
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        // Оновлення відстаней та пріоритетів
        for (auto& [id, tex] : texture_cache) {
            float dx = tex.descriptor.gpu_address - cx; // Спрощено
            float dy = 0;
            float dz = 0;
            tex.distance_to_camera = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            // Автоматичний запит на завантаження близьких текстур
            if (tex.distance_to_camera < config.prefetch_distance && 
                !tex.descriptor.is_resident && !tex.is_loading) {
                float priority = 1.0f - (tex.distance_to_camera / config.prefetch_distance);
                RequestLoad(id, priority);
            }
        }
        
        // Евікція старих текстур якщо кеш переповнений
        EvictIfNeeded();
        
        // Оновлення статистики
        if (stats.cache_hits + stats.cache_misses > 0) {
            stats.cache_hit_ratio = static_cast<float>(stats.cache_hits) / 
                                    (stats.cache_hits + stats.cache_misses);
        }
    }
    
    void ClearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        for (auto& [id, tex] : texture_cache) {
            tex.data.clear();
            tex.descriptor.is_resident = false;
            tex.descriptor.current_mip = tex.descriptor.mip_levels;
        }
        
        stats.bytes_cached = 0;
        g_current_cache_size_mb.store(0);
        
        LOGI("Texture cache cleared");
    }
    
private:
    void WorkerThread() {
        while (running) {
            LoadRequest request;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this]() {
                    return !running || !load_queue.empty();
                });
                
                if (!running) break;
                if (load_queue.empty()) continue;
                
                request = load_queue.top();
                load_queue.pop();
            }
            
            auto start_time = std::chrono::steady_clock::now();
            
            // Симуляція завантаження текстури
            bool success = LoadTexture(request.texture_id);
            
            auto end_time = std::chrono::steady_clock::now();
            float load_time = std::chrono::duration<float, std::milli>(
                end_time - start_time).count();
            
            // Оновлення статистики
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                stats.pending_loads--;
                stats.textures_streamed++;
                
                // Ковзне середнє часу завантаження
                stats.average_load_time_ms = stats.average_load_time_ms * 0.9f + 
                                             load_time * 0.1f;
            }
            
            if (load_callback) {
                load_callback(request.texture_id, success);
            }
        }
    }
    
    bool LoadTexture(uint64_t id) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        auto it = texture_cache.find(id);
        if (it == texture_cache.end()) {
            return false;
        }
        
        auto& tex = it->second;
        
        // Симуляція завантаження - генерація даних
        size_t data_size = CalculateTextureSize(
            tex.descriptor.width, tex.descriptor.height,
            tex.descriptor.mip_levels, tex.descriptor.format);
        
        // Застосування якості
        uint32_t target_mip = GetTargetMipLevel(tex.descriptor);
        size_t actual_size = CalculateMipSize(
            tex.descriptor.width, tex.descriptor.height,
            target_mip, tex.descriptor.format);
        
        tex.data.resize(actual_size);
        // В реальній реалізації тут було б читання з диску/мережі
        
        tex.descriptor.is_resident = true;
        tex.descriptor.current_mip = target_mip;
        tex.is_loading = false;
        tex.last_access_time = frame_time;
        
        stats.bytes_streamed += actual_size;
        stats.bytes_cached += actual_size;
        UpdateCacheSize();
        
        return true;
    }
    
    void EvictIfNeeded() {
        uint32_t max_bytes = config.max_cache_size_mb * 1024 * 1024;
        
        while (stats.bytes_cached > max_bytes) {
            // Знаходимо найменш пріоритетну текстуру
            uint64_t evict_id = 0;
            float min_priority = 999999.0f;
            uint64_t oldest_access = frame_time;
            
            for (const auto& [id, tex] : texture_cache) {
                if (!tex.descriptor.is_resident) continue;
                
                // Пріоритет = близькість + недавність використання
                float priority = tex.descriptor.priority - 
                                (frame_time - tex.last_access_time) * 0.001f;
                
                if (priority < min_priority) {
                    min_priority = priority;
                    evict_id = id;
                }
            }
            
            if (evict_id == 0) break;
            
            auto& tex = texture_cache[evict_id];
            stats.bytes_cached -= tex.data.size();
            tex.data.clear();
            tex.data.shrink_to_fit();
            tex.descriptor.is_resident = false;
            tex.descriptor.current_mip = tex.descriptor.mip_levels;
            
            LOGI("Evicted texture %llu (priority %.2f)", 
                 (unsigned long long)evict_id, min_priority);
        }
    }
    
    void UpdateCacheSize() {
        g_current_cache_size_mb.store(stats.bytes_cached / (1024 * 1024));
        stats.current_cache_size_mb = g_current_cache_size_mb.load();
    }
    
    uint32_t GetTargetMipLevel(const TextureDescriptor& desc) {
        switch (config.quality) {
            case TextureQuality::ULTRA: return 0;
            case TextureQuality::HIGH: return std::min(1u, desc.mip_levels - 1);
            case TextureQuality::MEDIUM: return std::min(2u, desc.mip_levels - 1);
            case TextureQuality::LOW: return std::min(3u, desc.mip_levels - 1);
            case TextureQuality::POTATO: return desc.mip_levels - 1;
            default: return 0;
        }
    }
    
    size_t CalculateTextureSize(uint32_t w, uint32_t h, uint32_t mips, uint32_t fmt) {
        size_t size = 0;
        for (uint32_t i = 0; i < mips; ++i) {
            size += CalculateMipSize(w, h, i, fmt);
        }
        return size;
    }
    
    size_t CalculateMipSize(uint32_t w, uint32_t h, uint32_t mip, uint32_t fmt) {
        uint32_t mw = std::max(1u, w >> mip);
        uint32_t mh = std::max(1u, h >> mip);
        
        // ASTC 4x4 = 1 byte per texel, RGBA = 4 bytes
        size_t bpp = config.use_astc_compression ? 1 : 4;
        return mw * mh * bpp;
    }
    
    const char* GetModeName(StreamingMode mode) {
        switch (mode) {
            case StreamingMode::DISABLED: return "Disabled";
            case StreamingMode::CONSERVATIVE: return "Conservative";
            case StreamingMode::BALANCED: return "Balanced";
            case StreamingMode::AGGRESSIVE: return "Aggressive";
            case StreamingMode::ULTRA_LOW_MEM: return "Ultra Low Memory";
            default: return "Unknown";
        }
    }
    
    const char* GetQualityName(TextureQuality q) {
        switch (q) {
            case TextureQuality::ULTRA: return "Ultra";
            case TextureQuality::HIGH: return "High";
            case TextureQuality::MEDIUM: return "Medium";
            case TextureQuality::LOW: return "Low";
            case TextureQuality::POTATO: return "Potato";
            default: return "Unknown";
        }
    }
};

static TextureStreamingSystem g_system;

// =============================================================================
// API Implementation
// =============================================================================

bool InitializeTextureStreaming(const StreamingConfig& config) {
    return g_system.Initialize(config);
}

void ShutdownTextureStreaming() {
    g_system.Shutdown();
}

bool IsStreamingActive() {
    return g_streaming_active.load();
}

void SetStreamingMode(StreamingMode mode) {
    g_system.config.mode = mode;
    LOGI("Streaming mode set to: %d", static_cast<int>(mode));
}

StreamingMode GetStreamingMode() {
    return g_system.config.mode;
}

void SetTextureQuality(TextureQuality quality) {
    g_system.config.quality = quality;
    LOGI("Texture quality set to: %d", static_cast<int>(quality));
}

TextureQuality GetTextureQuality() {
    return g_system.config.quality;
}

void SetMaxCacheSize(uint32_t size_mb) {
    g_system.config.max_cache_size_mb = size_mb;
    LOGI("Max cache size set to: %u MB", size_mb);
}

uint32_t GetMaxCacheSize() {
    return g_system.config.max_cache_size_mb;
}

uint64_t RegisterTexture(uint32_t width, uint32_t height,
                         uint32_t mip_levels, uint32_t format,
                         const void* initial_data) {
    return g_system.RegisterTexture(width, height, mip_levels, format, initial_data);
}

void UnregisterTexture(uint64_t texture_id) {
    g_system.UnregisterTexture(texture_id);
}

void RequestTextureLoad(uint64_t texture_id, float priority) {
    g_system.RequestLoad(texture_id, priority);
}

void SetTexturePriority(uint64_t texture_id, float priority) {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    auto it = g_system.texture_cache.find(texture_id);
    if (it != g_system.texture_cache.end()) {
        it->second.descriptor.priority = priority;
    }
}

void UpdateStreaming(float camera_x, float camera_y, float camera_z) {
    g_system.Update(camera_x, camera_y, camera_z);
}

void FlushPendingLoads() {
    // Чекаємо завершення всіх завантажень
    while (g_system.stats.pending_loads > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ClearCache() {
    g_system.ClearCache();
}

void PrefetchZone(float x, float y, float z, float radius) {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    
    for (auto& [id, tex] : g_system.texture_cache) {
        // Спрощена перевірка - в реальності потрібні просторові координати
        if (!tex.descriptor.is_resident && !tex.is_loading) {
            g_system.RequestLoad(id, 0.8f);
        }
    }
}

void GetStreamingStats(StreamingStats* stats) {
    if (stats) {
        std::lock_guard<std::mutex> lock(g_system.cache_mutex);
        *stats = g_system.stats;
    }
}

void ResetStreamingStats() {
    std::lock_guard<std::mutex> lock(g_system.cache_mutex);
    auto cached = g_system.stats.bytes_cached;
    auto cache_size = g_system.stats.current_cache_size_mb;
    g_system.stats = {};
    g_system.stats.bytes_cached = cached;
    g_system.stats.current_cache_size_mb = cache_size;
}

StreamingConfig GetStreamingConfig() {
    return g_system.config;
}

void SetStreamingConfig(const StreamingConfig& config) {
    g_system.config = config;
}

void SetTextureLoadCallback(TextureLoadCallback callback) {
    g_system.load_callback = std::move(callback);
}

} // namespace rpcsx::textures
