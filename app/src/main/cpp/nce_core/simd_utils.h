// ============================================================================
// SIMD Utils (NEON/SVE2 wrappers for memcpy, memset, math)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

namespace rpcsx {
namespace simd {

// SIMD-optimized memcpy (NEON/SVE2)
void* memcpy_simd(void* dst, const void* src, size_t size);

// SIMD-optimized memset (NEON/SVE2)
void* memset_simd(void* dst, int value, size_t size);

// SIMD-optimized math (example: vector add)
void vector_add_f32(float* dst, const float* a, const float* b, size_t count);

} // namespace simd
} // namespace rpcsx
