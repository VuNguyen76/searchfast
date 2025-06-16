#pragma once

#include "core/types.h"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <functional>

namespace FastFileSearch {
namespace Storage {

// Trie node for efficient prefix matching
struct TrieNode {
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    std::unordered_set<uint64_t> fileIds;
    bool isEndOfWord = false;
    
    TrieNode() = default;
    ~TrieNode() = default;
    
    // Non-copyable
    TrieNode(const TrieNode&) = delete;
    TrieNode& operator=(const TrieNode&) = delete;
    
    // Movable
    TrieNode(TrieNode&&) = default;
    TrieNode& operator=(TrieNode&&) = default;
};

// Bloom filter for fast negative lookups
class BloomFilter {
private:
    std::vector<bool> bitArray_;
    size_t size_;
    size_t numHashFunctions_;
    std::atomic<size_t> elementCount_;
    
    // Hash functions
    std::hash<std::string> hasher_;
    
public:
    explicit BloomFilter(size_t expectedElements, double falsePositiveRate = 0.01);
    
    void add(const std::string& element);
    bool mightContain(const std::string& element) const;
    void clear();
    
    size_t size() const { return size_; }
    size_t elementCount() const { return elementCount_.load(); }
    double estimatedFalsePositiveRate() const;

private:
    std::vector<size_t> getHashes(const std::string& element) const;
    static size_t calculateOptimalSize(size_t expectedElements, double falsePositiveRate);
    static size_t calculateOptimalHashFunctions(size_t size, size_t expectedElements);
};

// Inverted index for full-text search
class InvertedIndex {
private:
    std::unordered_map<std::string, std::unordered_set<uint64_t>> tokenToFiles_;
    std::unordered_map<uint64_t, std::unordered_set<std::string>> fileToTokens_;
    mutable std::shared_mutex mutex_;
    
public:
    void addDocument(uint64_t fileId, const std::vector<std::string>& tokens);
    void removeDocument(uint64_t fileId);
    void updateDocument(uint64_t fileId, const std::vector<std::string>& tokens);
    
    std::unordered_set<uint64_t> search(const std::string& token) const;
    std::unordered_set<uint64_t> searchMultiple(const std::vector<std::string>& tokens, bool andOperation = true) const;
    
    void clear();
    size_t getTokenCount() const;
    size_t getDocumentCount() const;
    
    // Statistics
    std::vector<std::pair<std::string, size_t>> getMostFrequentTokens(size_t count = 10) const;
};

// Main in-memory index class
class MemoryIndex {
private:
    // Primary storage
    std::unordered_map<uint64_t, FileEntry> files_;
    std::unordered_map<std::string, uint64_t> pathToId_;
    
    // Specialized indexes
    std::unique_ptr<TrieNode> nameTrieRoot_;
    std::unique_ptr<BloomFilter> nameBloomFilter_;
    std::unique_ptr<InvertedIndex> invertedIndex_;
    
    // Extension index
    std::unordered_map<std::string, std::unordered_set<uint64_t>> extensionIndex_;
    
    // Size index (range-based)
    std::map<uint64_t, std::unordered_set<uint64_t>> sizeIndex_;
    
    // Date index (range-based)
    std::map<std::time_t, std::unordered_set<uint64_t>> modifiedDateIndex_;
    std::map<std::time_t, std::unordered_set<uint64_t>> accessedDateIndex_;
    
    // Directory hierarchy
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> parentToChildren_;
    std::unordered_map<uint64_t, uint64_t> childToParent_;
    
    // Drive mapping
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> driveToFiles_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
    
    // Statistics
    std::atomic<size_t> totalFiles_;
    std::atomic<size_t> totalDirectories_;
    std::atomic<uint64_t> totalSize_;
    
    // Configuration
    bool enableBloomFilter_;
    bool enableInvertedIndex_;
    size_t maxBloomFilterElements_;

public:
    explicit MemoryIndex(bool enableBloomFilter = true, bool enableInvertedIndex = true);
    ~MemoryIndex() = default;
    
    // Non-copyable
    MemoryIndex(const MemoryIndex&) = delete;
    MemoryIndex& operator=(const MemoryIndex&) = delete;
    
    // File operations
    bool addFile(const FileEntry& entry);
    bool updateFile(const FileEntry& entry);
    bool removeFile(uint64_t fileId);
    bool removeFileByPath(const std::string& path);
    
    // Retrieval operations
    std::shared_ptr<FileEntry> getFile(uint64_t fileId) const;
    std::shared_ptr<FileEntry> getFileByPath(const std::string& path) const;
    std::vector<FileEntry> getFilesByParent(uint64_t parentId) const;
    std::vector<FileEntry> getFilesByDrive(uint64_t driveId) const;
    
    // Search operations
    std::vector<uint64_t> searchByName(const std::string& name, bool exactMatch = false) const;
    std::vector<uint64_t> searchByPrefix(const std::string& prefix) const;
    std::vector<uint64_t> searchByExtension(const std::string& extension) const;
    std::vector<uint64_t> searchBySize(const SizeRange& range) const;
    std::vector<uint64_t> searchByModifiedDate(const DateRange& range) const;
    std::vector<uint64_t> searchByAccessedDate(const DateRange& range) const;
    std::vector<uint64_t> searchByTokens(const std::vector<std::string>& tokens, bool andOperation = true) const;
    
    // Complex search
    std::vector<uint64_t> search(const SearchQuery& query) const;
    
    // Bulk operations
    bool addFilesBatch(const std::vector<FileEntry>& entries);
    bool removeFilesBatch(const std::vector<uint64_t>& fileIds);
    
    // Index management
    void clear();
    void rebuild();
    void optimize();
    
    // Statistics
    size_t getFileCount() const { return totalFiles_.load(); }
    size_t getDirectoryCount() const { return totalDirectories_.load(); }
    uint64_t getTotalSize() const { return totalSize_.load(); }
    
    IndexStatistics getStatistics() const;
    
    // Memory usage
    size_t getEstimatedMemoryUsage() const;
    
    // Configuration
    void setBloomFilterEnabled(bool enabled);
    void setInvertedIndexEnabled(bool enabled);
    void setMaxBloomFilterElements(size_t maxElements);
    
    // Validation
    bool validateIntegrity() const;
    std::vector<std::string> getIntegrityErrors() const;

private:
    // Helper methods
    void addToNameTrie(const std::string& name, uint64_t fileId);
    void removeFromNameTrie(const std::string& name, uint64_t fileId);
    std::vector<uint64_t> searchTrie(const std::string& prefix) const;
    void searchTrieRecursive(const TrieNode* node, std::vector<uint64_t>& results) const;
    
    void addToExtensionIndex(const std::string& extension, uint64_t fileId);
    void removeFromExtensionIndex(const std::string& extension, uint64_t fileId);
    
    void addToSizeIndex(uint64_t size, uint64_t fileId);
    void removeFromSizeIndex(uint64_t size, uint64_t fileId);
    
    void addToDateIndex(std::time_t date, uint64_t fileId, 
                       std::map<std::time_t, std::unordered_set<uint64_t>>& index);
    void removeFromDateIndex(std::time_t date, uint64_t fileId,
                            std::map<std::time_t, std::unordered_set<uint64_t>>& index);
    
    void updateHierarchy(const FileEntry& entry);
    void removeFromHierarchy(uint64_t fileId);
    
    void updateDriveMapping(uint64_t driveId, uint64_t fileId);
    void removeFromDriveMapping(uint64_t driveId, uint64_t fileId);
    
    void updateStatistics(const FileEntry& entry, bool add);
    
    std::string normalizeString(const std::string& str) const;
    std::vector<std::string> tokenizeString(const std::string& str) const;
    
    // Range search helpers
    template<typename T>
    std::vector<uint64_t> searchRange(const std::map<T, std::unordered_set<uint64_t>>& index,
                                     T minValue, T maxValue) const;
};

} // namespace Storage
} // namespace FastFileSearch
