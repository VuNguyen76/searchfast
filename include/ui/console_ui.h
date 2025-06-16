#pragma once

#include "app/search_manager.h"
#include "core/types.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace FastFileSearch {
namespace UI {

// Console colors for Windows and Unix
class ConsoleColors {
public:
    static const std::string RESET;
    static const std::string BLACK;
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string MAGENTA;
    static const std::string CYAN;
    static const std::string WHITE;
    static const std::string BRIGHT_BLACK;
    static const std::string BRIGHT_RED;
    static const std::string BRIGHT_GREEN;
    static const std::string BRIGHT_YELLOW;
    static const std::string BRIGHT_BLUE;
    static const std::string BRIGHT_MAGENTA;
    static const std::string BRIGHT_CYAN;
    static const std::string BRIGHT_WHITE;
    
    static void enableColors();
    static bool isColorSupported();
};

// Progress bar for console
class ProgressBar {
private:
    int width_;
    std::string title_;
    std::atomic<double> progress_;
    std::atomic<bool> active_;
    std::string currentStatus_;
    std::mutex statusMutex_;
    
public:
    explicit ProgressBar(const std::string& title, int width = 50);
    
    void setProgress(double percentage);
    void setStatus(const std::string& status);
    void show();
    void hide();
    void update();
    
private:
    void clearLine();
    std::string createBar(double percentage);
};

// File icon representation for console
class ConsoleFileIcons {
public:
    static std::string getFileIcon(const FileEntry& entry);
    static std::string getFolderIcon();
    static std::string getFileTypeIcon(const std::string& extension);
    
private:
    static std::string getUnicodeIcon(const std::string& extension);
    static std::string getAsciiIcon(const std::string& extension);
};

// Main console UI class
class ConsoleUI {
private:
    std::unique_ptr<App::SearchManager> searchManager_;
    std::unique_ptr<ProgressBar> progressBar_;
    
    // State
    std::atomic<bool> isIndexing_;
    std::atomic<bool> isRunning_;
    std::atomic<double> indexingProgress_;
    std::string currentIndexingPath_;
    std::mutex pathMutex_;
    
    // Search results
    SearchResults currentResults_;
    std::mutex resultsMutex_;
    
    // UI settings
    bool useColors_;
    bool useUnicodeIcons_;
    int maxDisplayResults_;
    
public:
    ConsoleUI();
    ~ConsoleUI();
    
    // Main interface
    bool initialize();
    void run();
    void shutdown();
    
    // Commands
    void showWelcome();
    void showHelp();
    void showStatus();
    void startIndexing();
    void performSearch();
    void showSearchResults();
    void exportResults();
    void showSettings();
    
    // Interactive mode
    void runInteractiveMode();
    std::string getInput(const std::string& prompt);
    void processCommand(const std::string& command);

    // Access to search manager for direct commands
    App::SearchManager* getSearchManager() { return searchManager_.get(); }
    
    // Display helpers
    void printHeader(const std::string& title);
    void printSeparator(char character = '=', int length = 80);
    void printError(const std::string& message);
    void printSuccess(const std::string& message);
    void printWarning(const std::string& message);
    void printInfo(const std::string& message);
    
    // File display
    void displayFileEntry(const FileEntry& entry, double score = 0.0);
    void displayFileList(const std::vector<SearchResult>& results, int maxCount = 20);
    std::string formatFileSize(uint64_t size);
    std::string formatDateTime(std::time_t timestamp);
    
    // Progress monitoring
    void onIndexingProgress(double percentage, const std::string& currentPath);
    void onIndexingCompleted(bool success, const std::string& message);
    void onSearchCompleted(const SearchResults& results);
    
private:
    // Command parsing
    std::vector<std::string> parseCommand(const std::string& input);
    void executeSearchCommand(const std::vector<std::string>& args);
    void executeIndexCommand(const std::vector<std::string>& args);
    void executeListCommand(const std::vector<std::string>& args);
    void executeExportCommand(const std::vector<std::string>& args);
    void executeConfigCommand(const std::vector<std::string>& args);
    
    // Display formatting
    void printTable(const std::vector<std::vector<std::string>>& data, 
                   const std::vector<std::string>& headers);
    void printFileTree(const std::vector<FileEntry>& entries);
    
    // Input validation
    bool validateSearchQuery(const std::string& query);
    SearchMode parseSearchMode(const std::string& mode);
    
    // Settings
    void loadSettings();
    void saveSettings();
    void detectCapabilities();
    
    // Statistics
    void showIndexStatistics();
    void showSearchStatistics();
    void showPerformanceStats();
};

// Command line argument parser
class CommandLineParser {
private:
    std::vector<std::string> args_;
    std::map<std::string, std::string> options_;
    std::vector<std::string> positional_;
    
public:
    explicit CommandLineParser(int argc, char* argv[]);
    
    bool hasOption(const std::string& option) const;
    std::string getOption(const std::string& option, const std::string& defaultValue = "") const;
    const std::vector<std::string>& getPositionalArgs() const;
    
    void printUsage() const;
    
private:
    void parseArguments();
};

// Application entry point for console UI
class ConsoleApplication {
private:
    std::unique_ptr<ConsoleUI> ui_;
    CommandLineParser parser_;
    
public:
    ConsoleApplication(int argc, char* argv[]);
    
    int run();
    
private:
    void handleDirectCommand();
    void handleInteractiveMode();
    void setupSignalHandlers();
    
    static void signalHandler(int signal);
    static std::atomic<bool> shouldExit_;
};

} // namespace UI
} // namespace FastFileSearch
