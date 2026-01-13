// ============================================================================
// Thread Pool Implementation
// ============================================================================
#include "thread_pool.h"

namespace rpcsx {
namespace util {

ThreadPool::ThreadPool(size_t num_threads) : stop_(false), active_tasks_(0) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++active_tasks_;
                }
                task();
                --active_tasks_;
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
}

void ThreadPool::wait() {
    while (active_tasks_ > 0 || !tasks_.empty()) {
        std::this_thread::yield();
    }
}

} // namespace util
} // namespace rpcsx
