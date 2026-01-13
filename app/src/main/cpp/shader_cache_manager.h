/**
 * Shader Cache Manager заголовковий файл
 */

#ifndef RPCSX_SHADER_CACHE_MANAGER_H
#define RPCSX_SHADER_CACHE_MANAGER_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <nce_core/thread_pool.h>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>

namespace rpcsx::shaders {

struct CompiledShader {
    std::vector<uint8_t> spirv_code;
    uint64_t hash;
    size_t original_size;
};

struct ShaderCompilationTask {
    std::vector<uint8_t> source_code;
    uint64_t hash;
    // Додаткові параметри компіляції
};

/**
 * Ініціалізація трирівневого shader cache
 */
bool InitializeShaderCache(const char* cache_directory);
#include <nce_core/thread_pool.h>

/**
 * Ініціалізація shader cache з build-id (для інвалідації кешу після оновлень)
 */
bool InitializeShaderCache(const char* cache_directory, const char* build_id);

/**
 * Пошук шейдера в кеші
 */
CompiledShader* FindShader(uint64_t shader_hash);

/**
 * Обчислення хешу шейдера
 */
uint64_t ComputeShaderHash(const void* shader_code, size_t size);

/**
 * Кешування шейдерів на різних рівнях
 */
void CacheShaderL1(uint64_t hash, const CompiledShader& shader);
void CacheShaderL2(uint64_t hash, const CompiledShader& shader);
void CacheShaderL3(uint64_t hash, const CompiledShader& shader);

/**
 * Async компіляція шейдерів
 */
void QueueShaderCompilation(const ShaderCompilationTask& task);
void SetThreadPool(util::ThreadPool* pool);

/**
 * Компресія/декомпресія
 */
std::vector<uint8_t> CompressShader(const void* data, size_t size);
std::vector<uint8_t> DecompressShader(const void* compressed_data, size_t compressed_size);

/**
 * Статистика
 */
void PrintCacheStats();

/**
 * Завершення роботи
 */
void ShutdownShaderCache();

// Допоміжні функції
CompiledShader* LoadFromL2Cache(uint64_t hash);
CompiledShader* LoadFromL3Cache(uint64_t hash);
void EvictOldestL1Entries();
void SetThreadPool(util::ThreadPool* pool);
void CompileShaderAsync(const ShaderCompilationTask& task);

} // namespace rpcsx::shaders

#endif // RPCSX_SHADER_CACHE_MANAGER_H
