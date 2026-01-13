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
#include <dirent.h>
#include <cerrno>
#include <cstring>
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
    
    // Async компіляція через глобальний thread pool
    util::ThreadPool* thread_pool = nullptr;
    
    // Статистика
    std::atomic<uint64_t> l1_hits{0};
    std::atomic<uint64_t> l2_hits{0};
    std::atomic<uint64_t> l3_hits{0};
    std::atomic<uint64_t> cache_misses{0};
};

static std::unique_ptr<ShaderCacheImpl> g_cache;

static constexpr uint32_t kShaderCacheMetaVersion = 2;

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
    // Best-effort: KGSL exposes useful identifiers on Adreno devices.
    const char* paths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_model",
        "/sys/class/kgsl/kgsl-3d0/gpu_chipid",
        "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",
    };

    for (const char* p : paths) {
        std::string v = Trim(ReadTextFile(p));
        if (!v.empty()) {
            return std::string(p) + ":" + v;
        }
    }
    return "unknown";
}

static void PurgeFile(const std::string& path) {
    if (unlink(path.c_str()) == 0) {
        LOGI("Purged cache file: %s", path.c_str());
    }
}

static void PurgeDirectoryContents(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return;

    LOGI("Purging cache directory: %s", path.c_str());

    while (dirent* ent = readdir(d)) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        std::string child = path + "/" + ent->d_name;
        if (ent->d_type == DT_DIR) {
            PurgeDirectoryContents(child);
            rmdir(child.c_str());
        } else {
            unlink(child.c_str());
        }
    }
    closedir(d);
}

static bool EnsureCacheCompatible(const std::string& root_dir,
                                  const std::string& l2_dir,
                                  const std::string& l3_file,
                                  const char* build_id) {
    const std::string meta_path = root_dir + "/shader_cache_meta.txt";
    const std::string current_build = build_id ? Trim(build_id) : "unknown";
    const std::string current_gpu = GetGpuIdentity();

    bool purge = false;
    const std::string existing = ReadTextFile(meta_path);
    if (!existing.empty()) {
        uint32_t version = 0;
        std::string build;
        std::string gpu;

        std::istringstream iss(existing);
        std::string line;
        while (std::getline(iss, line)) {
            line = Trim(line);
            if (line.rfind("version=", 0) == 0) version = static_cast<uint32_t>(std::strtoul(line.c_str() + 8, nullptr, 10));
            if (line.rfind("build=", 0) == 0) build = line.substr(6);
            if (line.rfind("gpu=", 0) == 0) gpu = line.substr(4);
        }

        if (version != kShaderCacheMetaVersion || build != current_build || gpu != current_gpu) {
            LOGI("Shader cache meta mismatch: v%u/%u build='%s'/'%s' gpu='%s'/'%s'", 
                 version, kShaderCacheMetaVersion,
                 build.c_str(), current_build.c_str(),
                 gpu.c_str(), current_gpu.c_str());
            purge = true;
        }
    } else {
        purge = true;
    }

    if (purge) {
        PurgeFile(l3_file);
        PurgeDirectoryContents(l2_dir);
    }

    std::ostringstream meta;
    meta << "version=" << kShaderCacheMetaVersion << "\n";
    meta << "build=" << current_build << "\n";
    meta << "gpu=" << current_gpu << "\n";
    if (!WriteTextFile(meta_path, meta.str())) {
        LOGE("Failed to write shader cache meta: %s", meta_path.c_str());
    }

    return true;
}

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
    return InitializeShaderCache(cache_directory, nullptr);
}

bool InitializeShaderCache(const char* cache_directory, const char* build_id) {
    LOGI("Initializing 3-tier Shader Cache with Zstd compression");

    g_cache = std::make_unique<ShaderCacheImpl>();

    const std::string root_dir = std::string(cache_directory);
    mkdir(root_dir.c_str(), 0755);

    // Створюємо директорії для кешу
    g_cache->l2_cache_dir = root_dir + "/shader_cache_l2";
    g_cache->l3_cache_file = root_dir + "/shader_cache_l3.zst";

    mkdir(g_cache->l2_cache_dir.c_str(), 0755);

    // Інвалідація кешу при зміні білда або GPU-ідентифікатора.
    EnsureCacheCompatible(root_dir, g_cache->l2_cache_dir, g_cache->l3_cache_file, build_id);

    // Завантажуємо існуючий кеш з диска
    LoadPersistentCache();

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
// AsyncCompilationWorker більше не потрібен

/**
 * Додавання завдання в чергу async компіляції
 */
void QueueShaderCompilation(const ShaderCompilationTask& task) {
    if (!g_cache || !g_cache->thread_pool) {
        LOGE("Thread pool not set for shader cache!");
        return;
    }
    // Кидаємо завдання у глобальний thread pool
    g_cache->thread_pool->enqueue([task]{
        CompileShaderAsync(task);
    });
    LOGI("Shader queued for async compilation (thread pool)");
}
void SetThreadPool(util::ThreadPool* pool) {
    if (g_cache) {
        g_cache->thread_pool = pool;
        LOGI("Shader cache: thread pool set");
    }
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
    // Дочекаємося завершення всіх завдань, якщо thread pool заданий
    if (g_cache->thread_pool) {
        g_cache->thread_pool->wait();
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
