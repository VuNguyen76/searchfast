#include "ui/console_ui.h"
#include "core/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace FastFileSearch {
namespace UI {

// Console Colors implementation
const std::string ConsoleColors::RESET = "\033[0m";
const std::string ConsoleColors::BLACK = "\033[30m";
const std::string ConsoleColors::RED = "\033[31m";
const std::string ConsoleColors::GREEN = "\033[32m";
const std::string ConsoleColors::YELLOW = "\033[33m";
const std::string ConsoleColors::BLUE = "\033[34m";
const std::string ConsoleColors::MAGENTA = "\033[35m";
const std::string ConsoleColors::CYAN = "\033[36m";
const std::string ConsoleColors::WHITE = "\033[37m";
const std::string ConsoleColors::BRIGHT_BLACK = "\033[90m";
const std::string ConsoleColors::BRIGHT_RED = "\033[91m";
const std::string ConsoleColors::BRIGHT_GREEN = "\033[92m";
const std::string ConsoleColors::BRIGHT_YELLOW = "\033[93m";
const std::string ConsoleColors::BRIGHT_BLUE = "\033[94m";
const std::string ConsoleColors::BRIGHT_MAGENTA = "\033[95m";
const std::string ConsoleColors::BRIGHT_CYAN = "\033[96m";
const std::string ConsoleColors::BRIGHT_WHITE = "\033[97m";

void ConsoleColors::enableColors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

bool ConsoleColors::isColorSupported() {
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(STDOUT_FILENO);
#endif
}

// ProgressBar implementation
ProgressBar::ProgressBar(const std::string& title, int width)
    : width_(width), title_(title), progress_(0.0), active_(false) {
}

void ProgressBar::setProgress(double percentage) {
    progress_.store(std::max(0.0, std::min(100.0, percentage)));
}

void ProgressBar::setStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    currentStatus_ = status;
}

void ProgressBar::show() {
    active_.store(true);
}

void ProgressBar::hide() {
    active_.store(false);
    clearLine();
}

void ProgressBar::update() {
    if (!active_.load()) return;
    
    double progress = progress_.load();
    std::string status;
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        status = currentStatus_;
    }
    
    clearLine();
    
    std::cout << title_ << ": " << createBar(progress) 
              << " " << std::fixed << std::setprecision(1) << progress << "%";
    
    if (!status.empty()) {
        std::cout << " - " << status;
    }
    
    std::cout << std::flush;
}

void ProgressBar::clearLine() {
    std::cout << "\r" << std::string(120, ' ') << "\r";
}

std::string ProgressBar::createBar(double percentage) {
    int filled = static_cast<int>(percentage * width_ / 100.0);
    int empty = width_ - filled;
    
    std::string bar = "[";
    bar += std::string(filled, '#');
    bar += std::string(empty, '-');
    bar += "]";
    
    return bar;
}

// ConsoleFileIcons implementation
std::string ConsoleFileIcons::getFileIcon(const FileEntry& entry) {
    if (entry.isDirectory()) {
        return getFolderIcon();
    }
    return getFileTypeIcon(entry.extension);
}

std::string ConsoleFileIcons::getFolderIcon() {
    return "📁";
}

std::string ConsoleFileIcons::getFileTypeIcon(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Text files
    if (ext == "txt" || ext == "md" || ext == "readme") return "📄";
    
    // Images
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "bmp") return "🖼️";
    
    // Audio
    if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "aac") return "🎵";
    
    // Video
    if (ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "mov") return "🎬";
    
    // Archives
    if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar") return "📦";
    
    // Executables
    if (ext == "exe" || ext == "msi" || ext == "app") return "⚙️";
    
    // Documents
    if (ext == "pdf" || ext == "doc" || ext == "docx") return "📋";
    
    // Code files
    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "py" || ext == "js") return "💻";
    
    return "📄"; // Default file icon
}

// ConsoleUI implementation
ConsoleUI::ConsoleUI()
    : isIndexing_(false), isRunning_(false), indexingProgress_(0.0)
    , currentResults_("")
    , useColors_(true), useUnicodeIcons_(true), maxDisplayResults_(20) {
    
    detectCapabilities();
    
    if (useColors_) {
        ConsoleColors::enableColors();
    }
}

ConsoleUI::~ConsoleUI() {
    shutdown();
}

bool ConsoleUI::initialize() {
    printInfo("Initializing FastFileSearch...");
    
    // Setup search manager
    AppSettings settings;
    settings.setDefaults();
    
    searchManager_ = std::make_unique<App::SearchManager>(settings);
    
    if (!searchManager_->initialize()) {
        printError("Failed to initialize search manager!");
        return false;
    }
    
    // Setup callbacks
    searchManager_->setIndexingProgressCallback(
        [this](double percentage, const std::string& currentPath) {
            onIndexingProgress(percentage, currentPath);
        });
    
    searchManager_->setIndexingCompletedCallback(
        [this](bool success, const std::string& message) {
            onIndexingCompleted(success, message);
        });
    
    printSuccess("FastFileSearch initialized successfully!");
    return true;
}

void ConsoleUI::run() {
    isRunning_.store(true);
    
    showWelcome();
    runInteractiveMode();
}

void ConsoleUI::shutdown() {
    isRunning_.store(false);
    
    if (progressBar_) {
        progressBar_->hide();
    }
    
    if (searchManager_) {
        searchManager_->shutdown();
    }
    
    printInfo("FastFileSearch shutdown complete.");
}

void ConsoleUI::showWelcome() {
    printSeparator('=', 80);
    std::cout << ConsoleColors::BRIGHT_CYAN;
    std::cout << R"(
    ███████╗ █████╗ ███████╗████████╗    ███████╗██╗██╗     ███████╗
    ██╔════╝██╔══██╗██╔════╝╚══██╔══╝    ██╔════╝██║██║     ██╔════╝
    █████╗  ███████║███████╗   ██║       █████╗  ██║██║     █████╗  
    ██╔══╝  ██╔══██║╚════██║   ██║       ██╔══╝  ██║██║     ██╔══╝  
    ██║     ██║  ██║███████║   ██║       ██║     ██║███████╗███████╗
    ╚═╝     ╚═╝  ╚═╝╚══════╝   ╚═╝       ╚═╝     ╚═╝╚══════╝╚══════╝
    
                    ███████╗███████╗ █████╗ ██████╗  ██████╗██╗  ██╗
                    ██╔════╝██╔════╝██╔══██╗██╔══██╗██╔════╝██║  ██║
                    ███████╗█████╗  ███████║██████╔╝██║     ███████║
                    ╚════██║██╔══╝  ██╔══██║██╔══██╗██║     ██╔══██║
                    ███████║███████╗██║  ██║██║  ██║╚██████╗██║  ██║
                    ╚══════╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝
)" << ConsoleColors::RESET << std::endl;
    
    std::cout << ConsoleColors::BRIGHT_WHITE << "    High-Performance File Search Application v1.0.0" << ConsoleColors::RESET << std::endl;
    std::cout << ConsoleColors::YELLOW << "    Built with C++20 • Lightning Fast • Cross-Platform" << ConsoleColors::RESET << std::endl;
    printSeparator('=', 80);
    
    printInfo("Welcome to FastFileSearch! Type 'help' for available commands.");
    
    // Show current status
    showStatus();
}

void ConsoleUI::showHelp() {
    printHeader("Available Commands");
    
    std::vector<std::vector<std::string>> commands = {
        {"search <query>", "Search for files (supports fuzzy, wildcard, regex)"},
        {"index", "Start indexing all drives"},
        {"status", "Show current indexing status"},
        {"results", "Show last search results"},
        {"export <file>", "Export search results to file"},
        {"settings", "Show current settings"},
        {"clear", "Clear the screen"},
        {"help", "Show this help message"},
        {"exit", "Exit the application"}
    };
    
    std::vector<std::string> headers = {"Command", "Description"};
    printTable(commands, headers);
    
    std::cout << std::endl;
    printInfo("Search modes: Use 'search:fuzzy', 'search:exact', 'search:wildcard', or 'search:regex'");
    printInfo("Examples:");
    std::cout << "  " << ConsoleColors::CYAN << "search document.txt" << ConsoleColors::RESET << " - Fuzzy search for document.txt" << std::endl;
    std::cout << "  " << ConsoleColors::CYAN << "search:wildcard *.cpp" << ConsoleColors::RESET << " - Find all C++ files" << std::endl;
    std::cout << "  " << ConsoleColors::CYAN << "search:regex \\.(jpg|png)$" << ConsoleColors::RESET << " - Find image files" << std::endl;
}

void ConsoleUI::showStatus() {
    printHeader("System Status");
    
    auto stats = searchManager_->getIndexStatistics();
    
    std::cout << "📊 " << ConsoleColors::BRIGHT_WHITE << "Index Statistics:" << ConsoleColors::RESET << std::endl;
    std::cout << "   Files indexed: " << ConsoleColors::GREEN << stats.totalFiles << ConsoleColors::RESET << std::endl;
    std::cout << "   Directories: " << ConsoleColors::GREEN << stats.totalDirectories << ConsoleColors::RESET << std::endl;
    std::cout << "   Total size: " << ConsoleColors::GREEN << formatFileSize(stats.totalSize) << ConsoleColors::RESET << std::endl;
    
    if (isIndexing_.load()) {
        std::cout << "   Status: " << ConsoleColors::YELLOW << "Indexing in progress (" 
                  << std::fixed << std::setprecision(1) << indexingProgress_.load() << "%)" << ConsoleColors::RESET << std::endl;
    } else if (stats.totalFiles > 0) {
        std::cout << "   Status: " << ConsoleColors::GREEN << "Ready for search" << ConsoleColors::RESET << std::endl;
    } else {
        std::cout << "   Status: " << ConsoleColors::YELLOW << "No index available - run 'index' command" << ConsoleColors::RESET << std::endl;
    }
    
    std::cout << std::endl;
    
    // Memory usage
    size_t memoryUsage = searchManager_->getMemoryUsage();
    std::cout << "💾 " << ConsoleColors::BRIGHT_WHITE << "Memory Usage: " << ConsoleColors::RESET 
              << ConsoleColors::CYAN << formatFileSize(memoryUsage) << ConsoleColors::RESET << std::endl;
    
    std::cout << std::endl;
}

void ConsoleUI::runInteractiveMode() {
    while (isRunning_.load()) {
        try {
            std::string input = getInput(ConsoleColors::BRIGHT_GREEN + "FastFileSearch> " + ConsoleColors::RESET);
            
            if (input.empty()) continue;
            
            processCommand(input);
            
        } catch (const std::exception& e) {
            printError(std::string("Command error: ") + e.what());
        }
    }
}

std::string ConsoleUI::getInput(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    
    // Trim whitespace
    input.erase(0, input.find_first_not_of(" \t\r\n"));
    input.erase(input.find_last_not_of(" \t\r\n") + 1);
    
    return input;
}

void ConsoleUI::processCommand(const std::string& command) {
    auto args = parseCommand(command);
    if (args.empty()) return;
    
    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "help" || cmd == "h" || cmd == "?") {
        showHelp();
    } else if (cmd == "exit" || cmd == "quit" || cmd == "q") {
        isRunning_.store(false);
    } else if (cmd == "clear" || cmd == "cls") {
        system("clear || cls");
        showWelcome();
    } else if (cmd == "status") {
        showStatus();
    } else if (cmd == "index") {
        executeIndexCommand(args);
    } else if (cmd.find("search") == 0) {
        executeSearchCommand(args);
    } else if (cmd == "results") {
        showSearchResults();
    } else if (cmd == "export") {
        executeExportCommand(args);
    } else if (cmd == "settings") {
        showSettings();
    } else {
        printError("Unknown command: " + cmd + ". Type 'help' for available commands.");
    }
}

void ConsoleUI::startIndexing() {
    if (isIndexing_.load()) {
        printWarning("Indexing is already in progress!");
        return;
    }

    printInfo("Starting comprehensive file system indexing...");
    printWarning("This may take several minutes depending on your system.");

    progressBar_ = std::make_unique<ProgressBar>("Indexing", 50);
    progressBar_->show();

    isIndexing_.store(true);
    indexingProgress_.store(0.0);

    // Start indexing in background
    std::thread indexingThread([this]() {
        try {
            bool success = searchManager_->buildIndex();
            if (!success && isIndexing_.load()) {
                onIndexingCompleted(false, "Indexing failed");
            }
        } catch (const std::exception& e) {
            onIndexingCompleted(false, std::string("Indexing error: ") + e.what());
        }
    });

    indexingThread.detach();

    // Monitor progress
    while (isIndexing_.load()) {
        progressBar_->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ConsoleUI::performSearch() {
    std::string query = getInput("Enter search query: ");
    if (query.empty()) {
        printWarning("Search query cannot be empty.");
        return;
    }

    printInfo("Searching for: " + query);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto results = searchManager_->search(query, SearchMode::Fuzzy);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    {
        std::lock_guard<std::mutex> lock(resultsMutex_);
        currentResults_ = results;
    }

    std::cout << std::endl;
    printSuccess("Search completed in " + std::to_string(searchTime.count()) + "ms");

    if (results.empty()) {
        printWarning("No files found matching your query.");
    } else {
        std::cout << "Found " << ConsoleColors::BRIGHT_GREEN << results.size()
                  << ConsoleColors::RESET << " result(s)" << std::endl;
        showSearchResults();
    }
}

void ConsoleUI::showSearchResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);

    if (currentResults_.empty()) {
        printWarning("No search results to display. Run a search first.");
        return;
    }

    printHeader("Search Results");

    const auto& results = currentResults_.getResults();
    int displayCount = std::min(maxDisplayResults_, static_cast<int>(results.size()));

    for (int i = 0; i < displayCount; ++i) {
        displayFileEntry(results[i].entry, results[i].score);
    }

    if (results.size() > maxDisplayResults_) {
        std::cout << std::endl;
        printInfo("Showing first " + std::to_string(maxDisplayResults_) + " of " +
                 std::to_string(results.size()) + " results.");
        printInfo("Use 'export' command to save all results to a file.");
    }
}

void ConsoleUI::displayFileEntry(const FileEntry& entry, double score) {
    std::string icon = useUnicodeIcons_ ? ConsoleFileIcons::getFileIcon(entry) :
                      (entry.isDirectory() ? "[DIR]" : "[FILE]");

    std::cout << icon << " ";

    if (entry.isDirectory()) {
        std::cout << ConsoleColors::BRIGHT_BLUE;
    } else {
        std::cout << ConsoleColors::WHITE;
    }

    std::cout << entry.fileName << ConsoleColors::RESET;

    if (score > 0.0) {
        std::cout << " " << ConsoleColors::BRIGHT_YELLOW << "(score: "
                  << std::fixed << std::setprecision(2) << score << ")" << ConsoleColors::RESET;
    }

    std::cout << std::endl;

    std::cout << "  " << ConsoleColors::BRIGHT_BLACK << "Path: " << ConsoleColors::RESET
              << entry.fullPath << std::endl;

    if (!entry.isDirectory()) {
        std::cout << "  " << ConsoleColors::BRIGHT_BLACK << "Size: " << ConsoleColors::RESET
                  << formatFileSize(entry.size);
        std::cout << " | " << ConsoleColors::BRIGHT_BLACK << "Modified: " << ConsoleColors::RESET
                  << formatDateTime(entry.lastModified) << std::endl;
    }

    std::cout << std::endl;
}

std::string ConsoleUI::formatFileSize(uint64_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double displaySize = static_cast<double>(size);

    while (displaySize >= 1024.0 && unitIndex < 4) {
        displaySize /= 1024.0;
        unitIndex++;
    }

    std::ostringstream oss;
    if (unitIndex == 0) {
        oss << static_cast<int>(displaySize) << " " << units[unitIndex];
    } else {
        oss << std::fixed << std::setprecision(1) << displaySize << " " << units[unitIndex];
    }
    return oss.str();
}

std::string ConsoleUI::formatDateTime(std::time_t timestamp) {
    std::tm* timeinfo = std::localtime(&timestamp);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void ConsoleUI::onIndexingProgress(double percentage, const std::string& currentPath) {
    indexingProgress_.store(percentage);

    {
        std::lock_guard<std::mutex> lock(pathMutex_);
        currentIndexingPath_ = currentPath;
    }

    if (progressBar_) {
        progressBar_->setProgress(percentage);

        // Show current path occasionally
        if (!currentPath.empty() && static_cast<int>(percentage) % 5 == 0) {
            std::string shortPath = currentPath;
            if (shortPath.length() > 60) {
                shortPath = "..." + shortPath.substr(shortPath.length() - 57);
            }
            progressBar_->setStatus(shortPath);
        }
    }
}

void ConsoleUI::onIndexingCompleted(bool success, const std::string& message) {
    isIndexing_.store(false);

    if (progressBar_) {
        progressBar_->hide();
        progressBar_.reset();
    }

    std::cout << std::endl;

    if (success) {
        printSuccess("Indexing completed successfully!");

        auto stats = searchManager_->getIndexStatistics();
        std::cout << "📊 " << ConsoleColors::BRIGHT_WHITE << "Results:" << ConsoleColors::RESET << std::endl;
        std::cout << "   Files indexed: " << ConsoleColors::GREEN << stats.totalFiles << ConsoleColors::RESET << std::endl;
        std::cout << "   Directories: " << ConsoleColors::GREEN << stats.totalDirectories << ConsoleColors::RESET << std::endl;
        std::cout << "   Total size: " << ConsoleColors::GREEN << formatFileSize(stats.totalSize) << ConsoleColors::RESET << std::endl;

        printInfo("You can now search for files using the 'search' command.");
    } else {
        printError("Indexing failed: " + message);
    }

    std::cout << std::endl;
}

void ConsoleUI::printHeader(const std::string& title) {
    std::cout << std::endl;
    printSeparator('=', 80);
    std::cout << ConsoleColors::BRIGHT_WHITE << "  " << title << ConsoleColors::RESET << std::endl;
    printSeparator('=', 80);
}

void ConsoleUI::printSeparator(char character, int length) {
    std::cout << std::string(length, character) << std::endl;
}

void ConsoleUI::printError(const std::string& message) {
    std::cout << ConsoleColors::BRIGHT_RED << "❌ Error: " << message << ConsoleColors::RESET << std::endl;
}

void ConsoleUI::printSuccess(const std::string& message) {
    std::cout << ConsoleColors::BRIGHT_GREEN << "✅ " << message << ConsoleColors::RESET << std::endl;
}

void ConsoleUI::printWarning(const std::string& message) {
    std::cout << ConsoleColors::BRIGHT_YELLOW << "⚠️  Warning: " << message << ConsoleColors::RESET << std::endl;
}

void ConsoleUI::printInfo(const std::string& message) {
    std::cout << ConsoleColors::BRIGHT_CYAN << "ℹ️  " << message << ConsoleColors::RESET << std::endl;
}

void ConsoleUI::detectCapabilities() {
    useColors_ = ConsoleColors::isColorSupported();

    // Check for Unicode support (simplified check)
    useUnicodeIcons_ = true; // Assume Unicode support for now

#ifdef _WIN32
    // On Windows, check console code page
    if (GetConsoleOutputCP() != CP_UTF8) {
        useUnicodeIcons_ = false;
    }
#endif
}

std::vector<std::string> ConsoleUI::parseCommand(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

void ConsoleUI::executeSearchCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        performSearch();
        return;
    }

    // Parse search mode from command
    SearchMode mode = SearchMode::Fuzzy;
    std::string command = args[0];

    if (command == "search:exact") {
        mode = SearchMode::Exact;
    } else if (command == "search:fuzzy") {
        mode = SearchMode::Fuzzy;
    } else if (command == "search:wildcard") {
        mode = SearchMode::Wildcard;
    } else if (command == "search:regex") {
        mode = SearchMode::Regex;
    }

    // Get query from remaining arguments
    std::string query;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) query += " ";
        query += args[i];
    }

    if (query.empty()) {
        query = getInput("Enter search query: ");
        if (query.empty()) {
            printWarning("Search query cannot be empty.");
            return;
        }
    }

    printInfo("Searching for: " + query);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto results = searchManager_->search(query, mode);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    {
        std::lock_guard<std::mutex> lock(resultsMutex_);
        currentResults_ = results;
    }

    std::cout << std::endl;
    printSuccess("Search completed in " + std::to_string(searchTime.count()) + "ms");

    if (results.empty()) {
        printWarning("No files found matching your query.");
    } else {
        std::cout << "Found " << ConsoleColors::BRIGHT_GREEN << results.size()
                  << ConsoleColors::RESET << " result(s)" << std::endl;
        showSearchResults();
    }
}

void ConsoleUI::executeIndexCommand(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    startIndexing();
}

void ConsoleUI::executeExportCommand(const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(resultsMutex_);

    if (currentResults_.empty()) {
        printWarning("No search results to export. Run a search first.");
        return;
    }

    std::string filename;
    if (args.size() > 1) {
        filename = args[1];
    } else {
        filename = getInput("Enter export filename (e.g., results.csv): ");
        if (filename.empty()) {
            printWarning("Export cancelled.");
            return;
        }
    }

    // Simple CSV export
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            printError("Cannot create file: " + filename);
            return;
        }

        // Write CSV header
        file << "Name,Path,Size,Modified,Type,Score\n";

        // Write results
        for (const auto& result : currentResults_.getResults()) {
            file << "\"" << result.entry.fileName << "\","
                 << "\"" << result.entry.fullPath << "\","
                 << result.entry.size << ","
                 << "\"" << formatDateTime(result.entry.lastModified) << "\","
                 << "\"" << (result.entry.isDirectory() ? "Directory" : "File") << "\","
                 << std::fixed << std::setprecision(3) << result.score << "\n";
        }

        file.close();
        printSuccess("Results exported to: " + filename);

    } catch (const std::exception& e) {
        printError("Export failed: " + std::string(e.what()));
    }
}

void ConsoleUI::showSettings() {
    printHeader("Current Settings");

    const auto& settings = searchManager_->getSettings();

    std::cout << "🔧 " << ConsoleColors::BRIGHT_WHITE << "Search Settings:" << ConsoleColors::RESET << std::endl;
    std::cout << "   Max results: " << ConsoleColors::CYAN << settings.maxSearchResults << ConsoleColors::RESET << std::endl;
    std::cout << "   Fuzzy threshold: " << ConsoleColors::CYAN << settings.fuzzyThreshold << ConsoleColors::RESET << std::endl;
    std::cout << "   Enable fuzzy search: " << ConsoleColors::CYAN << (settings.enableFuzzySearch ? "Yes" : "No") << ConsoleColors::RESET << std::endl;

    std::cout << std::endl;
    std::cout << "📁 " << ConsoleColors::BRIGHT_WHITE << "Indexing Settings:" << ConsoleColors::RESET << std::endl;
    std::cout << "   Indexing threads: " << ConsoleColors::CYAN << settings.indexingThreads << ConsoleColors::RESET << std::endl;
    std::cout << "   Max memory usage: " << ConsoleColors::CYAN << settings.maxMemoryUsage << " MB" << ConsoleColors::RESET << std::endl;
    std::cout << "   Index hidden files: " << ConsoleColors::CYAN << (settings.indexHiddenFiles ? "Yes" : "No") << ConsoleColors::RESET << std::endl;

    std::cout << std::endl;
    std::cout << "💾 " << ConsoleColors::BRIGHT_WHITE << "Performance Settings:" << ConsoleColors::RESET << std::endl;
    std::cout << "   Enable cache: " << ConsoleColors::CYAN << (settings.enableCache ? "Yes" : "No") << ConsoleColors::RESET << std::endl;
    std::cout << "   Cache size: " << ConsoleColors::CYAN << settings.cacheSize << " MB" << ConsoleColors::RESET << std::endl;

    std::cout << std::endl;
    std::cout << "🎨 " << ConsoleColors::BRIGHT_WHITE << "UI Settings:" << ConsoleColors::RESET << std::endl;
    std::cout << "   Colors enabled: " << ConsoleColors::CYAN << (useColors_ ? "Yes" : "No") << ConsoleColors::RESET << std::endl;
    std::cout << "   Unicode icons: " << ConsoleColors::CYAN << (useUnicodeIcons_ ? "Yes" : "No") << ConsoleColors::RESET << std::endl;
    std::cout << "   Max display results: " << ConsoleColors::CYAN << maxDisplayResults_ << ConsoleColors::RESET << std::endl;

    std::cout << std::endl;
}

void ConsoleUI::printTable(const std::vector<std::vector<std::string>>& data,
                          const std::vector<std::string>& headers) {
    if (data.empty() || headers.empty()) return;

    // Calculate column widths
    std::vector<size_t> widths(headers.size(), 0);

    // Check header widths
    for (size_t i = 0; i < headers.size(); ++i) {
        widths[i] = std::max(widths[i], headers[i].length());
    }

    // Check data widths
    for (const auto& row : data) {
        for (size_t i = 0; i < std::min(row.size(), widths.size()); ++i) {
            widths[i] = std::max(widths[i], row[i].length());
        }
    }

    // Print header
    std::cout << ConsoleColors::BRIGHT_WHITE;
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << std::left << std::setw(widths[i] + 2) << headers[i];
    }
    std::cout << ConsoleColors::RESET << std::endl;

    // Print separator
    for (size_t i = 0; i < headers.size(); ++i) {
        std::cout << std::string(widths[i] + 2, '-');
    }
    std::cout << std::endl;

    // Print data
    for (const auto& row : data) {
        for (size_t i = 0; i < std::min(row.size(), widths.size()); ++i) {
            std::cout << std::left << std::setw(widths[i] + 2) << row[i];
        }
        std::cout << std::endl;
    }
}

} // namespace UI
} // namespace FastFileSearch
