#pragma once

#include "core/types.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>

namespace FastFileSearch {
namespace Engine {

// Loading states for directories
enum class LoadingState {
    NotLoaded,      // Chưa tải
    Loading,        // Đang tải
    Loaded,         // Đã tải xong
    Error          // Lỗi khi tải
};

// Directory node in the progressive tree
struct DirectoryNode {
    std::string path;
    std::string name;
    LoadingState state;
    
    // Children (only immediate children)
    std::vector<std::shared_ptr<DirectoryNode>> subdirectories;
    std::vector<FileEntry> files;
    
    // Metadata
    size_t totalFiles;
    size_t totalSubdirs;
    uint64_t totalSize;
    std::time_t lastScanned;
    
    // Loading info
    std::atomic<double> loadingProgress;
    std::string currentLoadingPath;
    
    DirectoryNode(const std::string& dirPath);
    bool isRoot() const;
    bool hasChildren() const;
    bool needsLoading() const;
};

// Progressive indexing strategy
class ProgressiveIndexer {
public:
    // Callback types
    using ProgressCallback = std::function<void(const std::string& path, double progress)>;
    using CompletionCallback = std::function<void(const std::string& path, bool success)>;
    using FileFoundCallback = std::function<void(const FileEntry& file)>;
    
private:
    // Core data structures
    std::shared_ptr<DirectoryNode> rootNode_;
    std::unordered_map<std::string, std::shared_ptr<DirectoryNode>> nodeCache_;
    
    // Loading queue and management
    std::queue<std::shared_ptr<DirectoryNode>> loadingQueue_;
    std::unordered_set<std::string> currentlyLoading_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    // Threading
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> isRunning_;
    
    // Callbacks
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    FileFoundCallback fileFoundCallback_;
    
    // Configuration
    size_t maxWorkerThreads_;
    size_t maxCacheSize_;
    bool enableBackgroundLoading_;
    std::chrono::milliseconds loadingDelay_;
    
    // Statistics
    std::atomic<size_t> totalDirectoriesScanned_;
    std::atomic<size_t> totalFilesFound_;
    std::atomic<uint64_t> totalSizeScanned_;
    
public:
    explicit ProgressiveIndexer(size_t maxThreads = 2);
    ~ProgressiveIndexer();
    
    // Main interface
    bool initialize(const std::vector<std::string>& rootPaths);
    void shutdown();
    
    // Progressive loading
    std::shared_ptr<DirectoryNode> getRootNode() const;
    std::shared_ptr<DirectoryNode> getNode(const std::string& path);
    bool loadDirectory(const std::string& path, bool blocking = false);
    bool loadDirectoryAsync(const std::string& path);
    
    // Tree navigation
    std::vector<std::shared_ptr<DirectoryNode>> getChildren(const std::string& path);
    std::vector<FileEntry> getFiles(const std::string& path);
    bool isDirectoryLoaded(const std::string& path) const;
    LoadingState getLoadingState(const std::string& path) const;
    
    // Search in loaded areas
    std::vector<SearchResult> searchLoaded(const SearchQuery& query);
    std::vector<SearchResult> searchInDirectory(const std::string& path, const SearchQuery& query);
    
    // Background operations
    void enableBackgroundLoading(bool enable);
    void preloadSiblings(const std::string& path);
    void preloadChildren(const std::string& path);
    
    // Cache management
    void clearCache();
    void evictOldEntries();
    size_t getCacheSize() const;
    
    // Callbacks
    void setProgressCallback(ProgressCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    void setFileFoundCallback(FileFoundCallback callback);
    
    // Statistics
    IndexStatistics getStatistics() const;
    double getLoadingProgress(const std::string& path) const;
    
private:
    // Core loading logic
    void workerThread();
    bool loadDirectoryInternal(std::shared_ptr<DirectoryNode> node);
    void scanDirectoryContents(std::shared_ptr<DirectoryNode> node);
    
    // Cache management
    void addToCache(std::shared_ptr<DirectoryNode> node);
    void removeFromCache(const std::string& path);
    bool shouldEvictFromCache() const;
    
    // Path utilities
    std::string normalizePath(const std::string& path) const;
    std::string getParentPath(const std::string& path) const;
    bool isValidPath(const std::string& path) const;
    
    // Background loading strategies
    void scheduleBackgroundLoading(const std::string& path);
    void loadSiblingsInBackground(const std::string& path);
    void loadChildrenInBackground(const std::string& path);
    
    // Error handling
    void handleLoadingError(std::shared_ptr<DirectoryNode> node, const std::exception& e);
    void notifyLoadingComplete(std::shared_ptr<DirectoryNode> node, bool success);
};

// Smart file tree for UI
class SmartFileTree {
private:
    std::unique_ptr<ProgressiveIndexer> indexer_;
    std::shared_ptr<DirectoryNode> currentNode_;
    std::vector<std::string> navigationHistory_;
    size_t historyIndex_;
    
    // UI state
    std::string selectedPath_;
    std::unordered_set<std::string> expandedPaths_;
    
public:
    SmartFileTree();
    ~SmartFileTree();
    
    // Initialization
    bool initialize(const std::vector<std::string>& rootPaths);
    
    // Navigation
    bool navigateTo(const std::string& path);
    bool navigateUp();
    bool navigateBack();
    bool navigateForward();
    
    // Tree operations
    bool expandDirectory(const std::string& path);
    bool collapseDirectory(const std::string& path);
    bool refreshDirectory(const std::string& path);
    
    // Current view
    std::vector<std::shared_ptr<DirectoryNode>> getCurrentDirectories() const;
    std::vector<FileEntry> getCurrentFiles() const;
    std::string getCurrentPath() const;
    
    // Selection
    void selectItem(const std::string& path);
    std::string getSelectedPath() const;
    
    // Search
    std::vector<SearchResult> searchCurrent(const SearchQuery& query);
    std::vector<SearchResult> searchAll(const SearchQuery& query);
    
    // State queries
    bool isExpanded(const std::string& path) const;
    bool isLoading(const std::string& path) const;
    LoadingState getLoadingState(const std::string& path) const;
    
    // Callbacks for UI updates
    using TreeUpdateCallback = std::function<void(const std::string& path)>;
    void setTreeUpdateCallback(TreeUpdateCallback callback);
    
private:
    TreeUpdateCallback treeUpdateCallback_;
    
    void onDirectoryLoaded(const std::string& path, bool success);
    void onLoadingProgress(const std::string& path, double progress);
    void updateNavigationHistory(const std::string& path);
};

// Optimized loading strategies
class LoadingStrategy {
public:
    virtual ~LoadingStrategy() = default;
    virtual std::vector<std::string> getNextPaths(const std::string& currentPath) = 0;
    virtual int getPriority(const std::string& path) = 0;
};

class BreadthFirstStrategy : public LoadingStrategy {
public:
    std::vector<std::string> getNextPaths(const std::string& currentPath) override;
    int getPriority(const std::string& path) override;
};

class DepthFirstStrategy : public LoadingStrategy {
public:
    std::vector<std::string> getNextPaths(const std::string& currentPath) override;
    int getPriority(const std::string& path) override;
};

class UserPatternStrategy : public LoadingStrategy {
private:
    std::unordered_map<std::string, int> accessCounts_;
    std::unordered_map<std::string, std::time_t> lastAccessed_;
    
public:
    std::vector<std::string> getNextPaths(const std::string& currentPath) override;
    int getPriority(const std::string& path) override;
    void recordAccess(const std::string& path);
};

} // namespace Engine
} // namespace FastFileSearch
