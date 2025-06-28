#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <algorithm>
#include <mutex>
#include <chrono>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Control IDs
#define ID_SEARCH_EDIT      1001
#define ID_RESULTS_LIST     1002
#define ID_STATUS_BAR       1003

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hSearchEdit = NULL;
HWND g_hResultsList = NULL;
HWND g_hStatusBar = NULL;

// FastFileSearch Engine - Parallel Optimized
class FastFileSearchEngine {
private:
    struct FileInfo {
        std::string path;
        std::string name;
        std::string lowerName;
        ULONGLONG size;
    };
    
    std::vector<FileInfo> m_fileIndex;
    std::mutex m_indexMutex;
    std::atomic<size_t> m_indexedFiles;
    std::atomic<bool> m_isIndexing;
    std::atomic<bool> m_shouldStop;
    std::thread m_backgroundIndexer;
    
    // Performance tracking
    std::chrono::steady_clock::time_point m_indexStartTime;
    
public:
    FastFileSearchEngine() : m_indexedFiles(0), m_isIndexing(false), m_shouldStop(false) {}
    
    ~FastFileSearchEngine() {
        Shutdown();
    }
    
    bool Initialize() {
        SetStatus("FastFileSearch ready - Parallel indexing starting...");
        StartBackgroundIndexing();
        return true;
    }
    
    void Shutdown() {
        m_shouldStop.store(true);
        if (m_backgroundIndexer.joinable()) {
            m_backgroundIndexer.join();
        }
    }
    
    // Instant Search - Core Feature
    std::vector<std::string> SearchInstant(const std::string& query) {
        std::vector<std::string> results;
        
        if (query.empty() || query.length() < 1) {
            return results;
        }
        
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        
        {
            std::lock_guard<std::mutex> lock(m_indexMutex);
            
            for (const auto& file : m_fileIndex) {
                if (file.lowerName.find(lowerQuery) != std::string::npos) {
                    results.push_back(file.path);
                    if (results.size() >= 100) break;
                }
            }
        }
        
        return results;
    }
    
    size_t GetIndexedFileCount() const {
        return m_indexedFiles.load();
    }
    
    bool IsIndexing() const {
        return m_isIndexing.load();
    }
    
private:
    void StartBackgroundIndexing() {
        m_backgroundIndexer = std::thread([this]() {
            BackgroundIndexingWorker();
        });
    }
    
    void BackgroundIndexingWorker() {
        m_isIndexing.store(true);
        m_indexStartTime = std::chrono::steady_clock::now();
        
        SetStatus("Starting parallel file indexing...");
        
        // Phase 1: Index user directories first (for instant results)
        IndexUserDirectoriesParallel();
        
        // Phase 2: Index all drives in parallel
        IndexAllDrivesParallel();
        
        m_isIndexing.store(false);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - m_indexStartTime);
        
        SetStatus("Parallel indexing complete - " + std::to_string(m_indexedFiles.load()) + 
                 " files indexed in " + std::to_string(duration.count()) + " seconds");
    }
    
    void IndexUserDirectoriesParallel() {
        std::vector<std::string> userPaths = {
            GetSpecialFolderPath(CSIDL_MYDOCUMENTS),
            GetSpecialFolderPath(CSIDL_DESKTOP),
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Downloads",
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Pictures",
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Videos",
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Music"
        };
        
        std::vector<std::thread> userWorkers;
        
        SetStatus("Parallel indexing user directories for instant results...");
        
        for (const auto& path : userPaths) {
            if (!path.empty() && !m_shouldStop.load()) {
                userWorkers.emplace_back([this, path]() {
                    IndexDirectoryOptimized(path);
                });
            }
        }
        
        // Wait for user directories to complete
        for (auto& worker : userWorkers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        SetStatus("User directories indexed - " + std::to_string(m_indexedFiles.load()) + " files ready for search");
    }
    
    void IndexAllDrivesParallel() {
        DWORD drives = GetLogicalDrives();
        std::vector<std::thread> workers;
        std::atomic<int> completedDrives(0);
        std::atomic<int> totalDrives(0);
        
        // Count total drives first
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i)) {
                char drive = static_cast<char>('A' + i);
                if (drive != 'A' && drive != 'B') { // Skip floppy drives
                    totalDrives.fetch_add(1);
                }
            }
        }
        
        SetStatus("Starting parallel indexing of " + std::to_string(totalDrives.load()) + " drives...");
        
        // Create worker threads for each drive
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i) && !m_shouldStop.load()) {
                char drive[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
                
                if (drive[0] != 'A' && drive[0] != 'B') {
                    workers.emplace_back([this, drive = std::string(drive), &completedDrives, &totalDrives]() {
                        try {
                            size_t filesBeforeDrive = m_indexedFiles.load();
                            
                            SetStatus("Parallel indexing drive " + drive + "...");
                            IndexDirectoryOptimized(drive);
                            
                            size_t filesAfterDrive = m_indexedFiles.load();
                            size_t filesFromDrive = filesAfterDrive - filesBeforeDrive;
                            
                            int completed = completedDrives.fetch_add(1) + 1;
                            SetStatus("Drive " + drive + " completed: " + std::to_string(filesFromDrive) + 
                                     " files (" + std::to_string(completed) + "/" + 
                                     std::to_string(totalDrives.load()) + " drives done)");
                        } catch (...) {
                            SetStatus("Error indexing drive " + drive);
                        }
                    });
                }
            }
        }
        
        // Wait for all drives to complete
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    void IndexDirectoryOptimized(const std::string& path) {
        try {
            WIN32_FIND_DATAA findData;
            std::string searchPath = path + "\\*";
            
            // Use optimized FindFirstFileEx for better performance
            HANDLE hFind = FindFirstFileExA(
                searchPath.c_str(),
                FindExInfoBasic,        // Less info = faster
                &findData,
                FindExSearchNameMatch,
                NULL,
                FIND_FIRST_EX_LARGE_FETCH  // Fetch multiple entries at once
            );
            
            if (hFind == INVALID_HANDLE_VALUE) return;
            
            do {
                if (m_shouldStop.load()) break;
                
                std::string fileName = findData.cFileName;
                if (fileName == "." || fileName == "..") continue;
                
                std::string fullPath = path + "\\" + fileName;
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Skip system directories for speed
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) &&
                        !(findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                        fileName != "System Volume Information" &&
                        fileName != "$Recycle.Bin" &&
                        fileName != "Windows" &&
                        fileName != "Program Files" &&
                        fileName != "Program Files (x86)" &&
                        fileName != "ProgramData") {
                        
                        IndexDirectoryOptimized(fullPath);
                    }
                } else {
                    // Add file to index
                    FileInfo fileInfo;
                    fileInfo.path = fullPath;
                    fileInfo.name = fileName;
                    fileInfo.lowerName = fileName;
                    std::transform(fileInfo.lowerName.begin(), fileInfo.lowerName.end(), 
                                 fileInfo.lowerName.begin(), ::tolower);
                    
                    fileInfo.size = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                    
                    {
                        std::lock_guard<std::mutex> lock(m_indexMutex);
                        m_fileIndex.push_back(fileInfo);
                    }
                    
                    size_t currentCount = m_indexedFiles.fetch_add(1) + 1;
                    
                    // Update status every 2000 files for better performance
                    if (currentCount % 2000 == 0) {
                        SetStatus("Parallel indexing... " + std::to_string(currentCount) + " files found");
                    }
                }
                
            } while (FindNextFileA(hFind, &findData) && !m_shouldStop.load());
            
            FindClose(hFind);
            
        } catch (...) {
            // Skip inaccessible directories
        }
    }
    
    std::string GetSpecialFolderPath(int csidl) {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path))) {
            return std::string(path);
        }
        return "";
    }
    
    void SetStatus(const std::string& status) {
        if (g_hStatusBar) {
            SetWindowTextA(g_hStatusBar, status.c_str());
        }
    }
};

// Global search engine
std::unique_ptr<FastFileSearchEngine> g_searchEngine;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PerformInstantSearch();
void DisplayResults(const std::vector<std::string>& results);
std::string FormatFileSize(ULONGLONG size);

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateControls(hwnd);
            g_searchEngine = std::make_unique<FastFileSearchEngine>();
            g_searchEngine->Initialize();
            break;

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            MoveWindow(g_hSearchEdit, 20, 20, width - 40, 35, TRUE);
            MoveWindow(g_hResultsList, 20, 70, width - 40, height - 130, TRUE);
            MoveWindow(g_hStatusBar, 0, height - 25, width, 25, TRUE);
            break;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hSearchEdit) {
                PerformInstantSearch();
            }
            break;

        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->hwndFrom == g_hResultsList && pnmh->code == NM_DBLCLK) {
                int selectedIndex = ListView_GetNextItem(g_hResultsList, -1, LVNI_SELECTED);
                if (selectedIndex >= 0) {
                    char filePath[MAX_PATH];
                    ListView_GetItemText(g_hResultsList, selectedIndex, 0, filePath, MAX_PATH);
                    ShellExecuteA(hwnd, "open", filePath, NULL, NULL, SW_SHOWNORMAL);
                }
            }
            break;
        }

        case WM_DESTROY:
            if (g_searchEngine) {
                g_searchEngine->Shutdown();
                g_searchEngine.reset();
            }
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd) {
    // Search box
    g_hSearchEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 20, 400, 35,
        hwnd,
        (HMENU)ID_SEARCH_EDIT,
        GetModuleHandle(NULL),
        NULL
    );

    // Set font
    HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Results list
    g_hResultsList = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWA,
        "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        20, 70, 500, 400,
        hwnd,
        (HMENU)ID_RESULTS_LIST,
        GetModuleHandle(NULL),
        NULL
    );

    // Add columns
    LVCOLUMNA column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = 500;
    column.pszText = const_cast<char*>("File Path");
    ListView_InsertColumn(g_hResultsList, 0, &column);

    column.cx = 100;
    column.pszText = const_cast<char*>("Size");
    ListView_InsertColumn(g_hResultsList, 1, &column);

    // Status bar
    g_hStatusBar = CreateWindowA(
        "STATIC",
        "Initializing parallel indexing...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0,
        hwnd,
        (HMENU)ID_STATUS_BAR,
        GetModuleHandle(NULL),
        NULL
    );

    SetFocus(g_hSearchEdit);
}

void PerformInstantSearch() {
    if (!g_searchEngine) return;

    char searchText[256];
    GetWindowTextA(g_hSearchEdit, searchText, sizeof(searchText));

    std::string query(searchText);

    if (query.length() < 1) {
        ListView_DeleteAllItems(g_hResultsList);
        std::string status = "Ready - " + std::to_string(g_searchEngine->GetIndexedFileCount()) + " files indexed";
        if (g_searchEngine->IsIndexing()) {
            status += " (still indexing...)";
        }
        SetWindowTextA(g_hStatusBar, status.c_str());
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    auto results = g_searchEngine->SearchInstant(query);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    DisplayResults(results);

    std::string status = "Found " + std::to_string(results.size()) + " results in " +
                         std::to_string(searchTime.count()) + "ms";

    if (g_searchEngine->IsIndexing()) {
        status += " (indexing " + std::to_string(g_searchEngine->GetIndexedFileCount()) + " files...)";
    } else {
        status += " (from " + std::to_string(g_searchEngine->GetIndexedFileCount()) + " files)";
    }

    SetWindowTextA(g_hStatusBar, status.c_str());
}

void DisplayResults(const std::vector<std::string>& results) {
    ListView_DeleteAllItems(g_hResultsList);

    int index = 0;
    for (const auto& filePath : results) {
        if (index >= 100) break;

        LVITEMA lvItem = {};
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = index++;
        lvItem.iSubItem = 0;
        lvItem.pszText = const_cast<char*>(filePath.c_str());
        ListView_InsertItem(g_hResultsList, &lvItem);

        // Add file size
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(filePath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            ULONGLONG size = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            std::string sizeStr = FormatFileSize(size);
            ListView_SetItemText(g_hResultsList, index - 1, 1, const_cast<char*>(sizeStr.c_str()));
            FindClose(hFind);
        }
    }
}

std::string FormatFileSize(ULONGLONG size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double displaySize = static_cast<double>(size);

    while (displaySize >= 1024.0 && unitIndex < 4) {
        displaySize /= 1024.0;
        unitIndex++;
    }

    char buffer[32];
    if (unitIndex == 0) {
        sprintf_s(buffer, "%d %s", static_cast<int>(displaySize), units[unitIndex]);
    } else {
        sprintf_s(buffer, "%.1f %s", displaySize, units[unitIndex]);
    }
    return std::string(buffer);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "FastFileSearchWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassA(&wc);

    // Create main window
    g_hMainWnd = CreateWindowExA(
        0,
        "FastFileSearchWindow",
        "FastFileSearch - Parallel Instant Search",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hMainWnd) {
        MessageBoxA(NULL, "Failed to create window!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
