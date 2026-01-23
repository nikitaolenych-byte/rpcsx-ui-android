// ============================================================================
// Stub implementations for missing symbols
// ============================================================================

#include <string>
#include <cstddef>
#include <cstring>

namespace rpcsx {
namespace util {

class Profiler {
public:
    static void Start(const std::string& name) {
        (void)name;
        // Profiling disabled - stub implementation
    }
    
    static void Stop(const std::string& name) {
        (void)name;
        // Profiling disabled - stub implementation
    }
};

} // namespace util

namespace simd {

void memcpy_simd(void* dst, const void* src, size_t size) {
    // Fallback to standard memcpy
    std::memcpy(dst, src, size);
}

void memset_simd(void* dst, int value, size_t size) {
    // Fallback to standard memset
    std::memset(dst, value, size);
}

} // namespace simd
} // namespace rpcsx
