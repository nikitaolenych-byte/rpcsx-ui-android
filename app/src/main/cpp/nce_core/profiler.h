// ============================================================================
// Profiler (Timing, counters, hot-spot detection)
// ============================================================================
#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>

namespace rpcsx {
namespace util {

class Profiler {
public:
    void Start(const std::string& name);
    void Stop(const std::string& name);
    double GetElapsed(const std::string& name) const;
    void Reset(const std::string& name);

private:
    struct Entry {
        std::chrono::high_resolution_clock::time_point start;
        double elapsed = 0.0;
    };
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace util
} // namespace rpcsx
