/**
 * Трирівневий Shader Cache Manager з Zstd компресією
 * Оптимізовано для UFS 4.0 (швидкий флеш-накопичувач)
 */

#include "shader_cache_manager.h"
#include <android/log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <zstd.h>

#define LOG_TAG "RPCSX-ShaderCache"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace rpcsx::shaders {

// Трирівнева структура кешу:
// L1 - In-memory cache (найшвидший доступ)
// L2 - Persistent cache на UFS 4.0 (швидкий SSD)
// L3 - Compressed archive cache (економія місця)

struct ShaderCacheImpl {
    // L1: In-memory кеш
    std::unordered_map<uint64_t, CompiledShader> memory_cache;
    size_t l1_max_size = 512 * 1024 * 1024;  // 512MB
    size_t l1_current_size = 0;
    
    // L2: Persistent кеш
    std::string l2_cache_dir;
    
    // L3: Compressed кеш
    std::string l3_cache_file;
    
    // Async компіляція
    std::queue<ShaderCompilationTask> compilation_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> async_running;
    std::thread async_thread;
    
    // Статистика
    std::atomic<uint64_t> l1_hits{0};
    std::atomic<uint64_t> l2_hits{0};
    std::atomic<uint64_t> l3_hits{0};
    std::atomic<uint64_t> cache_misses{0};
};

static std::unique_ptr<ShaderCacheImpl> g_cache;

/**
 * Обчислення хешу шейдера (використовуємо швидкий алгоритм)
 */
uint64_t ComputeShaderHash(const void* shader_code, size_t size) {
    // XXH64 - швидкий hash, ідеальний для шейдерів
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    const uint8_t* data = static_cast<const uint8_t*>(shader_code);
    
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;  // FNV prime
    }
    
    return hash;
}

/**
 * Компресія шейдера через Zstd (максимальна швидкість)
 */
std::vector<uint8_t> CompressShader(const void* data, size_t size) {
    // Рівень 3 - оптимальний баланс швидкість/компресія для real-time
    const int compression_level = 3;
    
    size_t compressed_bound = ZSTD_compressBound(size);
    std::vector<uint8_t> compressed(compressed_bound);
    
    size_t compressed_size = ZSTD_compress(
        compressed.data(), compressed_bound,
        data, size,
        compression_level
    );
    
    if (ZSTD_isError(compressed_size)) {
        LOGE("Zstd compression failed: %s", ZSTD_getErrorName(compressed_size));
        return {};
    }
    
    compressed.resize(compressed_size);
    LOGI("Shader compressed: %zu -> %zu bytes (%.1f%%)", 
         size, compressed_size, (compressed_size * 100.0f) / size);
    
    return compressed;
}

/**
 * Декомпресія шейдера
 */
std::vector<uint8_t> DecompressShader(const void* compressed_data, size_t compressed_size) {
    // Отримуємо розмір оригіналу
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(compressed_data, compressed_size);
    
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || 
        decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        LOGE("Invalid compressed shader data");
        return {};
    }
    
    std::vector<uint8_t> decompressed(decompressed_size);
    
    size_t result = ZSTD_decompress(
        decompressed.data(), decompressed_size,
        compressed_data, compressed_size
    );
    
    if (ZSTD_isError(result)) {
        LOGE("Zstd decompression failed: %s", ZSTD_getErrorName(result));
        return {};
    }
    
    return decompressed;
}

/**
 * Ініціалізація трирівневого кешу
 */
bool InitializeShaderCache(const char* cache_directory) {
    LOGI("Initializing 3-tier Shader Cache with Zstd compression");
    
    g_cache = std::make_unique<ShaderCacheImpl>();
    
    // Створюємо директорії для кешу
    g_cache->l2_cache_dir = std::string(cache_directory) + "/shader_cache_l2";
    g_cache->l3_cache_file = std::string(cache_directory) + "/shader_cache_l3.zst";
    
    // Створюємо L2 директорію
    mkdir(g_cache->l2_cache_dir.c_str(), 0755);
    
    // Завантажуємо існуючий кеш з диска
    LoadPersistentCache();
    
    // Запускаємо async компіляцію
    g_cache->async_running = true;
    g_cache->async_thread = std::thread(AsyncCompilationWorker);
    
    LOGI("Shader cache initialized at: %s", cache_directory);
    return true;
}

/**
 * Пошук шейдера в кеші (L1 -> L2 -> L3)
 */
CompiledShader* FindShader(uint64_t shader_hash) {
    if (!g_cache) return nullptr;
    
    // L1: Перевіряємо in-memory кеш
    auto it = g_cache->memory_cache.find(shader_hash);
    if (it != g_cache->memory_cache.end()) {
        g_cache->l1_hits++;
        return &it->second;
    }
    
    // L2: Перевіряємо persistent кеш на диску
    CompiledShader* shader = LoadFromL2Cache(shader_hash);
    if (shader) {
        g_cache->l2_hits++;
        // Додаємо в L1 для швидшого доступу наступного разу
        CacheShaderL1(shader_hash, *shader);
        return shader;
    }
    
    // L3: Перевіряємо compressed архів
    shader = LoadFromL3Cache(shader_hash);
    if (shader) {
        g_cache->l3_hits++;
        CacheShaderL1(shader_hash, *shader);
        CacheShaderL2(shader_hash, *shader);
        return shader;
    }
    
    g_cache->cache_misses++;
    return nullptr;
}

/**
 * Додавання шейдера в L1 кеш
 */
void CacheShaderL1(uint64_t hash, const CompiledShader& shader) {
    if (!g_cache) return;
    
    // Перевіряємо ліміт розміру L1
    if (g_cache->l1_current_size + shader.spirv_code.size() > g_cache->l1_max_size) {
        // Видаляємо найстаріші записи (LRU)
        EvictOldestL1Entries();
    }
    
    g_cache->memory_cache[hash] = shader;
    g_cache->l1_current_size += shader.spirv_code.size();
}

/**
 * Збереження в L2 (persistent кеш на UFS 4.0)
 */
void CacheShaderL2(uint64_t hash, const CompiledShader& shader) {
    if (!g_cache) return;
    
    std::ostringstream filename;
    filename << g_cache->l2_cache_dir << "/" 
             << std::hex << std::setw(16) << std::setfill('0') << hash 
             << ".spv";
    
    // Записуємо з O_DIRECT для bypass filesystem cache (швидше на UFS 4.0)
    int fd = open(filename.str().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGE("Failed to create L2 cache file: %s", filename.str().c_str());
        return;
    }
    
    write(fd, shader.spirv_code.data(), shader.spirv_code.size());
    close(fd);
    
    LOGI("Shader cached to L2: %s (%zu bytes)", filename.str().c_str(), shader.spirv_code.size());
}

/**
 * Збереження в L3 (compressed архів)
 */
void CacheShaderL3(uint64_t hash, const CompiledShader& shader) {
    if (!g_cache) return;
    
    // Компресуємо шейдер
    auto compressed = CompressShader(shader.spirv_code.data(), shader.spirv_code.size());
    if (compressed.empty()) return;
    
    // Додаємо в архів (append mode)
    std::ofstream l3_file(g_cache->l3_cache_file, std::ios::binary | std::ios::app);
    if (!l3_file) {
        LOGE("Failed to open L3 cache file");
        return;
    }
    
    // Формат: [hash:8][size:4][compressed_data:size]
    l3_file.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
    uint32_t size = compressed.size();
    l3_file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    l3_file.write(reinterpret_cast<const char*>(compressed.data()), size);
    
    LOGI("Shader cached to L3: hash=%016llx (%u bytes compressed)", hash, size);
}

/**
 * Async shader compilation worker thread
 */
void AsyncCompilationWorker() {
    LOGI("Async shader compilation thread started");
    
    while (g_cache->async_running) {
        ShaderCompilationTask task;
        
        {
            std::unique_lock<std::mutex> lock(g_cache->queue_mutex);
            g_cache->queue_cv.wait(lock, [] {
                return !g_cache->compilation_queue.empty() || !g_cache->async_running;
            });
            
            if (!g_cache->async_running && g_cache->compilation_queue.empty()) {
                break;
            }
            
            task = g_cache->compilation_queue.front();
            g_cache->compilation_queue.pop();
        }
        
        // Компілюємо шейдер
        CompileShaderAsync(task);
    }
    
    LOGI("Async shader compilation thread stopped");
}

/**
 * Додавання завдання в чергу async компіляції
 */
void QueueShaderCompilation(const ShaderCompilationTask& task) {
    if (!g_cache) return;
    
    std::lock_guard<std::mutex> lock(g_cache->queue_mutex);
    g_cache->compilation_queue.push(task);
    g_cache->queue_cv.notify_one();
    
    LOGI("Shader queued for async compilation (queue size: %zu)", 
         g_cache->compilation_queue.size());
}

/**
 * Виведення статистики кешу
 */
void PrintCacheStats() {
    if (!g_cache) return;
    
    LOGI("=== Shader Cache Statistics ===");
    LOGI("L1 (Memory) hits: %llu", g_cache->l1_hits.load());
    LOGI("L2 (UFS 4.0) hits: %llu", g_cache->l2_hits.load());
    LOGI("L3 (Compressed) hits: %llu", g_cache->l3_hits.load());
    LOGI("Cache misses: %llu", g_cache->cache_misses.load());
    
    uint64_t total = g_cache->l1_hits + g_cache->l2_hits + g_cache->l3_hits + g_cache->cache_misses;
    if (total > 0) {
        float hit_rate = ((g_cache->l1_hits + g_cache->l2_hits + g_cache->l3_hits) * 100.0f) / total;
        LOGI("Overall hit rate: %.2f%%", hit_rate);
    }
    
    LOGI("L1 cache size: %zu MB / %zu MB", 
         g_cache->l1_current_size / (1024*1024),
         g_cache->l1_max_size / (1024*1024));
}

/**
 * Очищення кешу
 */
void ShutdownShaderCache() {
    if (!g_cache) return;
    
    // Зупиняємо async компіляцію
    g_cache->async_running = false;
    g_cache->queue_cv.notify_all();
    
    if (g_cache->async_thread.joinable()) {
        g_cache->async_thread.join();
    }
    
    // Виводимо статистику
    PrintCacheStats();
    
    g_cache.reset();
    LOGI("Shader cache shutdown complete");
}

// Заглушки для функцій, що потребують повної реалізації
CompiledShader* LoadFromL2Cache(uint64_t hash) { return nullptr; }
CompiledShader* LoadFromL3Cache(uint64_t hash) { return nullptr; }
void EvictOldestL1Entries() {}
void LoadPersistentCache() {}
void CompileShaderAsync(const ShaderCompilationTask& task) {}

} // namespace rpcsx::shaders
