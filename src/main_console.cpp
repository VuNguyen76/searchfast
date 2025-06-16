#include "ui/console_ui.h"
#include "core/logger.h"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace FastFileSearch;
using namespace FastFileSearch::UI;

// Global variables for signal handling
std::atomic<bool> g_shouldExit(false);
std::unique_ptr<ConsoleUI> g_consoleUI;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\n\n🛑 Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_shouldExit.store(true);
    
    if (g_consoleUI) {
        g_consoleUI->shutdown();
    }
    
    exit(0);
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request
#ifndef _WIN32
    std::signal(SIGQUIT, signalHandler);  // Quit signal (Unix)
#endif
}

void setupLogging() {
    auto& logger = Logger::getInstance();
    logger.setLogLevel(Logger::Level::INFO);
    logger.setLogToConsole(false); // Disable console logging to avoid interference with UI
    logger.setLogToFile(true);
    logger.setLogFile("fastfilesearch_console.log");
    
    LOG_INFO("FastFileSearch Console UI starting");
}

void printStartupBanner() {
    std::cout << "🚀 FastFileSearch Console Interface" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "Initializing..." << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // Setup signal handlers
        setupSignalHandlers();
        
        // Setup logging
        setupLogging();
        
        // Print startup banner
        printStartupBanner();
        
        // Create and initialize console UI
        g_consoleUI = std::make_unique<ConsoleUI>();
        
        if (!g_consoleUI->initialize()) {
            std::cerr << "❌ Failed to initialize FastFileSearch!" << std::endl;
            return 1;
        }
        
        // Check for command line arguments
        if (argc > 1) {
            std::string command = argv[1];
            
            if (command == "search" && argc > 2) {
                // Direct search mode
                std::string query;
                for (int i = 2; i < argc; ++i) {
                    if (i > 2) query += " ";
                    query += argv[i];
                }
                
                std::cout << "Searching for: " << query << std::endl;
                
                // Perform search and exit
                auto results = g_consoleUI->getSearchManager()->search(query, SearchMode::Fuzzy);
                
                if (results.empty()) {
                    std::cout << "No results found." << std::endl;
                } else {
                    std::cout << "Found " << results.size() << " result(s):" << std::endl;
                    std::cout << std::string(50, '-') << std::endl;
                    
                    int count = 0;
                    for (const auto& result : results) {
                        if (count >= 10) break; // Limit to 10 results in direct mode
                        
                        std::cout << result.entry.fileName << std::endl;
                        std::cout << "  " << result.entry.fullPath << std::endl;
                        if (!result.entry.isDirectory()) {
                            std::cout << "  Size: " << result.entry.size << " bytes" << std::endl;
                        }
                        std::cout << std::endl;
                        count++;
                    }
                    
                    if (results.size() > 10) {
                        std::cout << "... and " << (results.size() - 10) << " more results." << std::endl;
                        std::cout << "Use interactive mode for full results." << std::endl;
                    }
                }
                
                return 0;
                
            } else if (command == "index") {
                // Direct indexing mode
                std::cout << "Starting indexing..." << std::endl;
                
                bool success = g_consoleUI->getSearchManager()->buildIndex();

                if (success) {
                    auto stats = g_consoleUI->getSearchManager()->getIndexStatistics();
                    std::cout << "✅ Indexing completed!" << std::endl;
                    std::cout << "Files indexed: " << stats.totalFiles << std::endl;
                    std::cout << "Directories: " << stats.totalDirectories << std::endl;
                } else {
                    std::cout << "❌ Indexing failed!" << std::endl;
                    return 1;
                }
                
                return 0;
                
            } else if (command == "status") {
                // Show status and exit
                auto stats = g_consoleUI->getSearchManager()->getIndexStatistics();
                
                std::cout << "FastFileSearch Status:" << std::endl;
                std::cout << "=====================" << std::endl;
                std::cout << "Files indexed: " << stats.totalFiles << std::endl;
                std::cout << "Directories: " << stats.totalDirectories << std::endl;
                std::cout << "Total size: " << (stats.totalSize / (1024 * 1024)) << " MB" << std::endl;
                
                if (stats.totalFiles == 0) {
                    std::cout << "No index available. Run 'fastfilesearch index' to build index." << std::endl;
                } else {
                    std::cout << "Index is ready for searching." << std::endl;
                }
                
                return 0;
                
            } else if (command == "help" || command == "--help" || command == "-h") {
                // Show help and exit
                std::cout << "FastFileSearch - High-Performance File Search" << std::endl;
                std::cout << "=============================================" << std::endl;
                std::cout << std::endl;
                std::cout << "Usage:" << std::endl;
                std::cout << "  fastfilesearch                    Start interactive mode" << std::endl;
                std::cout << "  fastfilesearch search <query>     Search for files" << std::endl;
                std::cout << "  fastfilesearch index              Build file index" << std::endl;
                std::cout << "  fastfilesearch status             Show index status" << std::endl;
                std::cout << "  fastfilesearch help               Show this help" << std::endl;
                std::cout << std::endl;
                std::cout << "Interactive Mode Commands:" << std::endl;
                std::cout << "  search <query>                    Search for files" << std::endl;
                std::cout << "  search:exact <query>              Exact search" << std::endl;
                std::cout << "  search:wildcard <pattern>         Wildcard search (*.txt)" << std::endl;
                std::cout << "  search:regex <pattern>            Regex search" << std::endl;
                std::cout << "  index                             Start indexing" << std::endl;
                std::cout << "  status                            Show current status" << std::endl;
                std::cout << "  results                           Show last search results" << std::endl;
                std::cout << "  export <file>                     Export results to file" << std::endl;
                std::cout << "  settings                          Show current settings" << std::endl;
                std::cout << "  help                              Show help" << std::endl;
                std::cout << "  exit                              Exit application" << std::endl;
                std::cout << std::endl;
                std::cout << "Examples:" << std::endl;
                std::cout << "  fastfilesearch search document.txt" << std::endl;
                std::cout << "  fastfilesearch search \"my file\"" << std::endl;
                std::cout << std::endl;
                
                return 0;
                
            } else {
                std::cerr << "Unknown command: " << command << std::endl;
                std::cerr << "Use 'fastfilesearch help' for usage information." << std::endl;
                return 1;
            }
        }
        
        // Run interactive mode
        LOG_INFO("Starting interactive console mode");
        g_consoleUI->run();
        
        LOG_INFO("FastFileSearch Console UI shutting down");
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        LOG_FATAL_F("Fatal error in main: {}", e.what());
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred." << std::endl;
        LOG_FATAL("Unknown fatal error in main");
        return 1;
    }
}

// Alternative entry point for testing
int testMode() {
    std::cout << "🧪 FastFileSearch Test Mode" << std::endl;
    std::cout << "===========================" << std::endl;
    
    try {
        ConsoleUI ui;
        
        if (!ui.initialize()) {
            std::cout << "❌ Initialization failed!" << std::endl;
            return 1;
        }
        
        std::cout << "✅ Initialization successful!" << std::endl;
        
        // Test basic functionality
        std::cout << "\n📊 Testing status display..." << std::endl;
        ui.showStatus();
        
        std::cout << "\n⚙️ Testing settings display..." << std::endl;
        ui.showSettings();
        
        std::cout << "\n✅ All tests completed!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
