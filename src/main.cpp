#include "app/search_manager.h"
#include "app/config_manager.h"
#include "core/logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using namespace FastFileSearch;

// Global variables for signal handling
std::atomic<bool> g_shouldExit(false);
std::unique_ptr<App::SearchManager> g_searchManager;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    LOG_INFO_F("Received signal {}, initiating graceful shutdown...", signal);
    g_shouldExit.store(true);
    
    if (g_searchManager) {
        g_searchManager->shutdown();
    }
}

// Print application banner
void printBanner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║                    Fast File Search v1.0.0                   ║
║                High-Performance File Indexing System         ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

// Print help information
void printHelp() {
    std::cout << R"(
Usage: FastFileSearch [OPTIONS] [COMMAND] [ARGS...]

Commands:
  search <query>          Search for files matching the query
  index                   Build the initial file index
  rebuild                 Rebuild the entire index
  watch                   Start file system monitoring
  stats                   Show indexing statistics
  config                  Show current configuration
  help                    Show this help message

Options:
  --config <path>         Use custom configuration file
  --mode <mode>           Search mode: exact, fuzzy, wildcard, regex
  --drives <drives>       Comma-separated list of drives to index (e.g., C:,D:)
  --max-results <num>     Maximum number of search results
  --verbose               Enable verbose logging
  --quiet                 Suppress output except errors
  --daemon                Run as background daemon
  --no-watch              Disable file system monitoring

Examples:
  FastFileSearch search "*.txt"
  FastFileSearch search --mode fuzzy "document"
  FastFileSearch index --drives C:,D:
  FastFileSearch rebuild
  FastFileSearch watch --daemon

)" << std::endl;
}

// Parse command line arguments
struct CommandLineArgs {
    std::string command;
    std::vector<std::string> args;
    std::string configPath;
    SearchMode searchMode = SearchMode::Fuzzy;
    std::vector<std::string> drives;
    uint32_t maxResults = 1000;
    bool verbose = false;
    bool quiet = false;
    bool daemon = false;
    bool noWatch = false;
};

CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            args.command = "help";
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                args.configPath = argv[++i];
            }
        } else if (arg == "--mode") {
            if (i + 1 < argc) {
                std::string mode = argv[++i];
                if (mode == "exact") args.searchMode = SearchMode::Exact;
                else if (mode == "fuzzy") args.searchMode = SearchMode::Fuzzy;
                else if (mode == "wildcard") args.searchMode = SearchMode::Wildcard;
                else if (mode == "regex") args.searchMode = SearchMode::Regex;
            }
        } else if (arg == "--drives") {
            if (i + 1 < argc) {
                std::string drivesStr = argv[++i];
                // Parse comma-separated drives
                size_t pos = 0;
                while (pos < drivesStr.length()) {
                    size_t comma = drivesStr.find(',', pos);
                    if (comma == std::string::npos) {
                        args.drives.push_back(drivesStr.substr(pos));
                        break;
                    }
                    args.drives.push_back(drivesStr.substr(pos, comma - pos));
                    pos = comma + 1;
                }
            }
        } else if (arg == "--max-results") {
            if (i + 1 < argc) {
                args.maxResults = std::stoul(argv[++i]);
            }
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            args.quiet = true;
        } else if (arg == "--daemon" || arg == "-d") {
            args.daemon = true;
        } else if (arg == "--no-watch") {
            args.noWatch = true;
        } else if (arg[0] != '-') {
            // This is a command or argument
            if (args.command.empty()) {
                args.command = arg;
            } else {
                args.args.push_back(arg);
            }
        }
    }
    
    return args;
}

// Initialize logging
void initializeLogging(bool verbose, bool quiet) {
    auto& logger = Logger::getInstance();
    
    if (quiet) {
        logger.setLogLevel(Logger::Level::ERROR);
        logger.setLogToConsole(false);
    } else if (verbose) {
        logger.setLogLevel(Logger::Level::DEBUG);
        logger.setLogToConsole(true);
    } else {
        logger.setLogLevel(Logger::Level::INFO);
        logger.setLogToConsole(true);
    }
    
    logger.setLogFile("fastfilesearch.log");
    logger.setLogToFile(true);
}

// Execute search command
int executeSearch(const CommandLineArgs& args, App::SearchManager& searchManager) {
    if (args.args.empty()) {
        std::cerr << "Error: Search query is required" << std::endl;
        return 1;
    }
    
    std::string query = args.args[0];
    
    try {
        SearchQuery searchQuery;
        searchQuery.query = query;
        searchQuery.mode = args.searchMode;
        searchQuery.maxResults = args.maxResults;
        
        LOG_INFO_F("Searching for: '{}'", query);
        auto startTime = std::chrono::high_resolution_clock::now();
        
        SearchResults results = searchManager.search(searchQuery);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::cout << "Found " << results.size() << " results in " << duration.count() << "ms:" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (const auto& result : results) {
            std::cout << result.entry.fullPath;
            if (result.score > 0) {
                std::cout << " (score: " << std::fixed << std::setprecision(2) << result.score << ")";
            }
            std::cout << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Search error: " << e.what() << std::endl;
        return 1;
    }
}

// Execute index command
int executeIndex(const CommandLineArgs& args, App::SearchManager& searchManager) {
    try {
        LOG_INFO("Starting initial indexing...");
        
        bool success;
        if (!args.drives.empty()) {
            success = searchManager.buildIndex(args.drives);
        } else {
            success = searchManager.buildIndex();
        }
        
        if (success) {
            std::cout << "Indexing completed successfully." << std::endl;
            
            auto stats = searchManager.getIndexStatistics();
            std::cout << "Indexed " << stats.totalFiles << " files and " 
                     << stats.totalDirectories << " directories." << std::endl;
            return 0;
        } else {
            std::cerr << "Indexing failed." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Indexing error: " << e.what() << std::endl;
        return 1;
    }
}

// Execute watch command
int executeWatch(const CommandLineArgs& args, App::SearchManager& searchManager) {
    try {
        LOG_INFO("Starting file system monitoring...");
        
        bool success;
        if (!args.drives.empty()) {
            success = searchManager.startFileWatching(args.drives);
        } else {
            success = searchManager.startFileWatching();
        }
        
        if (!success) {
            std::cerr << "Failed to start file watching." << std::endl;
            return 1;
        }
        
        std::cout << "File system monitoring started." << std::endl;
        
        if (args.daemon) {
            std::cout << "Running in daemon mode. Press Ctrl+C to stop." << std::endl;
            
            while (!g_shouldExit.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "File watching error: " << e.what() << std::endl;
        return 1;
    }
}

// Execute stats command
int executeStats(const CommandLineArgs& args, App::SearchManager& searchManager) {
    try {
        auto stats = searchManager.getIndexStatistics();
        
        std::cout << "Index Statistics:" << std::endl;
        std::cout << std::string(40, '=') << std::endl;
        std::cout << "Total Files: " << stats.totalFiles << std::endl;
        std::cout << "Total Directories: " << stats.totalDirectories << std::endl;
        std::cout << "Total Size: " << (stats.totalSize / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "Indexed Drives: " << stats.indexedDrives << std::endl;
        std::cout << "Last Full Scan: " << std::ctime(&stats.lastFullScan);
        std::cout << "Last Update: " << std::ctime(&stats.lastUpdate);
        std::cout << "Indexing Progress: " << std::fixed << std::setprecision(1) 
                 << (stats.indexingProgress * 100) << "%" << std::endl;
        std::cout << "Currently Indexing: " << (stats.isIndexing ? "Yes" : "No") << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Stats error: " << e.what() << std::endl;
        return 1;
    }
}

// Main application entry point
int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Parse command line arguments
        CommandLineArgs args = parseCommandLine(argc, argv);
        
        // Initialize logging
        initializeLogging(args.verbose, args.quiet);
        
        if (!args.quiet) {
            printBanner();
        }
        
        // Handle help command
        if (args.command == "help" || args.command.empty()) {
            printHelp();
            return 0;
        }
        
        LOG_INFO("FastFileSearch starting up...");
        
        // Initialize configuration
        App::ConfigManager configManager;
        if (!args.configPath.empty()) {
            if (!configManager.load(args.configPath)) {
                std::cerr << "Failed to load configuration from: " << args.configPath << std::endl;
                return 1;
            }
        } else {
            configManager.load(); // Load default config
        }
        
        // Initialize search manager
        g_searchManager = std::make_unique<App::SearchManager>(configManager.getSettings());
        
        if (!g_searchManager->initialize()) {
            std::cerr << "Failed to initialize search manager." << std::endl;
            return 1;
        }
        
        // Execute the requested command
        int result = 0;
        
        if (args.command == "search") {
            result = executeSearch(args, *g_searchManager);
        } else if (args.command == "index") {
            result = executeIndex(args, *g_searchManager);
        } else if (args.command == "rebuild") {
            LOG_INFO("Rebuilding index...");
            if (g_searchManager->rebuildIndex()) {
                std::cout << "Index rebuilt successfully." << std::endl;
            } else {
                std::cerr << "Failed to rebuild index." << std::endl;
                result = 1;
            }
        } else if (args.command == "watch") {
            result = executeWatch(args, *g_searchManager);
        } else if (args.command == "stats") {
            result = executeStats(args, *g_searchManager);
        } else if (args.command == "config") {
            std::cout << "Configuration:" << std::endl;
            std::cout << configManager.exportToJSON() << std::endl;
        } else {
            std::cerr << "Unknown command: " << args.command << std::endl;
            printHelp();
            result = 1;
        }
        
        // Cleanup
        LOG_INFO("Shutting down...");
        g_searchManager->shutdown();
        g_searchManager.reset();
        
        LOG_INFO("FastFileSearch shutdown complete.");
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        LOG_FATAL_F("Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred." << std::endl;
        LOG_FATAL("Unknown fatal error occurred.");
        return 1;
    }
}
