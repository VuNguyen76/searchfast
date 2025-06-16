#pragma once

#include "core/types.h"
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <vector>

namespace FastFileSearch {
namespace App {

class ConfigManager {
public:
    using ConfigChangedCallback = std::function<void(const std::string& key, const std::string& oldValue, const std::string& newValue)>;

private:
    AppSettings settings_;
    std::string configFilePath_;
    mutable std::mutex mutex_;
    
    // Configuration change tracking
    std::unordered_map<std::string, ConfigChangedCallback> changeCallbacks_;
    bool autoSave_;
    bool isDirty_;
    
    // Default configuration
    static const AppSettings DEFAULT_SETTINGS;
    
    // Configuration validation
    struct ValidationRule {
        std::string key;
        std::function<bool(const std::string&)> validator;
        std::string errorMessage;
    };
    
    std::vector<ValidationRule> validationRules_;

public:
    ConfigManager();
    explicit ConfigManager(const std::string& configFilePath);
    ~ConfigManager();
    
    // Non-copyable
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Configuration file operations
    bool load();
    bool load(const std::string& filePath);
    bool save();
    bool save(const std::string& filePath);
    bool saveAs(const std::string& filePath);
    
    // Settings access
    const AppSettings& getSettings() const;
    void setSettings(const AppSettings& settings);
    void resetToDefaults();
    
    // Individual setting access
    template<typename T>
    T getValue(const std::string& key) const;
    
    template<typename T>
    void setValue(const std::string& key, const T& value);
    
    bool hasKey(const std::string& key) const;
    void removeKey(const std::string& key);
    
    // Indexing settings
    std::vector<std::string> getIncludeDrives() const;
    void setIncludeDrives(const std::vector<std::string>& drives);
    void addIncludeDrive(const std::string& drive);
    void removeIncludeDrive(const std::string& drive);
    
    std::vector<std::string> getExcludePaths() const;
    void setExcludePaths(const std::vector<std::string>& paths);
    void addExcludePath(const std::string& path);
    void removeExcludePath(const std::string& path);
    
    std::vector<std::string> getExcludeExtensions() const;
    void setExcludeExtensions(const std::vector<std::string>& extensions);
    void addExcludeExtension(const std::string& extension);
    void removeExcludeExtension(const std::string& extension);
    
    bool getIndexHiddenFiles() const;
    void setIndexHiddenFiles(bool index);
    
    bool getIndexSystemFiles() const;
    void setIndexSystemFiles(bool index);
    
    // Search settings
    SearchMode getDefaultSearchMode() const;
    void setDefaultSearchMode(SearchMode mode);
    
    uint32_t getMaxSearchResults() const;
    void setMaxSearchResults(uint32_t maxResults);
    
    bool getEnableFuzzySearch() const;
    void setEnableFuzzySearch(bool enable);
    
    double getFuzzyThreshold() const;
    void setFuzzyThreshold(double threshold);
    
    // UI settings
    bool getStartMinimized() const;
    void setStartMinimized(bool minimized);
    
    bool getShowInSystemTray() const;
    void setShowInSystemTray(bool show);
    
    std::string getHotkey() const;
    void setHotkey(const std::string& hotkey);
    
    Theme getUITheme() const;
    void setUITheme(Theme theme);
    
    // Performance settings
    uint32_t getIndexingThreads() const;
    void setIndexingThreads(uint32_t threads);
    
    uint32_t getMaxMemoryUsage() const;
    void setMaxMemoryUsage(uint32_t memoryMB);
    
    bool getEnableCache() const;
    void setEnableCache(bool enable);
    
    uint32_t getCacheSize() const;
    void setCacheSize(uint32_t sizeMB);
    
    // Database settings
    std::string getDatabasePath() const;
    void setDatabasePath(const std::string& path);
    
    bool getEnableWAL() const;
    void setEnableWAL(bool enable);
    
    uint32_t getCachePages() const;
    void setCachePages(uint32_t pages);
    
    // Configuration management
    void setAutoSave(bool autoSave);
    bool isAutoSave() const { return autoSave_; }
    bool isDirty() const { return isDirty_; }
    void markClean() { isDirty_ = false; }
    
    // Change notifications
    void registerChangeCallback(const std::string& key, ConfigChangedCallback callback);
    void unregisterChangeCallback(const std::string& key);
    void clearChangeCallbacks();
    
    // Validation
    bool validate() const;
    std::vector<std::string> getValidationErrors() const;
    void addValidationRule(const std::string& key, 
                          std::function<bool(const std::string&)> validator,
                          const std::string& errorMessage);
    
    // Import/Export
    bool importFromJSON(const std::string& jsonString);
    std::string exportToJSON() const;
    bool importFromINI(const std::string& iniString);
    std::string exportToINI() const;
    
    // Configuration profiles
    bool saveProfile(const std::string& profileName);
    bool loadProfile(const std::string& profileName);
    std::vector<std::string> getAvailableProfiles() const;
    bool deleteProfile(const std::string& profileName);
    
    // Backup and restore
    bool createBackup(const std::string& backupPath = "");
    bool restoreFromBackup(const std::string& backupPath);
    std::vector<std::string> getAvailableBackups() const;
    
    // Migration
    bool migrateFromOldVersion(const std::string& oldConfigPath);
    int getConfigVersion() const;
    bool upgradeConfig(int fromVersion, int toVersion);

private:
    // File format handlers
    bool loadFromJSON(const std::string& filePath);
    bool saveToJSON(const std::string& filePath);
    bool loadFromINI(const std::string& filePath);
    bool saveToINI(const std::string& filePath);
    
    // JSON parsing helpers
    void parseJSONSettings(const std::string& jsonString);
    std::string settingsToJSON() const;
    
    // INI parsing helpers
    void parseINISettings(const std::string& iniString);
    std::string settingsToINI() const;
    
    // Validation helpers
    void setupDefaultValidationRules();
    bool validateDrivePath(const std::string& drive) const;
    bool validateFilePath(const std::string& path) const;
    bool validateExtension(const std::string& extension) const;
    bool validateMemorySize(const std::string& size) const;
    bool validateThreadCount(const std::string& count) const;
    
    // Change notification
    void notifyChange(const std::string& key, const std::string& oldValue, const std::string& newValue);
    
    // Utility methods
    std::string getConfigDirectory() const;
    std::string getProfilePath(const std::string& profileName) const;
    std::string getBackupPath(const std::string& backupName = "") const;
    std::string generateBackupName() const;
    
    // File operations
    bool fileExists(const std::string& path) const;
    bool createDirectory(const std::string& path) const;
    std::string readFile(const std::string& path) const;
    bool writeFile(const std::string& path, const std::string& content) const;
    
    // String conversion helpers
    template<typename T>
    std::string toString(const T& value) const;
    
    template<typename T>
    T fromString(const std::string& str) const;
    
    // Specialized conversions
    std::string searchModeToString(SearchMode mode) const;
    SearchMode stringToSearchMode(const std::string& str) const;
    std::string themeToString(Theme theme) const;
    Theme stringToTheme(const std::string& str) const;
    
    // Vector conversions
    std::string vectorToString(const std::vector<std::string>& vec) const;
    std::vector<std::string> stringToVector(const std::string& str) const;
    
    // Error handling
    void handleConfigError(const std::string& operation, const std::exception& e);
    
    // Constants
    static const std::string CONFIG_FILE_EXTENSION;
    static const std::string PROFILE_DIRECTORY;
    static const std::string BACKUP_DIRECTORY;
    static const int CURRENT_CONFIG_VERSION;
};

// Configuration validator class
class ConfigValidator {
public:
    static bool validateSettings(const AppSettings& settings);
    static std::vector<std::string> getValidationErrors(const AppSettings& settings);
    
    // Individual validators
    static bool validateDrives(const std::vector<std::string>& drives);
    static bool validatePaths(const std::vector<std::string>& paths);
    static bool validateExtensions(const std::vector<std::string>& extensions);
    static bool validateSearchMode(SearchMode mode);
    static bool validateTheme(Theme theme);
    static bool validateMemoryUsage(uint32_t memoryMB);
    static bool validateThreadCount(uint32_t threads);
    static bool validateCacheSize(uint32_t sizeMB);
    static bool validateFuzzyThreshold(double threshold);
    static bool validateHotkey(const std::string& hotkey);
    static bool validateDatabasePath(const std::string& path);

private:
    static bool isValidDriveLetter(const std::string& drive);
    static bool isValidPath(const std::string& path);
    static bool isValidExtension(const std::string& extension);
    static bool isValidHotkeyFormat(const std::string& hotkey);
};

} // namespace App
} // namespace FastFileSearch
