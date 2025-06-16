#pragma once

#include "core/types.h"
#include "utils/thread_safe_queue.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace FastFileSearch {
namespace Engine {

// Abstract base class for platform-specific file watchers
class DriveWatcher {
protected:
    std::string drivePath_;
    std::atomic<bool> isWatching_;
    std::atomic<bool> shouldStop_;
    Utils::ThreadSafeQueue<FileChangeEvent>* eventQueue_;
    
public:
    explicit DriveWatcher(const std::string& drivePath, 
                         Utils::ThreadSafeQueue<FileChangeEvent>* eventQueue);
    virtual ~DriveWatcher() = default;
    
    // Non-copyable
    DriveWatcher(const DriveWatcher&) = delete;
    DriveWatcher& operator=(const DriveWatcher&) = delete;
    
    // Pure virtual methods to be implemented by platform-specific classes
    virtual bool startWatching() = 0;
    virtual void stopWatching() = 0;
    virtual bool isSupported() const = 0;
    
    // Common interface
    bool isWatching() const { return isWatching_.load(); }
    const std::string& getDrivePath() const { return drivePath_; }

protected:
    void postEvent(const FileChangeEvent& event);
    bool shouldContinueWatching() const;
};

// Platform-specific implementations
#ifdef _WIN32
class WindowsFileWatcher : public DriveWatcher {
private:
    void* directoryHandle_;
    void* overlapped_;
    std::thread watchThread_;
    std::vector<char> buffer_;
    static const size_t BUFFER_SIZE = 64 * 1024; // 64KB
    
public:
    explicit WindowsFileWatcher(const std::string& drivePath,
                               Utils::ThreadSafeQueue<FileChangeEvent>* eventQueue);
    ~WindowsFileWatcher() override;
    
    bool startWatching() override;
    void stopWatching() override;
    bool isSupported() const override { return true; }

private:
    void watchLoop();
    void processNotifications(const char* buffer, DWORD bytesReturned);
    FileChangeType convertWindowsAction(DWORD action);
    std::string extractFilePath(const char* buffer, size_t offset, size_t nameLength);
};

#elif defined(__linux__)
class LinuxFileWatcher : public DriveWatcher {
private:
    int inotifyFd_;
    int watchDescriptor_;
    std::thread watchThread_;
    std::unordered_map<int, std::string> watchDescriptorToPath_;
    
public:
    explicit LinuxFileWatcher(const std::string& drivePath,
                             Utils::ThreadSafeQueue<FileChangeEvent>* eventQueue);
    ~LinuxFileWatcher() override;
    
    bool startWatching() override;
    void stopWatching() override;
    bool isSupported() const override { return true; }

private:
    void watchLoop();
    bool addWatchRecursive(const std::string& path);
    void removeWatch(int wd);
    FileChangeType convertLinuxMask(uint32_t mask);
};

#elif defined(__APPLE__)
class MacOSFileWatcher : public DriveWatcher {
private:
    void* fsEventStream_;
    void* runLoop_;
    std::thread watchThread_;
    
public:
    explicit MacOSFileWatcher(const std::string& drivePath,
                             Utils::ThreadSafeQueue<FileChangeEvent>* eventQueue);
    ~MacOSFileWatcher() override;
    
    bool startWatching() override;
    void stopWatching() override;
    bool isSupported() const override { return true; }

private:
    void watchLoop();
    static void fsEventCallback(void* streamRef, void* clientCallBackInfo,
                               size_t numEvents, void* eventPaths,
                               const void* eventFlags, const void* eventIds);
    FileChangeType convertMacOSFlags(uint32_t flags);
};
#endif

// Main file watcher class that manages multiple drive watchers
class FileWatcher {
private:
    std::vector<std::unique_ptr<DriveWatcher>> driveWatchers_;
    Utils::ThreadSafeQueue<FileChangeEvent> eventQueue_;
    std::thread eventProcessorThread_;
    
    std::atomic<bool> isWatching_;
    std::atomic<bool> shouldStop_;
    
    // Event filtering
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> recentEvents_;
    std::mutex recentEventsMutex_;
    std::chrono::milliseconds eventCoalescingDelay_;
    
    // Callbacks
    std::function<void(const FileChangeEvent&)> eventCallback_;
    std::function<void(const std::string&)> errorCallback_;
    
    // Statistics
    std::atomic<uint64_t> eventsProcessed_;
    std::atomic<uint64_t> eventsFiltered_;
    std::atomic<uint64_t> errorsEncountered_;
    
    // Configuration
    bool enableEventCoalescing_;
    bool enableRecursiveWatching_;
    std::vector<std::string> excludedPaths_;
    std::vector<std::string> excludedExtensions_;

public:
    FileWatcher();
    ~FileWatcher();
    
    // Non-copyable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    
    // Main interface
    bool startWatching(const std::vector<std::string>& drives);
    void stopWatching();
    bool isWatching() const { return isWatching_.load(); }
    
    // Event processing
    void processEvents();
    bool getNextEvent(FileChangeEvent& event, std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
    
    // Configuration
    void setEventCallback(std::function<void(const FileChangeEvent&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setEventCoalescingDelay(std::chrono::milliseconds delay);
    void setEventCoalescingEnabled(bool enabled);
    void setRecursiveWatchingEnabled(bool enabled);
    void setExcludedPaths(const std::vector<std::string>& paths);
    void setExcludedExtensions(const std::vector<std::string>& extensions);
    
    // Statistics
    uint64_t getEventsProcessed() const { return eventsProcessed_.load(); }
    uint64_t getEventsFiltered() const { return eventsFiltered_.load(); }
    uint64_t getErrorsEncountered() const { return errorsEncountered_.load(); }
    void resetStatistics();
    
    // Utility methods
    std::vector<std::string> getSupportedDrives();
    bool isDriveSupported(const std::string& drivePath);
    
    // Manual event injection (for testing)
    void injectEvent(const FileChangeEvent& event);

private:
    // Event processing
    void eventProcessorLoop();
    bool shouldFilterEvent(const FileChangeEvent& event);
    bool isPathExcluded(const std::string& path);
    bool isExtensionExcluded(const std::string& path);
    
    // Event coalescing
    bool isRecentEvent(const std::string& path);
    void markRecentEvent(const std::string& path);
    void cleanupRecentEvents();
    
    // Drive watcher management
    std::unique_ptr<DriveWatcher> createDriveWatcher(const std::string& drivePath);
    void removeDriveWatcher(const std::string& drivePath);
    
    // Error handling
    void handleWatcherError(const std::string& drivePath, const std::string& error);
    
    // Platform detection
    static bool isPlatformSupported();
    static std::string getPlatformName();
};

// Event filter class for advanced filtering
class FileEventFilter {
private:
    std::vector<std::string> includePaths_;
    std::vector<std::string> excludePaths_;
    std::vector<std::string> includeExtensions_;
    std::vector<std::string> excludeExtensions_;
    std::vector<FileChangeType> includeEventTypes_;
    std::vector<FileChangeType> excludeEventTypes_;
    
    uint64_t minFileSize_;
    uint64_t maxFileSize_;
    
    bool enableSizeFilter_;
    bool enablePathFilter_;
    bool enableExtensionFilter_;
    bool enableEventTypeFilter_;

public:
    FileEventFilter();
    
    // Configuration
    void setIncludePaths(const std::vector<std::string>& paths);
    void setExcludePaths(const std::vector<std::string>& paths);
    void setIncludeExtensions(const std::vector<std::string>& extensions);
    void setExcludeExtensions(const std::vector<std::string>& extensions);
    void setIncludeEventTypes(const std::vector<FileChangeType>& types);
    void setExcludeEventTypes(const std::vector<FileChangeType>& types);
    void setSizeRange(uint64_t minSize, uint64_t maxSize);
    
    // Filter control
    void enableSizeFilter(bool enable) { enableSizeFilter_ = enable; }
    void enablePathFilter(bool enable) { enablePathFilter_ = enable; }
    void enableExtensionFilter(bool enable) { enableExtensionFilter_ = enable; }
    void enableEventTypeFilter(bool enable) { enableEventTypeFilter_ = enable; }
    
    // Filtering
    bool shouldAcceptEvent(const FileChangeEvent& event) const;
    
    // Utility
    void clear();
    bool isEmpty() const;

private:
    bool matchesPath(const std::string& path) const;
    bool matchesExtension(const std::string& path) const;
    bool matchesEventType(FileChangeType type) const;
    bool matchesSize(const std::string& path) const;
    
    std::string getFileExtension(const std::string& path) const;
    uint64_t getFileSize(const std::string& path) const;
    bool pathMatches(const std::string& path, const std::vector<std::string>& patterns) const;
};

} // namespace Engine
} // namespace FastFileSearch
