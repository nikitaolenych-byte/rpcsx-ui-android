// ============================================================================
// Profiler Implementation
// ============================================================================
#include "profiler.h"

namespace rpcsx {
namespace util {

void Profiler::Start(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[name].start = std::chrono::high_resolution_clock::now();
}

void Profiler::Stop(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::high_resolution_clock::now();
    auto& entry = entries_[name];
    entry.elapsed += std::chrono::duration<double>(now - entry.start).count();
}

double Profiler::GetElapsed(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(name);
    return (it != entries_.end()) ? it->second.elapsed : 0.0;
}

void Profiler::Reset(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[name].elapsed = 0.0;
}

} // namespace util
} // namespace rpcsx
