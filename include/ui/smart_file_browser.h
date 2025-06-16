#pragma once

#include "engine/progressive_indexer.h"
#include "core/types.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>

namespace FastFileSearch {
namespace UI {

// Console-based smart file browser with progressive loading
class SmartFileBrowser {
private:
    std::unique_ptr<Engine::SmartFileTree> fileTree_;
    
    // Display state
    std::string currentPath_;
    std::vector<std::string> displayItems_;
    int selectedIndex_;
    int scrollOffset_;
    int maxDisplayItems_;
    
    // Loading indicators
    std::unordered_map<std::string, double> loadingProgress_;
    std::unordered_map<std::string, std::string> loadingStatus_;
    
    // UI settings
    bool showHiddenFiles_;
    bool showFileSize_;
    bool showModifiedDate_;
    bool useColors_;
    bool useIcons_;
    
    // Performance tracking
    std::chrono::steady_clock::time_point lastUpdate_;
    size_t totalItemsLoaded_;
    
public:
    SmartFileBrowser();
    ~SmartFileBrowser();
    
    // Initialization
    bool initialize(const std::vector<std::string>& rootPaths);
    void shutdown();
    
    // Main interface
    void run();
    void refresh();
    void handleInput();
    
    // Navigation
    bool navigateToPath(const std::string& path);
    bool navigateUp();
    bool navigateInto(const std::string& item);
    bool navigateBack();
    bool navigateForward();
    
    // Display
    void displayCurrentDirectory();
    void displayHeader();
    void displayItems();
    void displayStatusBar();
    void displayLoadingIndicator(const std::string& path);
    
    // Selection and interaction
    void moveSelectionUp();
    void moveSelectionDown();
    void selectCurrentItem();
    void openCurrentItem();
    void showItemDetails();
    
    // Search
    void enterSearchMode();
    void performSearch(const std::string& query);
    void displaySearchResults(const std::vector<SearchResult>& results);
    
    // Settings
    void showSettings();
    void toggleHiddenFiles();
    void toggleFileDetails();
    void changeDisplayMode();
    
    // Callbacks for progressive loading
    void onDirectoryLoaded(const std::string& path, bool success);
    void onLoadingProgress(const std::string& path, double progress);
    void onFileFound(const FileEntry& file);
    
private:
    // Display helpers
    void clearScreen();
    void moveCursor(int row, int col);
    void setColor(const std::string& color);
    void resetColor();
    
    // Item formatting
    std::string formatItem(const std::string& path, bool isDirectory, 
                          uint64_t size = 0, std::time_t modified = 0);
    std::string getItemIcon(const std::string& path, bool isDirectory);
    std::string formatFileSize(uint64_t size);
    std::string formatDateTime(std::time_t timestamp);
    
    // Input handling
    char getKeyPress();
    std::string getStringInput(const std::string& prompt);
    bool handleSpecialKey(char key);
    
    // Loading management
    void updateLoadingIndicators();
    void showProgressBar(const std::string& path, double progress);
    bool isCurrentlyLoading() const;
    
    // Path utilities
    std::string getCurrentSelectedPath() const;
    bool isValidSelection() const;
    void updateDisplayItems();
    
    // Performance optimization
    void optimizeDisplay();
    void preloadVisibleItems();
    void scheduleBackgroundLoading();
};

// Console colors and formatting
class ConsoleFormatter {
public:
    static const std::string RESET;
    static const std::string BOLD;
    static const std::string DIM;
    
    // Colors
    static const std::string BLACK;
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string MAGENTA;
    static const std::string CYAN;
    static const std::string WHITE;
    
    // Bright colors
    static const std::string BRIGHT_BLACK;
    static const std::string BRIGHT_RED;
    static const std::string BRIGHT_GREEN;
    static const std::string BRIGHT_YELLOW;
    static const std::string BRIGHT_BLUE;
    static const std::string BRIGHT_MAGENTA;
    static const std::string BRIGHT_CYAN;
    static const std::string BRIGHT_WHITE;
    
    // Background colors
    static const std::string BG_BLACK;
    static const std::string BG_RED;
    static const std::string BG_GREEN;
    static const std::string BG_YELLOW;
    static const std::string BG_BLUE;
    static const std::string BG_MAGENTA;
    static const std::string BG_CYAN;
    static const std::string BG_WHITE;
    
    static void enableColors();
    static bool isColorSupported();
    static std::string colorize(const std::string& text, const std::string& color);
    static std::string highlight(const std::string& text);
    static std::string dim(const std::string& text);
};

// File icons for different types
class FileIcons {
public:
    static std::string getIcon(const std::string& path, bool isDirectory);
    static std::string getFolderIcon(bool isOpen = false);
    static std::string getFileIcon(const std::string& extension);
    
private:
    static std::unordered_map<std::string, std::string> extensionIcons_;
    static void initializeIcons();
};

// Progress indicators
class ProgressIndicator {
private:
    std::string title_;
    double progress_;
    int width_;
    bool isActive_;
    
public:
    ProgressIndicator(const std::string& title, int width = 30);
    
    void setProgress(double percentage);
    void setTitle(const std::string& title);
    void show();
    void hide();
    void update();
    
private:
    std::string createProgressBar(double percentage);
    void clearLine();
};

// Smart loading demo application
class SmartLoadingDemo {
private:
    std::unique_ptr<SmartFileBrowser> browser_;
    bool isRunning_;
    
public:
    SmartLoadingDemo();
    ~SmartLoadingDemo();
    
    int run(int argc, char* argv[]);
    
private:
    void showWelcome();
    void showHelp();
    void handleCommandLine(int argc, char* argv[]);
    void runInteractiveMode();
    void runBenchmarkMode();
    void runTestMode();
    
    // Demo scenarios
    void demoProgressiveLoading();
    void demoLazyLoading();
    void demoSearchPerformance();
    void demoMemoryEfficiency();
};

} // namespace UI
} // namespace FastFileSearch
