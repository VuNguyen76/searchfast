#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <chrono>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

namespace FastFileSearch {

class Logger {
public:
    enum class Level : uint8_t {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        FATAL = 4
    };

private:
    struct LogEntry {
        Level level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id threadId;
        
        LogEntry(Level l, const std::string& msg)
            : level(l), message(msg), timestamp(std::chrono::system_clock::now()),
              threadId(std::this_thread::get_id()) {}
    };

    mutable std::mutex mutex_;
    std::ofstream logFile_;
    std::string logFilePath_;
    Level logLevel_;
    bool logToConsole_;
    bool logToFile_;
    
    // Async logging
    std::queue<LogEntry> logQueue_;
    std::condition_variable cv_;
    std::thread logThread_;
    std::atomic<bool> stopLogging_;
    
    // Singleton pattern
    static std::unique_ptr<Logger> instance_;
    static std::once_flag initFlag_;
    
    Logger();

public:
    ~Logger();
    
    void logWorker();
    void writeLogEntry(const LogEntry& entry);
    std::string formatLogEntry(const LogEntry& entry) const;
    std::string levelToString(Level level) const;
    std::string getCurrentTimestamp() const;

public:
    // Singleton access
    static Logger& getInstance();
    
    // Configuration
    void setLogFile(const std::string& filename);
    void setLogLevel(Level level);
    void setLogToConsole(bool enable);
    void setLogToFile(bool enable);
    
    // Logging methods
    void log(Level level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    // Template methods for formatted logging
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        if (logLevel_ <= Level::DEBUG) {
            log(Level::DEBUG, formatString(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        if (logLevel_ <= Level::INFO) {
            log(Level::INFO, formatString(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void warning(const std::string& format, Args&&... args) {
        if (logLevel_ <= Level::WARNING) {
            log(Level::WARNING, formatString(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        if (logLevel_ <= Level::ERROR) {
            log(Level::ERROR, formatString(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void fatal(const std::string& format, Args&&... args) {
        log(Level::FATAL, formatString(format, std::forward<Args>(args)...));
    }
    
    // Utility methods
    void flush();
    void rotateLog();
    size_t getLogFileSize() const;
    
    // Performance logging
    class ScopedTimer {
    private:
        std::string operation_;
        std::chrono::high_resolution_clock::time_point start_;
        Logger& logger_;
        
    public:
        ScopedTimer(const std::string& operation, Logger& logger = Logger::getInstance());
        ~ScopedTimer();
    };

private:
    template<typename... Args>
    std::string formatString(const std::string& format, Args&&... args) {
        std::ostringstream oss;
        formatStringImpl(oss, format, std::forward<Args>(args)...);
        return oss.str();
    }
    
    template<typename T>
    void formatStringImpl(std::ostringstream& oss, const std::string& format, T&& value) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << std::forward<T>(value) << format.substr(pos + 2);
        } else {
            oss << format;
        }
    }
    
    template<typename T, typename... Args>
    void formatStringImpl(std::ostringstream& oss, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << std::forward<T>(value);
            formatStringImpl(oss, format.substr(pos + 2), std::forward<Args>(args)...);
        } else {
            oss << format;
        }
    }
};

// Convenience macros
#define LOG_DEBUG(msg) FastFileSearch::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) FastFileSearch::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) FastFileSearch::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) FastFileSearch::Logger::getInstance().error(msg)
#define LOG_FATAL(msg) FastFileSearch::Logger::getInstance().fatal(msg)

#define LOG_DEBUG_F(format, ...) FastFileSearch::Logger::getInstance().debug(format, __VA_ARGS__)
#define LOG_INFO_F(format, ...) FastFileSearch::Logger::getInstance().info(format, __VA_ARGS__)
#define LOG_WARNING_F(format, ...) FastFileSearch::Logger::getInstance().warning(format, __VA_ARGS__)
#define LOG_ERROR_F(format, ...) FastFileSearch::Logger::getInstance().error(format, __VA_ARGS__)
#define LOG_FATAL_F(format, ...) FastFileSearch::Logger::getInstance().fatal(format, __VA_ARGS__)

#define SCOPED_TIMER(operation) FastFileSearch::Logger::ScopedTimer timer(operation)

// Exception logging helper
class LoggedException : public std::exception {
private:
    std::string message_;
    
public:
    explicit LoggedException(const std::string& message) : message_(message) {
        LOG_ERROR(message_);
    }
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
};

} // namespace FastFileSearch
