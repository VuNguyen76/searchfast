#pragma once

#include "core/types.h"
#include "storage/sqlite_database.h"
#include "storage/memory_index.h"
#include "storage/cache_manager.h"
#include "utils/thread_safe_queue.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <filesystem>

namespace FastFileSearch {
namespace Engine {

class IndexManager {
public:
    using ProgressCallback = std::function<void(double percentage, const std::string& currentPath)>;
    using CompletionCallback = std::function<void(bool success, const std::string& message)>;

private:
    // Core components
    std::unique_ptr<Storage::SQLiteDatabase> database_;
    std::unique_ptr<Storage::MemoryIndex> memoryIndex_;
    std::unique_ptr<Storage::CacheManager> cacheManager_;
    
    // Threading
    std::vector<std::thread> indexingThreads_;
    Utils::ThreadSafeQueue<FileChangeEvent> changeEventQueue_;
    Utils::ThreadSafeQueue<std::filesystem::path> scanQueue_;
    
    // Thread control
    std::atomic<bool> isIndexing_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> isPaused_;
    std::atomic<double> indexingProgress_;
    
    // Synchronization
    mutable std::shared_mutex indexMutex_;
    std::mutex progressMutex_;
    std::condition_variable pauseCondition_;
    
    // Configuration
    AppSettings settings_;
    uint32_t numIndexingThreads_;
    
    // Statistics
    std::atomic<uint64_t> filesProcessed_;
    std::atomic<uint64_t> directoriesProcessed_;
    std::atomic<uint64_t> totalFilesFound_;
    std::atomic<uint64_t> errorsEncountered_;
    
    // Callbacks
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    
    // Drive information
    std::vector<DriveInfo> availableDrives_;
    mutable std::mutex drivesMutex_;

public:
    explicit IndexManager(const AppSettings& settings);
    ~IndexManager();
    
    // Non-copyable
    IndexManager(const IndexManager&) = delete;
    IndexManager& operator=(const IndexManager&) = delete;
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Index building
    bool buildInitialIndex();
    bool buildInitialIndex(const std::vector<std::string>& drives);
    bool rebuildIndex();
    bool rebuildIndex(const std::string& drive);
    
    // Incremental updates
    void updateIndex(const FileChangeEvent& event);
    void updateIndex(const std::vector<FileChangeEvent>& events);
    
    // Index management
    bool loadIndexFromDatabase();
    bool saveIndexToDatabase();
    bool validateIndex();
    void optimizeIndex();
    
    // Search interface
    std::vector<FileEntry> search(const SearchQuery& query);
    SearchResults searchWithResults(const SearchQuery& query);
    
    // File operations
    std::shared_ptr<FileEntry> getFile(uint64_t fileId);
    std::shared_ptr<FileEntry> getFileByPath(const std::string& path);
    std::vector<FileEntry> getFilesByParent(uint64_t parentId);
    std::vector<FileEntry> getFilesByDrive(uint64_t driveId);
    
    // Drive management
    std::vector<DriveInfo> getAvailableDrives();
    bool addDrive(const std::string& driveLetter);
    bool removeDrive(const std::string& driveLetter);
    void refreshDriveInfo();
    
    // Control operations
    void pauseIndexing();
    void resumeIndexing();
    void stopIndexing();
    bool isIndexing() const { return isIndexing_.load(); }
    bool isPaused() const { return isPaused_.load(); }
    double getIndexingProgress() const { return indexingProgress_.load(); }
    
    // Statistics
    IndexStatistics getStatistics() const;
    uint64_t getFilesProcessed() const { return filesProcessed_.load(); }
    uint64_t getDirectoriesProcessed() const { return directoriesProcessed_.load(); }
    uint64_t getTotalFilesFound() const { return totalFilesFound_.load(); }
    uint64_t getErrorsEncountered() const { return errorsEncountered_.load(); }
    
    // Configuration
    void updateSettings(const AppSettings& settings);
    const AppSettings& getSettings() const { return settings_; }
    
    // Callbacks
    void setProgressCallback(ProgressCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    
    // Memory management
    size_t getMemoryUsage() const;
    void clearCache();
    void optimizeMemoryUsage();
    
    // Maintenance
    bool performMaintenance();
    bool checkIntegrity();
    std::vector<std::string> getIntegrityErrors();

private:
    // Threading methods
    void indexingWorker();
    void changeEventProcessor();
    void scanDirectoryWorker();
    
    // Scanning methods
    void scanDrive(const std::string& driveLetter);
    void scanDirectory(const std::filesystem::path& directory);
    bool processFile(const std::filesystem::path& filePath);
    bool processDirectory(const std::filesystem::path& dirPath);
    
    // File processing
    FileEntry createFileEntry(const std::filesystem::path& path);
    bool shouldIndexFile(const std::filesystem::path& path);
    bool shouldIndexDirectory(const std::filesystem::path& path);
    
    // Change event processing
    void processFileCreated(const FileChangeEvent& event);
    void processFileModified(const FileChangeEvent& event);
    void processFileDeleted(const FileChangeEvent& event);
    void processFileRenamed(const FileChangeEvent& event);
    void processFileMoved(const FileChangeEvent& event);
    
    // Database operations
    bool syncToDatabase();
    bool loadFromDatabase();
    
    // Progress reporting
    void updateProgress(double percentage, const std::string& currentPath = "");
    void reportCompletion(bool success, const std::string& message = "");
    
    // Utility methods
    std::vector<std::string> getIncludedDrives();
    bool isDriveIncluded(const std::string& driveLetter);
    bool isPathExcluded(const std::filesystem::path& path);
    bool isExtensionExcluded(const std::string& extension);
    
    // Error handling
    void handleIndexingError(const std::exception& e, const std::filesystem::path& path);
    void logIndexingStatistics();
    
    // Thread management
    void startIndexingThreads();
    void stopIndexingThreads();
    void waitForIndexingThreads();
    
    // Drive detection
    std::vector<DriveInfo> detectAvailableDrives();
    DriveInfo getDriveInfo(const std::string& driveLetter);
    
    // Performance optimization
    void optimizeForSSD();
    void optimizeForHDD();
    void adjustThreadCount();
    
    // Batch processing
    struct BatchProcessor {
        std::vector<FileEntry> entries;
        std::mutex mutex;
        size_t batchSize;
        
        explicit BatchProcessor(size_t size = 1000) : batchSize(size) {}
        
        void addEntry(const FileEntry& entry);
        void flush(Storage::SQLiteDatabase& db, Storage::MemoryIndex& index);
    };
    
    std::unique_ptr<BatchProcessor> batchProcessor_;
};

// Helper class for monitoring indexing progress
class IndexingMonitor {
private:
    IndexManager& indexManager_;
    std::thread monitorThread_;
    std::atomic<bool> shouldStop_;
    std::chrono::milliseconds updateInterval_;
    
public:
    explicit IndexingMonitor(IndexManager& manager, 
                           std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    ~IndexingMonitor();
    
    void start();
    void stop();
    
private:
    void monitorLoop();
};

} // namespace Engine
} // namespace FastFileSearch
