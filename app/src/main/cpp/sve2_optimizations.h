/**
 * ARM SVE2-Specific Optimizations
 * 
 * Оптимізації для процесорів з підтримкою Scalable Vector Extension 2
 * (Cortex-X2/X3/X4, Snapdragon 8 Gen 1/2/3)
 * 
 * Особливості:
 * - Автодетекція SVE2/SME можливостей
 * - Оптимізовані SIMD операції для PS3 емуляції
 * - Vectorized memory copy/fill
 * - Паралельна обробка SPU даних
 * - Оптимізація для 128/256/512-bit векторів
 */

#ifndef RPCSX_SVE2_OPTIMIZATIONS_H
#define RPCSX_SVE2_OPTIMIZATIONS_H

#include <cstdint>
#include <cstddef>
#include <atomic>

namespace rpcsx::sve2 {

/**
 * SVE2 Feature Flags
 */
enum class SVE2Feature : uint32_t {
    NONE        = 0,
    SVE         = 1 << 0,   // Scalable Vector Extension
    SVE2        = 1 << 1,   // SVE2
    SVE2_AES    = 1 << 2,   // SVE2 AES instructions
    SVE2_SHA3   = 1 << 3,   // SVE2 SHA3 instructions
    SVE2_SM4    = 1 << 4,   // SVE2 SM4 instructions
    SVE2_BITPERM= 1 << 5,   // SVE2 bit permutation
    SME         = 1 << 6,   // Scalable Matrix Extension
    SME2        = 1 << 7,   // SME2
    I8MM        = 1 << 8,   // Int8 Matrix Multiplication
    F32MM       = 1 << 9,   // FP32 Matrix Multiplication
    BF16        = 1 << 10,  // BFloat16 support
};

inline SVE2Feature operator|(SVE2Feature a, SVE2Feature b) {
    return static_cast<SVE2Feature>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SVE2Feature operator&(SVE2Feature a, SVE2Feature b) {
    return static_cast<SVE2Feature>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * Конфігурація SVE2 оптимізацій
 */
struct SVE2Config {
    bool enable_sve2 = true;           // Увімкнути SVE2 якщо доступно
    bool enable_sme = true;            // Увімкнути SME якщо доступно
    bool force_neon_fallback = false;  // Примусовий fallback на NEON
    uint32_t vector_length = 0;        // 0 = автодетекція (128/256/512 біт)
    bool enable_prefetch = true;       // Увімкнути SVE prefetch
    bool enable_gather_scatter = true; // Увімкнути gather/scatter операції
};

/**
 * Статистика використання SVE2
 */
struct SVE2Stats {
    uint64_t sve2_ops_executed;        // Виконаних SVE2 операцій
    uint64_t neon_fallback_ops;        // Операцій через NEON fallback
    uint64_t vectorized_memcpy_bytes;  // Байт скопійовано через SVE2
    uint64_t matrix_ops_executed;      // Матричних операцій (SME)
    uint32_t current_vector_length;    // Поточна довжина вектора в бітах
    float sve2_utilization;            // % використання SVE2
};

/**
 * Результат детекції можливостей
 */
struct SVE2Capabilities {
    SVE2Feature features = SVE2Feature::NONE;
    uint32_t vector_length_bits = 128; // 128 = NEON only
    uint32_t num_predicates = 0;
    bool has_streaming_mode = false;   // SME streaming
    const char* cpu_name = "Unknown";
};

// =============================================================================
// Ініціалізація та детекція
// =============================================================================

/**
 * Ініціалізація SVE2 оптимізацій
 */
bool InitializeSVE2(const SVE2Config& config = SVE2Config());

/**
 * Завершення роботи
 */
void ShutdownSVE2();

/**
 * Перевірка чи SVE2 активний
 */
bool IsSVE2Active();

/**
 * Перевірка чи SME активний
 */
bool IsSMEActive();

/**
 * Детекція можливостей CPU
 */
SVE2Capabilities DetectCapabilities();

/**
 * Отримання поточних можливостей
 */
SVE2Capabilities GetCapabilities();

/**
 * Перевірка наявності конкретної фічі
 */
bool HasFeature(SVE2Feature feature);

// =============================================================================
// Оптимізовані операції пам'яті
// =============================================================================

/**
 * Оптимізоване копіювання пам'яті (SVE2 або NEON fallback)
 */
void* OptimizedMemcpy(void* dst, const void* src, size_t size);

/**
 * Оптимізоване заповнення пам'яті
 */
void* OptimizedMemset(void* dst, int value, size_t size);

/**
 * Оптимізоване порівняння пам'яті
 */
int OptimizedMemcmp(const void* s1, const void* s2, size_t size);

/**
 * Prefetch з оптимальною стратегією
 */
void OptimizedPrefetch(const void* addr, size_t size);

// =============================================================================
// Векторні операції для емуляції PS3
// =============================================================================

/**
 * Векторне додавання float32 (для SPU)
 */
void VectorAddF32(float* dst, const float* a, const float* b, size_t count);

/**
 * Векторне множення float32
 */
void VectorMulF32(float* dst, const float* a, const float* b, size_t count);

/**
 * Fused Multiply-Add (a * b + c)
 */
void VectorFMAF32(float* dst, const float* a, const float* b, 
                  const float* c, size_t count);

/**
 * Dot product для векторів
 */
float VectorDotF32(const float* a, const float* b, size_t count);

/**
 * Матричне множення 4x4 (для трансформацій)
 */
void Matrix4x4Multiply(float* dst, const float* a, const float* b);

/**
 * Batch Matrix-Vector multiply
 */
void BatchMatrixVectorMul(float* dst, const float* matrices, 
                          const float* vectors, size_t count);

// =============================================================================
// SPU-специфічні оптимізації
// =============================================================================

/**
 * SPU shuffle операція (SHUFB емуляція)
 */
void SPUShuffleBytes(uint8_t* dst, const uint8_t* a, const uint8_t* b,
                     const uint8_t* pattern, size_t count);

/**
 * SPU select операція
 */
void SPUSelect(uint32_t* dst, const uint32_t* a, const uint32_t* b,
               const uint32_t* mask, size_t count);

/**
 * SPU ROTL/ROTR операції
 */
void SPURotateLeft(uint32_t* dst, const uint32_t* src, 
                   const uint32_t* shift, size_t count);

/**
 * SPU Compare operations
 */
void SPUCompareGreater(uint32_t* dst, const float* a, 
                       const float* b, size_t count);

// =============================================================================
// RSX/Графічні оптимізації
// =============================================================================

/**
 * Swizzle текстури (для RSX format conversion)
 */
void SwizzleTexture(uint8_t* dst, const uint8_t* src,
                    uint32_t width, uint32_t height, uint32_t bpp);

/**
 * Deswizzle текстури
 */
void DeswizzleTexture(uint8_t* dst, const uint8_t* src,
                      uint32_t width, uint32_t height, uint32_t bpp);

/**
 * Конвертація кольорів (ARGB <-> RGBA etc)
 */
void ConvertColorFormat(uint8_t* dst, const uint8_t* src,
                        uint32_t src_format, uint32_t dst_format,
                        size_t pixel_count);

/**
 * Vertex buffer processing
 */
void ProcessVertexBuffer(float* dst, const float* src,
                         const float* transform_matrix,
                         size_t vertex_count, size_t stride);

// =============================================================================
// Конфігурація та статистика
// =============================================================================

/**
 * Встановлення конфігурації
 */
void SetConfig(const SVE2Config& config);

/**
 * Отримання конфігурації
 */
SVE2Config GetConfig();

/**
 * Отримання статистики
 */
void GetStats(SVE2Stats* stats);

/**
 * Скидання статистики
 */
void ResetStats();

/**
 * Встановлення довжини вектора (0 = авто)
 */
void SetVectorLength(uint32_t bits);

/**
 * Отримання поточної довжини вектора
 */
uint32_t GetVectorLength();

// Глобальні атомарні
extern std::atomic<bool> g_sve2_active;
extern std::atomic<uint32_t> g_vector_length;

} // namespace rpcsx::sve2

#endif // RPCSX_SVE2_OPTIMIZATIONS_H
