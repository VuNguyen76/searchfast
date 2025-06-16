#include "core/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace FastFileSearch {

// Static member definitions
std::unique_ptr<Logger> Logger::instance_;
std::once_flag Logger::initFlag_;

Logger::Logger() 
    : logLevel_(Level::INFO), logToConsole_(true), logToFile_(false), stopLogging_(false) {
    
    // Start the async logging thread
    logThread_ = std::thread(&Logger::logWorker, this);
}

Logger::~Logger() {
    // Signal the logging thread to stop
    stopLogging_.store(true);
    cv_.notify_all();
    
    // Wait for the logging thread to finish
    if (logThread_.joinable()) {
        logThread_.join();
    }
    
    // Close the log file
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

Logger& Logger::getInstance() {
    std::call_once(initFlag_, []() {
        instance_ = std::unique_ptr<Logger>(new Logger());
    });
    return *instance_;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFile_.is_open()) {
        logFile_.close();
    }
    
    logFilePath_ = filename;
    
    // Create directory if it doesn't exist
    std::filesystem::path filePath(filename);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }
    
    logFile_.open(filename, std::ios::app);
    if (logFile_.is_open()) {
        logToFile_ = true;
        
        // Write a startup message
        logFile_ << "\n" << std::string(80, '=') << "\n";
        logFile_ << "FastFileSearch Log Started: " << getCurrentTimestamp() << "\n";
        logFile_ << std::string(80, '=') << "\n";
        logFile_.flush();
    }
}

void Logger::setLogLevel(Level level) {
    logLevel_ = level;
}

void Logger::setLogToConsole(bool enable) {
    logToConsole_ = enable;
}

void Logger::setLogToFile(bool enable) {
    logToFile_ = enable;
}

void Logger::log(Level level, const std::string& message) {
    if (level < logLevel_) {
        return;
    }
    
    LogEntry entry(level, message);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logQueue_.push(entry);
    }
    
    cv_.notify_one();
}

void Logger::debug(const std::string& message) {
    log(Level::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(Level::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(Level::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(Level::ERROR, message);
}

void Logger::fatal(const std::string& message) {
    log(Level::FATAL, message);
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.flush();
    }
}

void Logger::rotateLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!logFile_.is_open() || logFilePath_.empty()) {
        return;
    }
    
    // Close current log file
    logFile_.close();
    
    // Rename current log file with timestamp
    std::string timestamp = getCurrentTimestamp();
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');
    
    std::string rotatedName = logFilePath_ + "." + timestamp;
    
    try {
        std::filesystem::rename(logFilePath_, rotatedName);
    } catch (const std::exception&) {
        // If rename fails, just continue with a new log file
    }
    
    // Open new log file
    logFile_.open(logFilePath_, std::ios::app);
    if (logFile_.is_open()) {
        logFile_ << "\n" << std::string(80, '=') << "\n";
        logFile_ << "FastFileSearch Log Rotated: " << getCurrentTimestamp() << "\n";
        logFile_ << std::string(80, '=') << "\n";
        logFile_.flush();
    }
}

size_t Logger::getLogFileSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFilePath_.empty()) {
        return 0;
    }
    
    try {
        return std::filesystem::file_size(logFilePath_);
    } catch (const std::exception&) {
        return 0;
    }
}

void Logger::logWorker() {
    while (!stopLogging_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait for log entries or stop signal
        cv_.wait(lock, [this] { return !logQueue_.empty() || stopLogging_.load(); });
        
        // Process all available log entries
        while (!logQueue_.empty()) {
            LogEntry entry = logQueue_.front();
            logQueue_.pop();
            
            // Release the lock while writing to avoid blocking other threads
            lock.unlock();
            writeLogEntry(entry);
            lock.lock();
        }
    }
    
    // Process any remaining log entries
    std::lock_guard<std::mutex> lock(mutex_);
    while (!logQueue_.empty()) {
        LogEntry entry = logQueue_.front();
        logQueue_.pop();
        writeLogEntry(entry);
    }
}

void Logger::writeLogEntry(const LogEntry& entry) {
    std::string formattedEntry = formatLogEntry(entry);
    
    // Write to console if enabled
    if (logToConsole_) {
        if (entry.level >= Level::ERROR) {
            std::cerr << formattedEntry << std::endl;
        } else {
            std::cout << formattedEntry << std::endl;
        }
    }
    
    // Write to file if enabled and file is open
    if (logToFile_ && logFile_.is_open()) {
        logFile_ << formattedEntry << std::endl;
        
        // Flush immediately for error and fatal messages
        if (entry.level >= Level::ERROR) {
            logFile_.flush();
        }
    }
}

std::string Logger::formatLogEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    
    // Timestamp
    auto timeT = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;
    
    oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    // Log level
    oss << " [" << std::setw(7) << levelToString(entry.level) << "]";
    
    // Thread ID
    oss << " [" << entry.threadId << "]";
    
    // Message
    oss << " " << entry.message;
    
    return oss.str();
}

std::string Logger::levelToString(Level level) const {
    switch (level) {
        case Level::DEBUG:   return "DEBUG";
        case Level::INFO:    return "INFO";
        case Level::WARNING: return "WARNING";
        case Level::ERROR:   return "ERROR";
        case Level::FATAL:   return "FATAL";
        default:             return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ScopedTimer implementation
Logger::ScopedTimer::ScopedTimer(const std::string& operation, Logger& logger)
    : operation_(operation), logger_(logger) {
    start_ = std::chrono::high_resolution_clock::now();
    logger_.debug("Started: " + operation_);
}

Logger::ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    
    std::ostringstream oss;
    oss << "Completed: " << operation_ << " (took " << duration.count() << "ms)";
    logger_.debug(oss.str());
}

} // namespace FastFileSearch
