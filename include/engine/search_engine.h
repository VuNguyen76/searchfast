#pragma once

#include "core/types.h"
#include <memory>
#include <vector>
#include <string>
#include <regex>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace FastFileSearch {
namespace Engine {

// Abstract base class for different matching algorithms
class Matcher {
public:
    virtual ~Matcher() = default;
    virtual std::vector<std::pair<uint64_t, double>> match(
        const std::string& query, 
        const std::vector<FileEntry>& candidates) = 0;
    virtual double calculateScore(const std::string& query, const FileEntry& entry) = 0;
    virtual bool isMatch(const std::string& query, const FileEntry& entry) = 0;
};

// Fuzzy matching implementation
class FuzzyMatcher : public Matcher {
private:
    double threshold_;
    bool caseSensitive_;
    
    // Caching for performance
    mutable std::unordered_map<std::string, std::vector<std::vector<int>>> dpCache_;
    mutable std::mutex cacheMutex_;
    
public:
    explicit FuzzyMatcher(double threshold = 0.6, bool caseSensitive = false);
    
    std::vector<std::pair<uint64_t, double>> match(
        const std::string& query, 
        const std::vector<FileEntry>& candidates) override;
    
    double calculateScore(const std::string& query, const FileEntry& entry) override;
    bool isMatch(const std::string& query, const FileEntry& entry) override;
    
    // Configuration
    void setThreshold(double threshold) { threshold_ = threshold; }
    void setCaseSensitive(bool caseSensitive) { caseSensitive_ = caseSensitive; }
    
    // Specific fuzzy algorithms
    double levenshteinDistance(const std::string& s1, const std::string& s2);
    double jaroWinklerSimilarity(const std::string& s1, const std::string& s2);
    double longestCommonSubsequence(const std::string& s1, const std::string& s2);
    
    // Clear cache
    void clearCache();

private:
    std::string normalizeString(const std::string& str) const;
    double jaroSimilarity(const std::string& s1, const std::string& s2);
    std::vector<int> getMatchingCharacters(const std::string& s1, const std::string& s2, int maxDistance);
};

// Wildcard matching implementation
class WildcardMatcher : public Matcher {
private:
    bool caseSensitive_;
    
public:
    explicit WildcardMatcher(bool caseSensitive = false);
    
    std::vector<std::pair<uint64_t, double>> match(
        const std::string& query, 
        const std::vector<FileEntry>& candidates) override;
    
    double calculateScore(const std::string& query, const FileEntry& entry) override;
    bool isMatch(const std::string& query, const FileEntry& entry) override;
    
    // Configuration
    void setCaseSensitive(bool caseSensitive) { caseSensitive_ = caseSensitive; }
    
    // Wildcard matching
    bool wildcardMatch(const std::string& pattern, const std::string& text);

private:
    std::string normalizeString(const std::string& str) const;
    bool wildcardMatchRecursive(const char* pattern, const char* text);
};

// Regex matching implementation
class RegexMatcher : public Matcher {
private:
    mutable std::unordered_map<std::string, std::regex> regexCache_;
    mutable std::mutex cacheMutex_;
    std::regex_constants::syntax_option_type regexFlags_;
    
public:
    explicit RegexMatcher(bool caseSensitive = false);
    
    std::vector<std::pair<uint64_t, double>> match(
        const std::string& query, 
        const std::vector<FileEntry>& candidates) override;
    
    double calculateScore(const std::string& query, const FileEntry& entry) override;
    bool isMatch(const std::string& query, const FileEntry& entry) override;
    
    // Configuration
    void setCaseSensitive(bool caseSensitive);
    void setRegexFlags(std::regex_constants::syntax_option_type flags);
    
    // Clear cache
    void clearCache();

private:
    std::regex getCompiledRegex(const std::string& pattern) const;
    bool isValidRegex(const std::string& pattern) const;
};

// Main search engine class
class SearchEngine {
private:
    std::unique_ptr<FuzzyMatcher> fuzzyMatcher_;
    std::unique_ptr<WildcardMatcher> wildcardMatcher_;
    std::unique_ptr<RegexMatcher> regexMatcher_;
    
    // Ranking configuration
    RankingConfig rankingConfig_;
    
    // Search cache
    mutable std::unordered_map<std::string, SearchResults> searchCache_;
    mutable std::mutex cacheMutex_;
    size_t maxCacheSize_;
    
    // Performance settings
    uint32_t maxResults_;
    bool enableParallelSearch_;
    uint32_t numSearchThreads_;

public:
    SearchEngine();
    ~SearchEngine() = default;
    
    // Non-copyable
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
    
    // Main search interface
    SearchResults search(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    SearchResults search(const SearchQuery& query);
    
    // Search mode configuration
    void setSearchMode(SearchMode mode);
    SearchMode getCurrentSearchMode() const;
    
    // Ranking configuration
    void updateRankingWeights(const RankingConfig& config);
    const RankingConfig& getRankingConfig() const { return rankingConfig_; }
    
    // Matcher configuration
    void setFuzzyThreshold(double threshold);
    void setCaseSensitive(bool caseSensitive);
    void setRegexFlags(std::regex_constants::syntax_option_type flags);
    
    // Performance configuration
    void setMaxResults(uint32_t maxResults) { maxResults_ = maxResults; }
    void setParallelSearchEnabled(bool enabled) { enableParallelSearch_ = enabled; }
    void setSearchThreads(uint32_t numThreads) { numSearchThreads_ = numThreads; }
    void setMaxCacheSize(size_t maxSize) { maxCacheSize_ = maxSize; }
    
    // Cache management
    void clearCache();
    size_t getCacheSize() const;
    double getCacheHitRatio() const;
    
    // Statistics
    struct SearchStatistics {
        uint64_t totalSearches = 0;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        double averageSearchTime = 0.0;
        uint64_t totalResultsReturned = 0;
    };
    
    SearchStatistics getStatistics() const;
    void resetStatistics();

private:
    // Search implementation
    SearchResults performSearch(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    SearchResults performExactSearch(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    SearchResults performFuzzySearch(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    SearchResults performWildcardSearch(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    SearchResults performRegexSearch(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    
    // Result processing
    void rankResults(SearchResults& results, const SearchQuery& query);
    void applyFilters(SearchResults& results, const SearchQuery& query);
    void limitResults(SearchResults& results, uint32_t maxResults);
    
    // Ranking algorithms
    double calculateRelevanceScore(const FileEntry& entry, const SearchQuery& query, double matchScore);
    double calculateNameScore(const FileEntry& entry, const SearchQuery& query);
    double calculatePathScore(const FileEntry& entry, const SearchQuery& query);
    double calculateAccessCountScore(const FileEntry& entry);
    double calculateRecentnessScore(const FileEntry& entry);
    double calculateSizeScore(const FileEntry& entry);
    
    // Filtering
    bool passesFilters(const FileEntry& entry, const SearchQuery& query);
    bool passesSizeFilter(const FileEntry& entry, const SizeRange& range);
    bool passesDateFilter(const FileEntry& entry, const DateRange& range);
    bool passesExtensionFilter(const FileEntry& entry, const std::vector<std::string>& extensions);
    bool passesPathFilter(const FileEntry& entry, const std::vector<std::string>& excludePaths);
    
    // Cache management
    std::string generateCacheKey(const SearchQuery& query) const;
    void addToCache(const std::string& key, const SearchResults& results);
    bool getFromCache(const std::string& key, SearchResults& results) const;
    void evictOldestCacheEntry();
    
    // Parallel search
    SearchResults performParallelSearch(const SearchQuery& query, const std::vector<FileEntry>& candidates);
    void searchWorker(const SearchQuery& query, 
                     const std::vector<FileEntry>& candidates,
                     size_t startIndex, size_t endIndex,
                     std::vector<std::pair<uint64_t, double>>& results);
    
    // Utility methods
    std::vector<std::string> tokenizeQuery(const std::string& query);
    std::string normalizeQuery(const std::string& query);
    bool isValidQuery(const SearchQuery& query);
    
    // Statistics tracking
    mutable SearchStatistics statistics_;
    mutable std::mutex statisticsMutex_;
    void updateStatistics(const SearchResults& results, double searchTime, bool cacheHit);
};

// Search result highlighter
class SearchHighlighter {
public:
    struct Highlight {
        size_t start;
        size_t length;
        
        Highlight(size_t s, size_t l) : start(s), length(l) {}
    };
    
    static std::vector<Highlight> highlightMatches(const std::string& text, const std::string& query, SearchMode mode);
    static std::vector<Highlight> highlightExactMatches(const std::string& text, const std::string& query);
    static std::vector<Highlight> highlightFuzzyMatches(const std::string& text, const std::string& query);
    static std::vector<Highlight> highlightWildcardMatches(const std::string& text, const std::string& query);
    static std::vector<Highlight> highlightRegexMatches(const std::string& text, const std::string& query);

private:
    static std::string normalizeString(const std::string& str);
    static std::vector<size_t> findAllOccurrences(const std::string& text, const std::string& pattern);
};

} // namespace Engine
} // namespace FastFileSearch
