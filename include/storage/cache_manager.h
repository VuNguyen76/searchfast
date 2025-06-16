#pragma once

#include "core/types.h"
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>

namespace FastFileSearch {
namespace Storage {

// LRU Cache implementation for FileEntry objects
template<typename Key, typename Value>
class LRUCache {
private:
    struct CacheNode {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point accessTime;
        
        CacheNode(const Key& k, const Value& v) 
            : key(k), value(v), accessTime(std::chrono::steady_clock::now()) {}
    };
    
    using NodeList = std::list<CacheNode>;
    using NodeIterator = typename NodeList::iterator;
    using HashMap = std::unordered_map<Key, NodeIterator>;
    
    mutable std::mutex mutex_;
    NodeList nodeList_;
    HashMap hashMap_;
    size_t capacity_;
    size_t currentSize_;
    
    // Statistics
    mutable size_t hitCount_;
    mutable size_t missCount_;
    mutable size_t evictionCount_;

public:
    explicit LRUCache(size_t capacity) 
        : capacity_(capacity), currentSize_(0), hitCount_(0), missCount_(0), evictionCount_(0) {
        if (capacity_ == 0) {
            capacity_ = 1;
        }
    }
    
    ~LRUCache() = default;
    
    // Non-copyable
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    
    // Movable
    LRUCache(LRUCache&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex_);
        nodeList_ = std::move(other.nodeList_);
        hashMap_ = std::move(other.hashMap_);
        capacity_ = other.capacity_;
        currentSize_ = other.currentSize_;
        hitCount_ = other.hitCount_;
        missCount_ = other.missCount_;
        evictionCount_ = other.evictionCount_;
    }
    
    // Insert or update a value
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hashMap_.find(key);
        if (it != hashMap_.end()) {
            // Update existing entry
            it->second->value = value;
            it->second->accessTime = std::chrono::steady_clock::now();
            moveToFront(it->second);
        } else {
            // Insert new entry
            if (currentSize_ >= capacity_) {
                evictLRU();
            }
            
            nodeList_.emplace_front(key, value);
            hashMap_[key] = nodeList_.begin();
            ++currentSize_;
        }
    }
    
    // Get a value by key
    bool get(const Key& key, Value& value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hashMap_.find(key);
        if (it != hashMap_.end()) {
            value = it->second->value;
            it->second->accessTime = std::chrono::steady_clock::now();
            moveToFront(it->second);
            ++hitCount_;
            return true;
        }
        
        ++missCount_;
        return false;
    }
    
    // Get a shared pointer to the value
    std::shared_ptr<Value> get(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hashMap_.find(key);
        if (it != hashMap_.end()) {
            it->second->accessTime = std::chrono::steady_clock::now();
            moveToFront(it->second);
            ++hitCount_;
            return std::make_shared<Value>(it->second->value);
        }
        
        ++missCount_;
        return nullptr;
    }
    
    // Check if key exists
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return hashMap_.find(key) != hashMap_.end();
    }
    
    // Remove a key
    bool remove(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hashMap_.find(key);
        if (it != hashMap_.end()) {
            nodeList_.erase(it->second);
            hashMap_.erase(it);
            --currentSize_;
            return true;
        }
        
        return false;
    }
    
    // Clear all entries
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        nodeList_.clear();
        hashMap_.clear();
        currentSize_ = 0;
    }
    
    // Get current size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentSize_;
    }
    
    // Get capacity
    size_t capacity() const {
        return capacity_;
    }
    
    // Check if empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentSize_ == 0;
    }
    
    // Resize cache
    void resize(size_t newCapacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = std::max(newCapacity, size_t(1));
        
        while (currentSize_ > capacity_) {
            evictLRU();
        }
    }
    
    // Statistics
    double getHitRatio() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = hitCount_ + missCount_;
        return total > 0 ? static_cast<double>(hitCount_) / total : 0.0;
    }
    
    size_t getHitCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return hitCount_;
    }
    
    size_t getMissCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return missCount_;
    }
    
    size_t getEvictionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return evictionCount_;
    }
    
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        hitCount_ = 0;
        missCount_ = 0;
        evictionCount_ = 0;
    }

private:
    void moveToFront(NodeIterator it) const {
        // Move to front (most recently used)
        if (it != nodeList_.begin()) {
            nodeList_.splice(nodeList_.begin(), nodeList_, it);
        }
    }
    
    void evictLRU() {
        if (!nodeList_.empty()) {
            auto lastIt = std::prev(nodeList_.end());
            hashMap_.erase(lastIt->key);
            nodeList_.erase(lastIt);
            --currentSize_;
            ++evictionCount_;
        }
    }
};

// Specialized cache manager for the file search application
class CacheManager {
private:
    // Different caches for different types of data
    std::unique_ptr<LRUCache<uint64_t, FileEntry>> fileCache_;
    std::unique_ptr<LRUCache<std::string, SearchResults>> searchCache_;
    std::unique_ptr<LRUCache<std::string, std::vector<FileEntry>>> pathCache_;
    
    // Cache configuration
    size_t fileCacheSize_;
    size_t searchCacheSize_;
    size_t pathCacheSize_;
    
    // TTL for search results (in seconds)
    std::chrono::seconds searchResultTTL_;
    
    mutable std::mutex statsMutex_;
    
public:
    explicit CacheManager(size_t totalCacheSize = 100 * 1024 * 1024); // 100MB default
    ~CacheManager() = default;
    
    // Non-copyable
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;
    
    // File cache operations
    void putFile(uint64_t fileId, const FileEntry& entry);
    bool getFile(uint64_t fileId, FileEntry& entry) const;
    std::shared_ptr<FileEntry> getFile(uint64_t fileId) const;
    void removeFile(uint64_t fileId);
    
    // Search cache operations
    void putSearchResults(const std::string& query, const SearchResults& results);
    bool getSearchResults(const std::string& query, SearchResults& results) const;
    std::shared_ptr<SearchResults> getSearchResults(const std::string& query) const;
    void removeSearchResults(const std::string& query);
    
    // Path cache operations
    void putPathResults(const std::string& path, const std::vector<FileEntry>& entries);
    bool getPathResults(const std::string& path, std::vector<FileEntry>& entries) const;
    void removePathResults(const std::string& path);
    
    // Cache management
    void clear();
    void clearFileCache();
    void clearSearchCache();
    void clearPathCache();
    
    // Configuration
    void setFileCacheSize(size_t size);
    void setSearchCacheSize(size_t size);
    void setPathCacheSize(size_t size);
    void setSearchResultTTL(std::chrono::seconds ttl);
    
    // Statistics
    struct CacheStatistics {
        size_t fileCacheSize;
        size_t searchCacheSize;
        size_t pathCacheSize;
        double fileCacheHitRatio;
        double searchCacheHitRatio;
        double pathCacheHitRatio;
        size_t totalHits;
        size_t totalMisses;
        size_t totalEvictions;
    };
    
    CacheStatistics getStatistics() const;
    void resetStatistics();
    
    // Memory usage estimation
    size_t getEstimatedMemoryUsage() const;
    
    // Cleanup expired entries
    void cleanupExpiredEntries();

private:
    void distributeCacheSize(size_t totalSize);
    std::string generateSearchKey(const std::string& query) const;
    bool isSearchResultExpired(const SearchResults& results) const;
};

} // namespace Storage
} // namespace FastFileSearch
