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

// Instant Search Engine - Windows Search Style
class InstantSearchEngine {
private:
    struct FileInfo {
        std::string path;
        std::string name;
        std::string lowerName; // Pre-computed for fast search
        ULONGLONG size;
    };
    
    std::vector<FileInfo> m_fileIndex;
    std::mutex m_indexMutex;
    std::atomic<size_t> m_indexedFiles;
    std::atomic<bool> m_isIndexing;
    std::atomic<bool> m_shouldStop;
    std::thread m_backgroundIndexer;
    
public:
    InstantSearchEngine() : m_indexedFiles(0), m_isIndexing(false), m_shouldStop(false) {}
    
    ~InstantSearchEngine() {
        Shutdown();
    }
    
    bool Initialize() {
        SetStatus("FastFileSearch ready - Start typing to search instantly!");
        
        // Start background indexing immediately
        StartBackgroundIndexing();
        
        return true;
    }
    
    void Shutdown() {
        m_shouldStop.store(true);
        if (m_backgroundIndexer.joinable()) {
            m_backgroundIndexer.join();
        }
    }
    
    // INSTANT SEARCH - Core feature like Windows Search
    std::vector<std::string> SearchInstant(const std::string& query) {
        std::vector<std::string> results;
        
        if (query.empty() || query.length() < 1) {
            return results;
        }
        
        // Convert query to lowercase for fast comparison
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        
        // Search in memory index - INSTANT results
        {
            std::lock_guard<std::mutex> lock(m_indexMutex);

            // Debug: Count files by drive
            size_t cDriveFiles = 0, dDriveFiles = 0, otherDriveFiles = 0;

            for (const auto& file : m_fileIndex) {
                // Count files by drive for debugging
                if (file.path.length() > 0) {
                    char drive = file.path[0];
                    if (drive == 'C' || drive == 'c') cDriveFiles++;
                    else if (drive == 'D' || drive == 'd') dDriveFiles++;
                    else otherDriveFiles++;
                }

                // Fast string search on pre-computed lowercase names
                if (file.lowerName.find(lowerQuery) != std::string::npos) {
                    results.push_back(file.path);
                    if (results.size() >= 50) break; // Limit for UI performance
                }
            }

            // Debug: Update status with drive breakdown
            if (g_hStatusBar) {
                std::string debugStatus = "Index: C:" + std::to_string(cDriveFiles) +
                                        " D:" + std::to_string(dDriveFiles) +
                                        " Other:" + std::to_string(otherDriveFiles);
                SetWindowTextA(g_hStatusBar, debugStatus.c_str());
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
        SetStatus("Building search index... You can search immediately!");
        
        // Phase 1: Index user directories FIRST for instant results
        IndexUserDirectoriesFirst();
        
        // Phase 2: Index all drives in background
        IndexAllDrives();
        
        m_isIndexing.store(false);
        SetStatus("Search index complete - " + std::to_string(m_indexedFiles.load()) + " files ready for instant search");
    }
    
    void IndexUserDirectoriesFirst() {
        // Get user directories for immediate indexing
        std::vector<std::string> userPaths = {
            GetSpecialFolderPath(CSIDL_MYDOCUMENTS),
            GetSpecialFolderPath(CSIDL_DESKTOP),
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Downloads",
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Pictures",
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Videos",
            GetSpecialFolderPath(CSIDL_PROFILE) + "\\Music"
        };
        
        SetStatus("Indexing user files for instant search...");
        
        for (const auto& path : userPaths) {
            if (!path.empty() && !m_shouldStop.load()) {
                IndexDirectoryFast(path);
                
                // Update status after each user directory
                SetStatus("Ready for search - " + std::to_string(m_indexedFiles.load()) + " files indexed");
            }
        }
    }
    
    void IndexAllDrives() {
        // Get all drives
        DWORD drives = GetLogicalDrives();
        
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i) && !m_shouldStop.load()) {
                char drive[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
                
                // Skip floppy drives
                if (drive[0] == 'A' || drive[0] == 'B') continue;
                
                SetStatus("Indexing drive " + std::string(drive) + " - " +
                         std::to_string(m_indexedFiles.load()) + " files found");

                // Debug: Count files per drive
                size_t filesBeforeDrive = m_indexedFiles.load();
                
                IndexDirectoryFast(drive);

                // Debug: Show files found per drive
                size_t filesAfterDrive = m_indexedFiles.load();
                size_t filesFromThisDrive = filesAfterDrive - filesBeforeDrive;
                SetStatus("Drive " + std::string(drive) + " completed - " +
                         std::to_string(filesFromThisDrive) + " files added (total: " +
                         std::to_string(filesAfterDrive) + ")");

                // Sleep a bit to show the status
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            }
        }
    }
    
    void IndexDirectoryFast(const std::string& path) {
        try {
            WIN32_FIND_DATAA findData;
            std::string searchPath = path + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
            
            if (hFind == INVALID_HANDLE_VALUE) return;
            
            do {
                if (m_shouldStop.load()) break;
                
                std::string fileName = findData.cFileName;
                if (fileName == "." || fileName == "..") continue;
                
                std::string fullPath = path + "\\" + fileName;
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Skip system and hidden directories for speed
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) &&
                        !(findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                        fileName != "System Volume Information" &&
                        fileName != "$Recycle.Bin" &&
                        fileName != "Windows" &&
                        fileName != "Program Files" &&
                        fileName != "Program Files (x86)") {
                        
                        IndexDirectoryFast(fullPath);
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
                    
                    // Add to index
                    {
                        std::lock_guard<std::mutex> lock(m_indexMutex);
                        m_fileIndex.push_back(fileInfo);
                    }
                    
                    m_indexedFiles.fetch_add(1);
                    
                    // Update status every 500 files for responsiveness
                    if (m_indexedFiles.load() % 500 == 0) {
                        SetStatus("Indexing... " + std::to_string(m_indexedFiles.load()) + 
                                " files ready for instant search");
                    }
                }
                
            } while (FindNextFileA(hFind, &findData) && !m_shouldStop.load());
            
            FindClose(hFind);
            
        } catch (...) {
            // Skip directories we can't access
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
std::unique_ptr<InstantSearchEngine> g_searchEngine;

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
            
            // Initialize instant search engine
            g_searchEngine = std::make_unique<InstantSearchEngine>();
            g_searchEngine->Initialize();
            break;
            
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            // Resize controls for optimal search experience
            MoveWindow(g_hSearchEdit, 20, 20, width - 40, 35, TRUE);
            MoveWindow(g_hResultsList, 20, 70, width - 40, height - 130, TRUE);
            MoveWindow(g_hStatusBar, 0, height - 25, width, 25, TRUE);
            break;
        }
        
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hSearchEdit) {
                // INSTANT SEARCH as user types - like Windows Search
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
                    
                    // Open file with default application
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
    // Large, prominent search box - Windows Search style
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

    // Set larger font for search box
    HFONT hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Results list view
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

    // Configure list view columns
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
        "Initializing instant search...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0,
        hwnd,
        (HMENU)ID_STATUS_BAR,
        GetModuleHandle(NULL),
        NULL
    );

    // Set focus to search box for immediate use
    SetFocus(g_hSearchEdit);
}

void PerformInstantSearch() {
    if (!g_searchEngine) return;

    char searchText[256];
    GetWindowTextA(g_hSearchEdit, searchText, sizeof(searchText));

    std::string query(searchText);

    // Clear results if query is too short
    if (query.length() < 1) {
        ListView_DeleteAllItems(g_hResultsList);

        std::string status = "Ready for instant search";
        if (g_searchEngine->GetIndexedFileCount() > 0) {
            status += " - " + std::to_string(g_searchEngine->GetIndexedFileCount()) + " files indexed";
        }
        SetWindowTextA(g_hStatusBar, status.c_str());
        return;
    }

    // Perform INSTANT search
    auto startTime = std::chrono::high_resolution_clock::now();
    auto results = g_searchEngine->SearchInstant(query);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    DisplayResults(results);

    // Update status with search performance
    std::string status = "Found " + std::to_string(results.size()) + " results in " +
                         std::to_string(searchTime.count()) + "ms";

    if (g_searchEngine->IsIndexing()) {
        status += " (still indexing " + std::to_string(g_searchEngine->GetIndexedFileCount()) + " files...)";
    } else {
        status += " (from " + std::to_string(g_searchEngine->GetIndexedFileCount()) + " files)";
    }

    SetWindowTextA(g_hStatusBar, status.c_str());
}

void DisplayResults(const std::vector<std::string>& results) {
    ListView_DeleteAllItems(g_hResultsList);

    int index = 0;
    for (const auto& filePath : results) {
        if (index >= 100) break; // Limit display for UI performance

        LVITEMA lvItem = {};
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = index++;
        lvItem.iSubItem = 0;
        lvItem.pszText = const_cast<char*>(filePath.c_str());
        ListView_InsertItem(g_hResultsList, &lvItem);

        // Add file size (quick lookup)
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
    wc.lpszClassName = "InstantSearchWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassA(&wc);

    // Create main window
    g_hMainWnd = CreateWindowExA(
        0,
        "InstantSearchWindow",
        "FastFileSearch - Instant Search (Windows Search Style)",
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
