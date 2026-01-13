// ============================================================================
// Thread Pool (Task-based parallelism)
// ============================================================================
#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace rpcsx {
namespace util {

class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Submit a task
    void enqueue(std::function<void()> task);

    // Wait for all tasks
    void wait();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_tasks_;
};

} // namespace util
} // namespace rpcsx
