#include "ui/main_window.h"
#include "core/logger.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSplashScreen>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QFont>
#include <iostream>

using namespace FastFileSearch;

// Create a simple splash screen
QSplashScreen* createSplashScreen() {
    // Create a simple splash screen pixmap
    QPixmap splashPixmap(400, 300);
    splashPixmap.fill(QColor(43, 43, 43)); // Dark background
    
    QPainter painter(&splashPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw title
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(QFont("Arial", 24, QFont::Bold));
    painter.drawText(splashPixmap.rect(), Qt::AlignCenter, "FastFileSearch");
    
    // Draw subtitle
    painter.setFont(QFont("Arial", 12));
    painter.drawText(QRect(0, 200, 400, 30), Qt::AlignCenter, "High-Performance File Search");
    
    // Draw version
    painter.setFont(QFont("Arial", 10));
    painter.drawText(QRect(0, 250, 400, 20), Qt::AlignCenter, "Version 1.0.0");
    
    // Draw loading text
    painter.setPen(QColor(0, 120, 212));
    painter.setFont(QFont("Arial", 10));
    painter.drawText(QRect(0, 270, 400, 20), Qt::AlignCenter, "Loading...");
    
    QSplashScreen* splash = new QSplashScreen(splashPixmap);
    splash->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    return splash;
}

void setupApplication(QApplication& app) {
    // Set application properties
    app.setApplicationName("FastFileSearch");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("FastFileSearch Team");
    app.setOrganizationDomain("fastfilesearch.org");
    
    // Set application icon
    // app.setWindowIcon(QIcon(":/icons/app_icon.png"));
    
    // Enable high DPI support
    app.setAttribute(Qt::AA_EnableHighDpiScaling);
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
}

void setupLogging() {
    auto& logger = Logger::getInstance();
    
    // Create logs directory
    QString logsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(logsDir);
    
    // Set up logging
    QString logFile = logsDir + "/fastfilesearch_gui.log";
    logger.setLogFile(logFile.toStdString());
    logger.setLogLevel(Logger::Level::INFO);
    logger.setLogToConsole(true);
    logger.setLogToFile(true);
    
    LOG_INFO("FastFileSearch GUI starting up");
    LOG_INFO_F("Log file: {}", logFile.toStdString());
}

bool checkSystemRequirements() {
    // Check if we have write access to app data directory
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.mkpath(appDataDir)) {
        QMessageBox::critical(nullptr, "System Requirements", 
                             "Cannot create application data directory.\n"
                             "Please check your permissions.");
        return false;
    }
    
    // Check available disk space (at least 100MB)
    QStorageInfo storage(appDataDir);
    if (storage.bytesAvailable() < 100 * 1024 * 1024) {
        QMessageBox::warning(nullptr, "Disk Space Warning",
                            "Low disk space available. FastFileSearch may not work properly.\n"
                            "Please free up some disk space.");
    }
    
    return true;
}

void showStartupError(const QString& error) {
    QMessageBox::critical(nullptr, "Startup Error", 
                         QString("FastFileSearch failed to start:\n\n%1").arg(error));
}

int main(int argc, char* argv[]) {
    // Create QApplication
    QApplication app(argc, argv);
    
    try {
        // Setup application
        setupApplication(app);
        
        // Check system requirements
        if (!checkSystemRequirements()) {
            return 1;
        }
        
        // Setup logging
        setupLogging();
        
        // Show splash screen
        QSplashScreen* splash = createSplashScreen();
        splash->show();
        app.processEvents();
        
        // Simulate loading time
        splash->showMessage("Initializing components...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        app.processEvents();
        QThread::msleep(500);
        
        splash->showMessage("Loading configuration...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        app.processEvents();
        QThread::msleep(300);
        
        splash->showMessage("Starting search engine...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        app.processEvents();
        QThread::msleep(400);
        
        splash->showMessage("Ready!", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        app.processEvents();
        QThread::msleep(200);
        
        // Create and show main window
        LOG_INFO("Creating main window");
        UI::MainWindow mainWindow;
        
        // Hide splash and show main window
        splash->finish(&mainWindow);
        delete splash;
        
        mainWindow.show();
        
        LOG_INFO("FastFileSearch GUI started successfully");
        
        // Run application
        int result = app.exec();
        
        LOG_INFO("FastFileSearch GUI shutting down");
        return result;
        
    } catch (const std::exception& e) {
        LOG_FATAL_F("Fatal error in main: {}", e.what());
        showStartupError(QString("Unexpected error: %1").arg(e.what()));
        return 1;
    } catch (...) {
        LOG_FATAL("Unknown fatal error in main");
        showStartupError("Unknown error occurred during startup.");
        return 1;
    }
}

// Alternative console mode entry point
int runConsoleMode(int argc, char* argv[]) {
    std::cout << "FastFileSearch Console Mode" << std::endl;
    std::cout << "Use --gui flag to start GUI mode" << std::endl;
    
    // Parse command line arguments for console mode
    if (argc > 1) {
        std::string command = argv[1];
        
        if (command == "search" && argc > 2) {
            std::string query = argv[2];
            std::cout << "Searching for: " << query << std::endl;
            
            // TODO: Implement console search
            std::cout << "Console search not yet implemented. Use GUI mode." << std::endl;
            return 0;
        } else if (command == "index") {
            std::cout << "Starting indexing..." << std::endl;
            
            // TODO: Implement console indexing
            std::cout << "Console indexing not yet implemented. Use GUI mode." << std::endl;
            return 0;
        } else if (command == "help" || command == "--help" || command == "-h") {
            std::cout << "Usage:" << std::endl;
            std::cout << "  fastfilesearch --gui          Start GUI mode" << std::endl;
            std::cout << "  fastfilesearch search <query> Search for files" << std::endl;
            std::cout << "  fastfilesearch index          Build file index" << std::endl;
            std::cout << "  fastfilesearch help           Show this help" << std::endl;
            return 0;
        }
    }
    
    std::cout << "Invalid command. Use 'help' for usage information." << std::endl;
    return 1;
}

// Entry point that can handle both GUI and console modes
int main_entry(int argc, char* argv[]) {
    // Check for GUI flag
    bool guiMode = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--console" || arg == "-c") {
            guiMode = false;
            break;
        }
    }
    
    if (guiMode) {
        return main(argc, argv);
    } else {
        return runConsoleMode(argc, argv);
    }
}
