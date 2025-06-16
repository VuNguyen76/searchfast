#pragma once

#include "core/types.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

namespace FastFileSearch {
namespace Storage {

class SQLiteDatabase {
private:
    sqlite3* db_;
    std::string dbPath_;
    mutable std::mutex mutex_;
    bool isOpen_;
    
    // Prepared statements for performance
    sqlite3_stmt* insertFileStmt_;
    sqlite3_stmt* updateFileStmt_;
    sqlite3_stmt* deleteFileStmt_;
    sqlite3_stmt* selectFileStmt_;
    sqlite3_stmt* selectFilesByPathStmt_;
    sqlite3_stmt* insertDriveStmt_;
    sqlite3_stmt* updateDriveStmt_;
    sqlite3_stmt* selectDriveStmt_;
    sqlite3_stmt* insertSearchIndexStmt_;
    sqlite3_stmt* deleteSearchIndexStmt_;
    sqlite3_stmt* searchFilesStmt_;

public:
    SQLiteDatabase();
    explicit SQLiteDatabase(const std::string& dbPath);
    ~SQLiteDatabase();

    // Non-copyable
    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    // Movable
    SQLiteDatabase(SQLiteDatabase&& other) noexcept;
    SQLiteDatabase& operator=(SQLiteDatabase&& other) noexcept;

    // Database operations
    bool open(const std::string& dbPath);
    void close();
    bool isOpen() const { return isOpen_; }
    
    // Schema management
    bool createTables();
    bool createIndexes();
    bool upgradeSchema(int fromVersion, int toVersion);
    int getSchemaVersion();
    bool setSchemaVersion(int version);
    
    // Transaction management
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    
    class Transaction {
    private:
        SQLiteDatabase& db_;
        bool committed_;
        
    public:
        explicit Transaction(SQLiteDatabase& db);
        ~Transaction();
        
        bool commit();
        void rollback();
    };
    
    // File operations
    bool insertFile(const FileEntry& entry);
    bool updateFile(const FileEntry& entry);
    bool deleteFile(uint64_t fileId);
    bool deleteFileByPath(const std::string& path);
    
    std::unique_ptr<FileEntry> getFile(uint64_t fileId);
    std::unique_ptr<FileEntry> getFileByPath(const std::string& path);
    std::vector<FileEntry> getFilesByParent(uint64_t parentId);
    std::vector<FileEntry> getFilesByDrive(uint64_t driveId);
    
    // Drive operations
    bool insertDrive(const DriveInfo& drive);
    bool updateDrive(const DriveInfo& drive);
    bool deleteDrive(uint64_t driveId);
    std::unique_ptr<DriveInfo> getDrive(uint64_t driveId);
    std::unique_ptr<DriveInfo> getDriveByLetter(const std::string& letter);
    std::vector<DriveInfo> getAllDrives();
    
    // Search index operations
    bool insertSearchIndex(uint64_t fileId, const std::string& token, int position);
    bool deleteSearchIndex(uint64_t fileId);
    bool rebuildSearchIndex();
    
    // Search operations
    std::vector<FileEntry> searchFiles(const SearchQuery& query);
    std::vector<FileEntry> searchFilesByName(const std::string& name, bool exactMatch = false);
    std::vector<FileEntry> searchFilesByExtension(const std::string& extension);
    std::vector<FileEntry> searchFilesBySize(const SizeRange& sizeRange);
    std::vector<FileEntry> searchFilesByDate(const DateRange& dateRange);
    
    // Statistics
    uint64_t getFileCount();
    uint64_t getDirectoryCount();
    uint64_t getTotalSize();
    IndexStatistics getIndexStatistics();
    
    // Maintenance operations
    bool vacuum();
    bool analyze();
    bool checkIntegrity();
    bool optimizeDatabase();
    
    // Batch operations
    bool insertFilesBatch(const std::vector<FileEntry>& entries);
    bool updateFilesBatch(const std::vector<FileEntry>& entries);
    bool deleteFilesBatch(const std::vector<uint64_t>& fileIds);
    
    // Utility methods
    std::string getLastError() const;
    int getLastErrorCode() const;
    void enableWAL(bool enable = true);
    void setCacheSize(int pages);
    void setPragma(const std::string& pragma, const std::string& value);
    
    // Callback for progress monitoring
    using ProgressCallback = std::function<void(int percentage)>;
    void setProgressCallback(ProgressCallback callback);

private:
    // Helper methods
    bool prepareStatements();
    void finalizeStatements();
    bool executeSQL(const std::string& sql);
    bool bindFileEntry(sqlite3_stmt* stmt, const FileEntry& entry);
    bool bindDriveInfo(sqlite3_stmt* stmt, const DriveInfo& drive);
    FileEntry extractFileEntry(sqlite3_stmt* stmt);
    DriveInfo extractDriveInfo(sqlite3_stmt* stmt);
    
    // Schema creation SQL
    static const char* CREATE_FILES_TABLE;
    static const char* CREATE_DRIVES_TABLE;
    static const char* CREATE_SEARCH_INDEX_TABLE;
    static const char* CREATE_METADATA_TABLE;
    
    // Index creation SQL
    static const char* CREATE_FILES_INDEXES;
    static const char* CREATE_SEARCH_INDEXES;
    
    // Prepared statement SQL
    static const char* INSERT_FILE_SQL;
    static const char* UPDATE_FILE_SQL;
    static const char* DELETE_FILE_SQL;
    static const char* SELECT_FILE_SQL;
    static const char* SELECT_FILES_BY_PATH_SQL;
    static const char* INSERT_DRIVE_SQL;
    static const char* UPDATE_DRIVE_SQL;
    static const char* SELECT_DRIVE_SQL;
    static const char* INSERT_SEARCH_INDEX_SQL;
    static const char* DELETE_SEARCH_INDEX_SQL;
    static const char* SEARCH_FILES_SQL;
    
    ProgressCallback progressCallback_;
    
    // Error handling
    void logSQLiteError(const std::string& operation) const;
    bool checkResult(int result, const std::string& operation) const;
};

// RAII helper for SQLite statements
class SQLiteStatement {
private:
    sqlite3_stmt* stmt_;
    
public:
    explicit SQLiteStatement(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~SQLiteStatement() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }
    
    // Non-copyable
    SQLiteStatement(const SQLiteStatement&) = delete;
    SQLiteStatement& operator=(const SQLiteStatement&) = delete;
    
    // Movable
    SQLiteStatement(SQLiteStatement&& other) noexcept : stmt_(other.stmt_) {
        other.stmt_ = nullptr;
    }
    
    SQLiteStatement& operator=(SQLiteStatement&& other) noexcept {
        if (this != &other) {
            if (stmt_) {
                sqlite3_finalize(stmt_);
            }
            stmt_ = other.stmt_;
            other.stmt_ = nullptr;
        }
        return *this;
    }
    
    sqlite3_stmt* get() const { return stmt_; }
    sqlite3_stmt* release() {
        auto* result = stmt_;
        stmt_ = nullptr;
        return result;
    }
    
    operator bool() const { return stmt_ != nullptr; }
};

} // namespace Storage
} // namespace FastFileSearch
