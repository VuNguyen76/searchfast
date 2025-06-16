#pragma once

#include "core/types.h"
#include "engine/index_manager.h"
#include "engine/search_engine.h"
#include "engine/file_watcher.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <thread>

namespace FastFileSearch {
namespace App {

class SearchManager {
public:
    using SearchCompletedCallback = std::function<void(const SearchResults&)>;
    using IndexingProgressCallback = std::function<void(double percentage, const std::string& currentPath)>;
    using IndexingCompletedCallback = std::function<void(bool success, const std::string& message)>;
    using FileChangeCallback = std::function<void(const FileChangeEvent&)>;

private:
    // Core components
    std::unique_ptr<Engine::IndexManager> indexManager_;
    std::unique_ptr<Engine::SearchEngine> searchEngine_;
    std::unique_ptr<Engine::FileWatcher> fileWatcher_;
    
    // Configuration
    AppSettings settings_;
    
    // State management
    std::atomic<bool> isInitialized_;
    std::atomic<bool> isIndexing_;
    std::atomic<bool> isWatching_;
    
    // Search history
    std::vector<SearchQuery> searchHistory_;
    mutable std::mutex historyMutex_;
    size_t maxHistorySize_;
    
    // Recent searches cache
    std::unordered_map<std::string, SearchResults> recentSearches_;
    mutable std::mutex recentSearchesMutex_;
    size_t maxRecentSearches_;
    
    // Callbacks
    SearchCompletedCallback searchCompletedCallback_;
    IndexingProgressCallback indexingProgressCallback_;
    IndexingCompletedCallback indexingCompletedCallback_;
    FileChangeCallback fileChangeCallback_;
    
    // Background processing
    std::thread backgroundThread_;
    std::atomic<bool> shouldStopBackground_;
    
    // Statistics
    std::atomic<uint64_t> totalSearches_;
    std::atomic<uint64_t> totalIndexedFiles_;
    std::atomic<uint64_t> totalFileChanges_;
    std::chrono::steady_clock::time_point startTime_;

public:
    SearchManager();
    explicit SearchManager(const AppSettings& settings);
    ~SearchManager();
    
    // Non-copyable
    SearchManager(const SearchManager&) = delete;
    SearchManager& operator=(const SearchManager&) = delete;
    
    // Initialization and shutdown
    bool initialize();
    bool initialize(const AppSettings& settings);
    void shutdown();
    bool isInitialized() const { return isInitialized_.load(); }
    
    // Search operations
    SearchResults search(const SearchQuery& query);
    SearchResults search(const std::string& queryString, SearchMode mode = SearchMode::Fuzzy);
    void searchAsync(const SearchQuery& query, SearchCompletedCallback callback = nullptr);
    void searchAsync(const std::string& queryString, SearchMode mode = SearchMode::Fuzzy, 
                    SearchCompletedCallback callback = nullptr);
    
    // Index management
    bool buildIndex();
    bool buildIndex(const std::vector<std::string>& drives);
    bool rebuildIndex();
    bool rebuildIndex(const std::string& drive);
    void pauseIndexing();
    void resumeIndexing();
    void stopIndexing();
    
    // File watching
    bool startFileWatching();
    bool startFileWatching(const std::vector<std::string>& drives);
    void stopFileWatching();
    bool isFileWatching() const { return isWatching_.load(); }
    
    // State queries
    bool isIndexing() const { return isIndexing_.load(); }
    double getIndexingProgress() const;
    IndexStatistics getIndexStatistics() const;
    
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
    
    // Search history
    void addToHistory(const SearchQuery& query);
    std::vector<SearchQuery> getSearchHistory() const;
    std::vector<SearchQuery> getSearchHistory(size_t maxCount) const;
    void clearSearchHistory();
    
    // Recent searches
    std::vector<std::string> getRecentSearchQueries(size_t maxCount = 10) const;
    SearchResults getRecentSearchResults(const std::string& query) const;
    void clearRecentSearches();
    
    // Configuration
    void updateSettings(const AppSettings& settings);
    const AppSettings& getSettings() const { return settings_; }
    
    // Search configuration
    void setDefaultSearchMode(SearchMode mode);
    void setMaxSearchResults(uint32_t maxResults);
    void setFuzzyThreshold(double threshold);
    void setCaseSensitive(bool caseSensitive);
    
    // Callbacks
    void setSearchCompletedCallback(SearchCompletedCallback callback);
    void setIndexingProgressCallback(IndexingProgressCallback callback);
    void setIndexingCompletedCallback(IndexingCompletedCallback callback);
    void setFileChangeCallback(FileChangeCallback callback);
    
    // Statistics
    uint64_t getTotalSearches() const { return totalSearches_.load(); }
    uint64_t getTotalIndexedFiles() const { return totalIndexedFiles_.load(); }
    uint64_t getTotalFileChanges() const { return totalFileChanges_.load(); }
    std::chrono::seconds getUptime() const;
    
    // Performance monitoring
    double getAverageSearchTime() const;
    double getCacheHitRatio() const;
    size_t getMemoryUsage() const;
    
    // Maintenance operations
    bool performMaintenance();
    bool checkIntegrity();
    std::vector<std::string> getIntegrityErrors();
    void optimizePerformance();
    void clearCaches();
    
    // Export functionality
    bool exportSearchResults(const SearchResults& results, const std::string& filePath, 
                           const std::string& format = "csv");
    bool exportIndex(const std::string& filePath, const std::string& format = "json");
    
    // Import functionality
    bool importSettings(const std::string& filePath);
    bool exportSettings(const std::string& filePath);

private:
    // Initialization helpers
    bool initializeIndexManager();
    bool initializeSearchEngine();
    bool initializeFileWatcher();
    
    // Background processing
    void backgroundWorker();
    void processFileChanges();
    void performPeriodicMaintenance();
    void updateStatistics();
    
    // Search helpers
    SearchResults performSearch(const SearchQuery& query);
    void validateSearchQuery(SearchQuery& query);
    void addToRecentSearches(const std::string& query, const SearchResults& results);
    
    // History management
    void trimSearchHistory();
    void trimRecentSearches();
    
    // Event handlers
    void onIndexingProgress(double percentage, const std::string& currentPath);
    void onIndexingCompleted(bool success, const std::string& message);
    void onFileChange(const FileChangeEvent& event);
    
    // Configuration helpers
    void applySettings();
    void validateSettings();
    
    // Error handling
    void handleSearchError(const std::exception& e, const SearchQuery& query);
    void handleIndexingError(const std::exception& e);
    void handleFileWatchingError(const std::exception& e);
    
    // Utility methods
    std::string generateSearchKey(const SearchQuery& query) const;
    bool shouldCacheSearch(const SearchQuery& query) const;
    void logSearchStatistics(const SearchQuery& query, const SearchResults& results, 
                           std::chrono::milliseconds searchTime);
};

// Helper class for search suggestions
class SearchSuggestionProvider {
private:
    const SearchManager& searchManager_;
    std::vector<std::string> commonQueries_;
    std::unordered_map<std::string, uint32_t> queryFrequency_;
    mutable std::mutex mutex_;
    
public:
    explicit SearchSuggestionProvider(const SearchManager& manager);
    
    std::vector<std::string> getSuggestions(const std::string& partialQuery, size_t maxSuggestions = 5) const;
    void addQuery(const std::string& query);
    void updateFrequency(const std::string& query);
    void clearSuggestions();
    
private:
    std::vector<std::string> generateFilenameSuggestions(const std::string& partialQuery) const;
    std::vector<std::string> generateExtensionSuggestions(const std::string& partialQuery) const;
    std::vector<std::string> generatePathSuggestions(const std::string& partialQuery) const;
    double calculateSuggestionScore(const std::string& suggestion, const std::string& partialQuery) const;
};

// Search result formatter
class SearchResultFormatter {
public:
    enum class Format {
        Plain,
        CSV,
        JSON,
        XML,
        HTML
    };
    
    static std::string format(const SearchResults& results, Format format);
    static std::string formatPlain(const SearchResults& results);
    static std::string formatCSV(const SearchResults& results);
    static std::string formatJSON(const SearchResults& results);
    static std::string formatXML(const SearchResults& results);
    static std::string formatHTML(const SearchResults& results);
    
private:
    static std::string escapeCSV(const std::string& str);
    static std::string escapeJSON(const std::string& str);
    static std::string escapeXML(const std::string& str);
    static std::string escapeHTML(const std::string& str);
};

} // namespace App
} // namespace FastFileSearch
