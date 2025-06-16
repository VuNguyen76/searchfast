#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>
#include <memory>
#include <unordered_map>

namespace FastFileSearch {

// Forward declarations
class FileEntry;
class SearchQuery;
class SearchResults;

// Enumerations
enum class FileType : uint8_t {
    Unknown = 0,
    File = 1,
    Directory = 2,
    SymbolicLink = 3,
    HardLink = 4
};

enum class SearchMode : uint8_t {
    Exact = 0,
    Fuzzy = 1,
    Wildcard = 2,
    Regex = 3
};

enum class SortOrder : uint8_t {
    Name = 0,
    Size = 1,
    Modified = 2,
    Accessed = 3,
    Relevance = 4
};

enum class Theme : uint8_t {
    Light = 0,
    Dark = 1,
    System = 2
};

// Data structures
struct SizeRange {
    uint64_t minSize = 0;
    uint64_t maxSize = UINT64_MAX;
    
    bool isInRange(uint64_t size) const {
        return size >= minSize && size <= maxSize;
    }
};

struct DateRange {
    std::time_t startDate = 0;
    std::time_t endDate = std::time(nullptr);
    
    bool isInRange(std::time_t date) const {
        return date >= startDate && date <= endDate;
    }
};

struct FileEntry {
    uint64_t id = 0;
    std::string fullPath;
    std::string fileName;
    std::string extension;
    uint64_t size = 0;
    std::time_t lastModified = 0;
    std::time_t lastAccessed = 0;
    FileType type = FileType::Unknown;
    uint32_t attributes = 0;
    
    // Indexing fields
    std::string normalizedName;
    std::vector<std::string> tokens;
    uint32_t accessCount = 0;
    double relevanceScore = 0.0;
    
    // Parent/child relationships
    uint64_t parentId = 0;
    uint64_t driveId = 0;
    
    // Constructors
    FileEntry() = default;
    FileEntry(const std::string& path);
    
    // Utility methods
    bool isDirectory() const { return type == FileType::Directory; }
    bool isFile() const { return type == FileType::File; }
    std::string getDisplayName() const;
    void updateTokens();
};

struct SearchQuery {
    std::string query;
    SearchMode mode = SearchMode::Fuzzy;
    std::vector<std::string> includeDrives;
    std::vector<std::string> excludePaths;
    std::vector<std::string> fileTypes;
    SizeRange sizeRange;
    DateRange dateRange;
    uint32_t maxResults = 1000;
    SortOrder sortOrder = SortOrder::Relevance;
    bool caseSensitive = false;
    
    // Fuzzy search parameters
    double fuzzyThreshold = 0.6;
    
    // Validation
    bool isValid() const;
    std::string toString() const;
};

struct SearchResult {
    FileEntry entry;
    double score = 0.0;
    std::vector<std::pair<size_t, size_t>> highlights; // start, length pairs
    
    SearchResult() = default;
    SearchResult(const FileEntry& e, double s) : entry(e), score(s) {}
};

class SearchResults {
private:
    std::vector<SearchResult> results_;
    std::string query_;
    std::time_t searchTime_;
    uint32_t totalMatches_ = 0;
    
public:
    SearchResults(const std::string& query);
    
    void addResult(const SearchResult& result);
    void addResult(const FileEntry& entry, double score);
    void sortByScore();
    void sortByName();
    void sortBySize();
    void sortByModified();
    
    const std::vector<SearchResult>& getResults() const { return results_; }
    size_t size() const { return results_.size(); }
    bool empty() const { return results_.empty(); }
    
    const std::string& getQuery() const { return query_; }
    std::time_t getSearchTime() const { return searchTime_; }
    uint32_t getTotalMatches() const { return totalMatches_; }
    void setTotalMatches(uint32_t total) { totalMatches_ = total; }
    
    // Iterator support
    auto begin() { return results_.begin(); }
    auto end() { return results_.end(); }
    auto begin() const { return results_.begin(); }
    auto end() const { return results_.end(); }
};

struct DriveInfo {
    uint64_t id = 0;
    std::string letter;
    std::string label;
    std::string fileSystem;
    uint64_t totalSize = 0;
    uint64_t freeSpace = 0;
    std::time_t lastScanned = 0;
    bool isAvailable = false;
    
    DriveInfo() = default;
    DriveInfo(const std::string& driveLetter);
    
    void updateInfo();
    double getUsagePercentage() const;
};

struct AppSettings {
    // Indexing settings
    std::vector<std::string> includeDrives;
    std::vector<std::string> excludePaths;
    std::vector<std::string> excludeExtensions;
    bool indexHiddenFiles = false;
    bool indexSystemFiles = false;
    
    // Search settings
    SearchMode defaultSearchMode = SearchMode::Fuzzy;
    uint32_t maxSearchResults = 1000;
    bool enableFuzzySearch = true;
    double fuzzyThreshold = 0.6;
    
    // UI settings
    bool startMinimized = false;
    bool showInSystemTray = true;
    std::string hotkey = "Ctrl+Alt+F";
    Theme uiTheme = Theme::System;
    
    // Performance settings
    uint32_t indexingThreads = 4;
    uint32_t maxMemoryUsage = 512; // MB
    bool enableCache = true;
    uint32_t cacheSize = 100; // MB
    
    // Database settings
    std::string databasePath = "fastfilesearch.db";
    bool enableWAL = true;
    uint32_t cachePages = 2000;
    
    // Validation and defaults
    void setDefaults();
    bool validate() const;
    void sanitize();
};

// File change events
enum class FileChangeType : uint8_t {
    Created = 0,
    Modified = 1,
    Deleted = 2,
    Renamed = 3,
    Moved = 4
};

struct FileChangeEvent {
    FileChangeType type;
    std::string path;
    std::string oldPath; // For rename/move operations
    std::time_t timestamp;
    
    FileChangeEvent(FileChangeType t, const std::string& p) 
        : type(t), path(p), timestamp(std::time(nullptr)) {}
    
    FileChangeEvent(FileChangeType t, const std::string& p, const std::string& oldP)
        : type(t), path(p), oldPath(oldP), timestamp(std::time(nullptr)) {}
};

// Ranking configuration
struct RankingConfig {
    double nameWeight = 0.4;
    double pathWeight = 0.2;
    double accessCountWeight = 0.2;
    double recentnessWeight = 0.1;
    double sizeWeight = 0.1;
    
    void normalize();
    bool isValid() const;
};

// Statistics
struct IndexStatistics {
    uint64_t totalFiles = 0;
    uint64_t totalDirectories = 0;
    uint64_t totalSize = 0;
    uint32_t indexedDrives = 0;
    std::time_t lastFullScan = 0;
    std::time_t lastUpdate = 0;
    double indexingProgress = 0.0;
    bool isIndexing = false;
    
    void reset();
    std::string toString() const;
};

} // namespace FastFileSearch
