#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <chrono>

namespace FastFileSearch {
namespace Utils {

template<typename T>
class ThreadSafeQueue {
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_;

public:
    ThreadSafeQueue() : shutdown_(false) {}
    
    ~ThreadSafeQueue() {
        shutdown();
    }

    // Non-copyable
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    // Movable
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = std::move(other.queue_);
        shutdown_.store(other.shutdown_.load());
    }

    ThreadSafeQueue& operator=(ThreadSafeQueue&& other) noexcept {
        if (this != &other) {
            std::lock(mutex_, other.mutex_);
            std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
            std::lock_guard<std::mutex> lock2(other.mutex_, std::adopt_lock);
            
            queue_ = std::move(other.queue_);
            shutdown_.store(other.shutdown_.load());
        }
        return *this;
    }

    // Push an item to the queue
    void push(const T& item) {
        if (shutdown_.load()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        condition_.notify_one();
    }

    void push(T&& item) {
        if (shutdown_.load()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        condition_.notify_one();
    }

    // Emplace an item to the queue
    template<typename... Args>
    void emplace(Args&&... args) {
        if (shutdown_.load()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.emplace(std::forward<Args>(args)...);
        condition_.notify_one();
    }

    // Pop an item from the queue (blocking)
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (queue_.empty() && !shutdown_.load()) {
            condition_.wait(lock);
        }
        
        if (shutdown_.load() && queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // Pop an item from the queue with timeout
    template<typename Rep, typename Period>
    bool pop(T& item, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (condition_.wait_for(lock, timeout, [this] { return !queue_.empty() || shutdown_.load(); })) {
            if (shutdown_.load() && queue_.empty()) {
                return false;
            }
            
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        
        return false; // Timeout
    }

    // Try to pop an item (non-blocking)
    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // Get a shared pointer to the front item (blocking)
    std::shared_ptr<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (queue_.empty() && !shutdown_.load()) {
            condition_.wait(lock);
        }
        
        if (shutdown_.load() && queue_.empty()) {
            return std::shared_ptr<T>();
        }
        
        auto result = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return result;
    }

    // Try to get a shared pointer to the front item (non-blocking)
    std::shared_ptr<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return std::shared_ptr<T>();
        }
        
        auto result = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return result;
    }

    // Check if the queue is empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // Get the size of the queue
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // Clear the queue
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        queue_.swap(empty);
    }

    // Shutdown the queue (wake up all waiting threads)
    void shutdown() {
        shutdown_.store(true);
        condition_.notify_all();
    }

    // Check if the queue is shutdown
    bool isShutdown() const {
        return shutdown_.load();
    }

    // Restart the queue after shutdown
    void restart() {
        shutdown_.store(false);
    }
};

// Specialized version for unique_ptr
template<typename T>
class ThreadSafeQueue<std::unique_ptr<T>> {
private:
    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<T>> queue_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_;

public:
    ThreadSafeQueue() : shutdown_(false) {}
    
    ~ThreadSafeQueue() {
        shutdown();
    }

    // Non-copyable
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    void push(std::unique_ptr<T> item) {
        if (shutdown_.load()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        condition_.notify_one();
    }

    std::unique_ptr<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (queue_.empty() && !shutdown_.load()) {
            condition_.wait(lock);
        }
        
        if (shutdown_.load() && queue_.empty()) {
            return nullptr;
        }
        
        auto result = std::move(queue_.front());
        queue_.pop();
        return result;
    }

    std::unique_ptr<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return nullptr;
        }
        
        auto result = std::move(queue_.front());
        queue_.pop();
        return result;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::unique_ptr<T>> empty;
        queue_.swap(empty);
    }

    void shutdown() {
        shutdown_.store(true);
        condition_.notify_all();
    }

    bool isShutdown() const {
        return shutdown_.load();
    }

    void restart() {
        shutdown_.store(false);
    }
};

} // namespace Utils
} // namespace FastFileSearch
