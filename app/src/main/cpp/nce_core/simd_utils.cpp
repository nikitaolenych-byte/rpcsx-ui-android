// ============================================================================
// SIMD Utils Implementation (NEON/SVE2 wrappers)
// ============================================================================
#include "simd_utils.h"
#include <arm_neon.h>
#include <cstring>

namespace rpcsx {
namespace simd {

void* memcpy_simd(void* dst, const void* src, size_t size) {
    // Simple NEON memcpy (128-bit blocks)
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        vst1q_u8(d + i, vld1q_u8(s + i));
    }
    for (; i < size; ++i) d[i] = s[i];
    return dst;
}

void* memset_simd(void* dst, int value, size_t size) {
    uint8x16_t v = vdupq_n_u8(static_cast<uint8_t>(value));
    uint8_t* d = static_cast<uint8_t*>(dst);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        vst1q_u8(d + i, v);
    }
    for (; i < size; ++i) d[i] = static_cast<uint8_t>(value);
    return dst;
}

void vector_add_f32(float* dst, const float* a, const float* b, size_t count) {
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        vst1q_f32(dst + i, vaddq_f32(va, vb));
    }
    for (; i < count; ++i) dst[i] = a[i] + b[i];
}

} // namespace simd
} // namespace rpcsx
