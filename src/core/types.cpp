#include "core/types.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <thread>

namespace FastFileSearch {

// FileEntry implementation
FileEntry::FileEntry(const std::string& path) {
    try {
        std::filesystem::path fsPath(path);
        
        fullPath = fsPath.string();
        fileName = fsPath.filename().string();
        
        if (fsPath.has_extension()) {
            extension = fsPath.extension().string();
            // Remove the leading dot
            if (!extension.empty() && extension[0] == '.') {
                extension = extension.substr(1);
            }
        }
        
        if (std::filesystem::exists(fsPath)) {
            auto fileStatus = std::filesystem::status(fsPath);
            
            if (std::filesystem::is_directory(fileStatus)) {
                type = FileType::Directory;
                size = 0;
            } else if (std::filesystem::is_regular_file(fileStatus)) {
                type = FileType::File;
                size = std::filesystem::file_size(fsPath);
            } else if (std::filesystem::is_symlink(fileStatus)) {
                type = FileType::SymbolicLink;
                size = 0;
            } else {
                type = FileType::Unknown;
                size = 0;
            }
            
            auto lastWriteTime = std::filesystem::last_write_time(fsPath);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                lastWriteTime - std::filesystem::file_time_type::clock::now() + 
                std::chrono::system_clock::now());
            lastModified = std::chrono::system_clock::to_time_t(sctp);
            lastAccessed = lastModified; // Approximation
        }
        
        updateTokens();
    } catch (const std::exception&) {
        // Handle filesystem errors gracefully
        type = FileType::Unknown;
        size = 0;
        lastModified = 0;
        lastAccessed = 0;
    }
}

std::string FileEntry::getDisplayName() const {
    if (fileName.empty()) {
        return fullPath;
    }
    return fileName;
}

void FileEntry::updateTokens() {
    tokens.clear();
    normalizedName.clear();
    
    if (fileName.empty()) {
        return;
    }
    
    // Normalize the filename (lowercase, remove special characters)
    normalizedName.reserve(fileName.length());
    for (char c : fileName) {
        if (std::isalnum(c)) {
            normalizedName += std::tolower(c);
        } else if (c == '.' || c == '_' || c == '-' || c == ' ') {
            normalizedName += ' ';
        }
    }
    
    // Tokenize the normalized name
    std::istringstream iss(normalizedName);
    std::string token;
    while (iss >> token) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    // Add extension as a token if present
    if (!extension.empty()) {
        std::string extToken = extension;
        std::transform(extToken.begin(), extToken.end(), extToken.begin(), ::tolower);
        tokens.push_back(extToken);
    }
}

// SearchQuery implementation
bool SearchQuery::isValid() const {
    if (query.empty()) {
        return false;
    }
    
    if (maxResults == 0) {
        return false;
    }
    
    if (fuzzyThreshold < 0.0 || fuzzyThreshold > 1.0) {
        return false;
    }
    
    return true;
}

std::string SearchQuery::toString() const {
    std::ostringstream oss;
    oss << "Query: '" << query << "', Mode: ";
    
    switch (mode) {
        case SearchMode::Exact: oss << "Exact"; break;
        case SearchMode::Fuzzy: oss << "Fuzzy"; break;
        case SearchMode::Wildcard: oss << "Wildcard"; break;
        case SearchMode::Regex: oss << "Regex"; break;
    }
    
    oss << ", MaxResults: " << maxResults;
    oss << ", CaseSensitive: " << (caseSensitive ? "Yes" : "No");
    
    if (mode == SearchMode::Fuzzy) {
        oss << ", FuzzyThreshold: " << std::fixed << std::setprecision(2) << fuzzyThreshold;
    }
    
    return oss.str();
}

// SearchResults implementation
SearchResults::SearchResults(const std::string& query) 
    : query_(query), searchTime_(std::time(nullptr)) {
}

void SearchResults::addResult(const SearchResult& result) {
    results_.push_back(result);
}

void SearchResults::addResult(const FileEntry& entry, double score) {
    results_.emplace_back(entry, score);
}

void SearchResults::sortByScore() {
    std::sort(results_.begin(), results_.end(), 
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });
}

void SearchResults::sortByName() {
    std::sort(results_.begin(), results_.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.entry.fileName < b.entry.fileName;
              });
}

void SearchResults::sortBySize() {
    std::sort(results_.begin(), results_.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.entry.size > b.entry.size;
              });
}

void SearchResults::sortByModified() {
    std::sort(results_.begin(), results_.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.entry.lastModified > b.entry.lastModified;
              });
}

// DriveInfo implementation
DriveInfo::DriveInfo(const std::string& driveLetter) : letter(driveLetter) {
    updateInfo();
}

void DriveInfo::updateInfo() {
    try {
        std::filesystem::path drivePath(letter);
        
        if (std::filesystem::exists(drivePath)) {
            isAvailable = true;
            
            auto spaceInfo = std::filesystem::space(drivePath);
            totalSize = spaceInfo.capacity;
            freeSpace = spaceInfo.free;
            
            // Try to get volume label and filesystem type
            // This is platform-specific and would need proper implementation
            label = "Local Disk"; // Placeholder
            fileSystem = "NTFS";   // Placeholder
        } else {
            isAvailable = false;
            totalSize = 0;
            freeSpace = 0;
        }
        
        lastScanned = std::time(nullptr);
    } catch (const std::exception&) {
        isAvailable = false;
        totalSize = 0;
        freeSpace = 0;
    }
}

double DriveInfo::getUsagePercentage() const {
    if (totalSize == 0) {
        return 0.0;
    }
    
    uint64_t usedSpace = totalSize - freeSpace;
    return (static_cast<double>(usedSpace) / totalSize) * 100.0;
}

// AppSettings implementation
void AppSettings::setDefaults() {
    // Indexing settings
    includeDrives.clear();
    excludePaths = {
        "C:\\Windows\\System32",
        "C:\\Windows\\SysWOW64",
        "C:\\$Recycle.Bin",
        "C:\\System Volume Information"
    };
    excludeExtensions = {
        "tmp", "temp", "log", "cache", "bak"
    };
    indexHiddenFiles = false;
    indexSystemFiles = false;
    
    // Search settings
    defaultSearchMode = SearchMode::Fuzzy;
    maxSearchResults = 1000;
    enableFuzzySearch = true;
    fuzzyThreshold = 0.6;
    
    // UI settings
    startMinimized = false;
    showInSystemTray = true;
    hotkey = "Ctrl+Alt+F";
    uiTheme = Theme::System;
    
    // Performance settings
    indexingThreads = std::max(2u, std::thread::hardware_concurrency());
    maxMemoryUsage = 512; // MB
    enableCache = true;
    cacheSize = 100; // MB
    
    // Database settings
    databasePath = "fastfilesearch.db";
    enableWAL = true;
    cachePages = 2000;
}

bool AppSettings::validate() const {
    if (indexingThreads == 0 || indexingThreads > 32) {
        return false;
    }
    
    if (maxMemoryUsage < 64 || maxMemoryUsage > 8192) {
        return false;
    }
    
    if (cacheSize > maxMemoryUsage) {
        return false;
    }
    
    if (fuzzyThreshold < 0.0 || fuzzyThreshold > 1.0) {
        return false;
    }
    
    if (maxSearchResults == 0 || maxSearchResults > 100000) {
        return false;
    }
    
    return true;
}

void AppSettings::sanitize() {
    // Clamp values to valid ranges
    indexingThreads = std::max(1u, std::min(32u, indexingThreads));
    maxMemoryUsage = std::max(64u, std::min(8192u, maxMemoryUsage));
    cacheSize = std::max(10u, std::min(maxMemoryUsage, cacheSize));
    fuzzyThreshold = std::max(0.0, std::min(1.0, fuzzyThreshold));
    maxSearchResults = std::max(1u, std::min(100000u, maxSearchResults));
    
    // Remove empty strings from vectors
    auto removeEmpty = [](std::vector<std::string>& vec) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                [](const std::string& s) { return s.empty(); }),
                 vec.end());
    };
    
    removeEmpty(includeDrives);
    removeEmpty(excludePaths);
    removeEmpty(excludeExtensions);
}

// RankingConfig implementation
void RankingConfig::normalize() {
    double total = nameWeight + pathWeight + accessCountWeight + recentnessWeight + sizeWeight;
    
    if (total > 0.0) {
        nameWeight /= total;
        pathWeight /= total;
        accessCountWeight /= total;
        recentnessWeight /= total;
        sizeWeight /= total;
    } else {
        // Reset to defaults if all weights are zero
        nameWeight = 0.4;
        pathWeight = 0.2;
        accessCountWeight = 0.2;
        recentnessWeight = 0.1;
        sizeWeight = 0.1;
    }
}

bool RankingConfig::isValid() const {
    return nameWeight >= 0.0 && pathWeight >= 0.0 && accessCountWeight >= 0.0 &&
           recentnessWeight >= 0.0 && sizeWeight >= 0.0;
}

// IndexStatistics implementation
void IndexStatistics::reset() {
    totalFiles = 0;
    totalDirectories = 0;
    totalSize = 0;
    indexedDrives = 0;
    lastFullScan = 0;
    lastUpdate = 0;
    indexingProgress = 0.0;
    isIndexing = false;
}

std::string IndexStatistics::toString() const {
    std::ostringstream oss;
    oss << "Files: " << totalFiles 
        << ", Directories: " << totalDirectories
        << ", Size: " << (totalSize / (1024 * 1024)) << " MB"
        << ", Drives: " << indexedDrives
        << ", Progress: " << std::fixed << std::setprecision(1) 
        << (indexingProgress * 100) << "%"
        << ", Indexing: " << (isIndexing ? "Yes" : "No");
    return oss.str();
}

} // namespace FastFileSearch
