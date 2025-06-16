#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <fstream>
#include <mutex>

namespace FastFileSearch {
namespace App {

class ExportManager {
public:
    enum class ExportFormat {
        CSV,
        JSON,
        XML,
        HTML,
        TXT,
        Excel,
        PDF
    };
    
    enum class ExportType {
        SearchResults,
        FileIndex,
        Statistics,
        Configuration,
        SearchHistory
    };
    
    struct ExportOptions {
        ExportFormat format = ExportFormat::CSV;
        bool includeHeaders = true;
        bool includeMetadata = true;
        bool includeStatistics = false;
        std::string delimiter = ",";
        std::string encoding = "UTF-8";
        bool compressOutput = false;
        uint32_t maxRecords = 0; // 0 = no limit
        
        // Filtering options
        std::vector<std::string> includeFields;
        std::vector<std::string> excludeFields;
        SizeRange sizeFilter;
        DateRange dateFilter;
        
        // Formatting options
        bool prettyPrint = true;
        std::string dateFormat = "yyyy-MM-dd HH:mm:ss";
        std::string sizeFormat = "auto"; // auto, bytes, KB, MB, GB
    };
    
    using ProgressCallback = std::function<void(double percentage, const std::string& currentItem)>;
    using CompletionCallback = std::function<void(bool success, const std::string& message, const std::string& outputPath)>;

private:
    mutable std::mutex mutex_;
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    
    // Export statistics
    std::atomic<uint64_t> totalExports_;
    std::atomic<uint64_t> successfulExports_;
    std::atomic<uint64_t> failedExports_;

public:
    ExportManager();
    ~ExportManager() = default;
    
    // Non-copyable
    ExportManager(const ExportManager&) = delete;
    ExportManager& operator=(const ExportManager&) = delete;
    
    // Main export methods
    bool exportSearchResults(const SearchResults& results, const std::string& outputPath, 
                           const ExportOptions& options = ExportOptions{});
    
    bool exportFileIndex(const std::vector<FileEntry>& files, const std::string& outputPath,
                        const ExportOptions& options = ExportOptions{});
    
    bool exportStatistics(const IndexStatistics& stats, const std::string& outputPath,
                         const ExportOptions& options = ExportOptions{});
    
    bool exportConfiguration(const AppSettings& settings, const std::string& outputPath,
                           const ExportOptions& options = ExportOptions{});
    
    bool exportSearchHistory(const std::vector<SearchQuery>& history, const std::string& outputPath,
                           const ExportOptions& options = ExportOptions{});
    
    // Async export methods
    void exportSearchResultsAsync(const SearchResults& results, const std::string& outputPath,
                                 const ExportOptions& options = ExportOptions{},
                                 CompletionCallback callback = nullptr);
    
    void exportFileIndexAsync(const std::vector<FileEntry>& files, const std::string& outputPath,
                             const ExportOptions& options = ExportOptions{},
                             CompletionCallback callback = nullptr);
    
    // Batch export
    bool exportMultiple(const std::vector<std::pair<ExportType, std::string>>& exports,
                       const std::string& outputDirectory,
                       const ExportOptions& options = ExportOptions{});
    
    // Format-specific export methods
    bool exportToCSV(const std::vector<FileEntry>& files, const std::string& outputPath,
                    const ExportOptions& options);
    
    bool exportToJSON(const std::vector<FileEntry>& files, const std::string& outputPath,
                     const ExportOptions& options);
    
    bool exportToXML(const std::vector<FileEntry>& files, const std::string& outputPath,
                    const ExportOptions& options);
    
    bool exportToHTML(const std::vector<FileEntry>& files, const std::string& outputPath,
                     const ExportOptions& options);
    
    bool exportToTXT(const std::vector<FileEntry>& files, const std::string& outputPath,
                    const ExportOptions& options);
    
    // Template-based export
    bool exportWithTemplate(const std::vector<FileEntry>& files, const std::string& templatePath,
                           const std::string& outputPath, const ExportOptions& options);
    
    // Import methods
    bool importFromCSV(const std::string& inputPath, std::vector<FileEntry>& files);
    bool importFromJSON(const std::string& inputPath, std::vector<FileEntry>& files);
    bool importFromXML(const std::string& inputPath, std::vector<FileEntry>& files);
    
    // Validation
    bool validateExportPath(const std::string& path, ExportFormat format) const;
    bool validateExportOptions(const ExportOptions& options) const;
    std::vector<std::string> getSupportedFormats() const;
    std::string getDefaultExtension(ExportFormat format) const;
    
    // Configuration
    void setProgressCallback(ProgressCallback callback);
    void setCompletionCallback(CompletionCallback callback);
    
    // Statistics
    uint64_t getTotalExports() const { return totalExports_.load(); }
    uint64_t getSuccessfulExports() const { return successfulExports_.load(); }
    uint64_t getFailedExports() const { return failedExports_.load(); }
    double getSuccessRate() const;
    void resetStatistics();
    
    // Utility methods
    std::string generateOutputPath(const std::string& baseDirectory, ExportType type, 
                                  ExportFormat format) const;
    size_t estimateOutputSize(const std::vector<FileEntry>& files, ExportFormat format) const;
    bool hasPermissionToWrite(const std::string& path) const;

private:
    // Core export implementation
    bool performExport(const std::vector<FileEntry>& files, const std::string& outputPath,
                      const ExportOptions& options);
    
    // Format-specific writers
    class CSVWriter {
    public:
        static bool write(const std::vector<FileEntry>& files, const std::string& outputPath,
                         const ExportOptions& options, ProgressCallback progressCallback);
    private:
        static std::string escapeCSVField(const std::string& field, const std::string& delimiter);
        static std::string formatCSVHeader(const ExportOptions& options);
        static std::string formatCSVRow(const FileEntry& entry, const ExportOptions& options);
    };
    
    class JSONWriter {
    public:
        static bool write(const std::vector<FileEntry>& files, const std::string& outputPath,
                         const ExportOptions& options, ProgressCallback progressCallback);
    private:
        static std::string escapeJSON(const std::string& str);
        static std::string formatJSONEntry(const FileEntry& entry, const ExportOptions& options);
    };
    
    class XMLWriter {
    public:
        static bool write(const std::vector<FileEntry>& files, const std::string& outputPath,
                         const ExportOptions& options, ProgressCallback progressCallback);
    private:
        static std::string escapeXML(const std::string& str);
        static std::string formatXMLEntry(const FileEntry& entry, const ExportOptions& options);
    };
    
    class HTMLWriter {
    public:
        static bool write(const std::vector<FileEntry>& files, const std::string& outputPath,
                         const ExportOptions& options, ProgressCallback progressCallback);
    private:
        static std::string escapeHTML(const std::string& str);
        static std::string generateHTMLHeader(const ExportOptions& options);
        static std::string generateHTMLFooter();
        static std::string formatHTMLRow(const FileEntry& entry, const ExportOptions& options);
    };
    
    class TXTWriter {
    public:
        static bool write(const std::vector<FileEntry>& files, const std::string& outputPath,
                         const ExportOptions& options, ProgressCallback progressCallback);
    private:
        static std::string formatTXTEntry(const FileEntry& entry, const ExportOptions& options);
    };
    
    // Filtering and processing
    std::vector<FileEntry> applyFilters(const std::vector<FileEntry>& files, 
                                       const ExportOptions& options) const;
    
    std::vector<FileEntry> limitRecords(const std::vector<FileEntry>& files,
                                       uint32_t maxRecords) const;
    
    // Field selection
    std::vector<std::string> getSelectedFields(const ExportOptions& options) const;
    std::vector<std::string> getAllAvailableFields() const;
    
    // Formatting helpers
    std::string formatFileSize(uint64_t size, const std::string& format) const;
    std::string formatDateTime(std::time_t timestamp, const std::string& format) const;
    std::string formatFileType(FileType type) const;
    
    // Compression
    bool compressFile(const std::string& inputPath, const std::string& outputPath) const;
    
    // Progress reporting
    void reportProgress(double percentage, const std::string& currentItem = "");
    void reportCompletion(bool success, const std::string& message, const std::string& outputPath);
    
    // Error handling
    void handleExportError(const std::exception& e, const std::string& operation);
    
    // File operations
    bool createOutputDirectory(const std::string& path) const;
    bool isValidOutputPath(const std::string& path) const;
    std::string sanitizeFilename(const std::string& filename) const;
    
    // Template processing
    std::string loadTemplate(const std::string& templatePath) const;
    std::string processTemplate(const std::string& templateContent, 
                               const std::vector<FileEntry>& files,
                               const ExportOptions& options) const;
    
    // Statistics tracking
    void updateStatistics(bool success);
};

// Export template engine
class ExportTemplateEngine {
public:
    struct TemplateVariable {
        std::string name;
        std::string value;
        std::string type; // string, number, date, boolean
    };
    
    static std::string processTemplate(const std::string& templateContent,
                                     const std::vector<TemplateVariable>& variables);
    
    static std::vector<TemplateVariable> createVariablesFromFileEntry(const FileEntry& entry);
    static std::vector<TemplateVariable> createVariablesFromSearchResults(const SearchResults& results);
    static std::vector<TemplateVariable> createVariablesFromStatistics(const IndexStatistics& stats);

private:
    static std::string replaceVariable(const std::string& content, const TemplateVariable& variable);
    static std::string evaluateExpression(const std::string& expression, 
                                         const std::vector<TemplateVariable>& variables);
};

} // namespace App
} // namespace FastFileSearch
