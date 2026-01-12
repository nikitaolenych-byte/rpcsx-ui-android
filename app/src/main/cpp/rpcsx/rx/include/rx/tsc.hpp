#pragma once

#include "types.hpp"
#include <cstdint>

#ifdef _M_X64
#ifdef _MSC_VER
extern "C" std::uint64_t __rdtsc();
#else
#include <immintrin.h>
#endif
#endif

namespace rx {
inline std::uint64_t get_tsc() {
#if defined(ARCH_ARM64)
  std::uint64_t r = 0;
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(r));
  return r;
#elif defined(_M_X64)
  return __rdtsc();
#elif defined(ARCH_X64)
  return __builtin_ia32_rdtsc();
#else
#error "Missing rx::get_tsc() implementation"
#endif
}
} // namespace rx
