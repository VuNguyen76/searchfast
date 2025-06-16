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
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <queue>

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
        std::wstring path;
        std::wstring name;
        std::wstring lowerName; // Pre-computed for fast search
        ULONGLONG size;
        FILETIME modified;
        std::wstring extension;
    };
    
    std::vector<FileInfo> m_fileIndex;
    std::mutex m_indexMutex;
    std::atomic<size_t> m_indexedFiles;
    std::atomic<bool> m_isIndexing;
    std::atomic<bool> m_shouldStop;
    std::thread m_backgroundIndexer;
    
    // Priority indexing for instant results
    std::queue<std::wstring> m_priorityPaths;
    std::queue<std::wstring> m_regularPaths;
    std::mutex m_queueMutex;
    
public:
    InstantSearchEngine() : m_indexedFiles(0), m_isIndexing(false), m_shouldStop(false) {}
    
    ~InstantSearchEngine() {
        Shutdown();
    }
    
    bool Initialize() {
        SetStatus(L"FastFileSearch ready - Start typing to search instantly");
        
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
    std::vector<std::wstring> SearchInstant(const std::wstring& query) {
        std::vector<std::wstring> results;
        
        if (query.empty() || query.length() < 1) {
            return results;
        }
        
        // Convert query to lowercase for fast comparison
        std::wstring lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::towlower);
        
        // Search in memory index - INSTANT results
        {
            std::lock_guard<std::mutex> lock(m_indexMutex);
            
            for (const auto& file : m_fileIndex) {
                // Fast string search on pre-computed lowercase names
                if (file.lowerName.find(lowerQuery) != std::wstring::npos) {
                    results.push_back(file.path);
                    if (results.size() >= 50) break; // Limit for UI performance
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
        SetStatus(L"Building search index... You can search immediately!");
        
        // Phase 1: Index user directories FIRST for instant results
        IndexUserDirectoriesFirst();
        
        // Phase 2: Index all drives in background
        IndexAllDrives();
        
        m_isIndexing.store(false);
        SetStatus(L"Search index complete - " + std::to_wstring(m_indexedFiles.load()) + L" files ready for instant search");
    }
    
    void IndexUserDirectoriesFirst() {
        // Get user directories for immediate indexing
        std::vector<std::wstring> userPaths = {
            GetSpecialFolderPath(CSIDL_MYDOCUMENTS),
            GetSpecialFolderPath(CSIDL_DESKTOP),
            GetSpecialFolderPath(CSIDL_PROFILE) + L"\\Downloads",
            GetSpecialFolderPath(CSIDL_PROFILE) + L"\\Pictures",
            GetSpecialFolderPath(CSIDL_PROFILE) + L"\\Videos",
            GetSpecialFolderPath(CSIDL_PROFILE) + L"\\Music"
        };
        
        SetStatus(L"Indexing user files for instant search...");
        
        for (const auto& path : userPaths) {
            if (!path.empty() && !m_shouldStop.load()) {
                IndexDirectoryFast(path);
                
                // Update status after each user directory
                SetStatus(L"Ready for search - " + std::to_wstring(m_indexedFiles.load()) + L" files indexed");
            }
        }
    }
    
    void IndexAllDrives() {
        // Get all drives
        DWORD drives = GetLogicalDrives();
        
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i) && !m_shouldStop.load()) {
                wchar_t drive[4] = { static_cast<wchar_t>(L'A' + i), L':', L'\\', L'\0' };
                
                // Skip system drives that are slow
                if (drive[0] == L'A' || drive[0] == L'B') continue;
                
                SetStatus(L"Indexing drive " + std::wstring(drive) + L" - " + 
                         std::to_wstring(m_indexedFiles.load()) + L" files found");
                
                IndexDirectoryFast(drive);
            }
        }
    }
    
    void IndexDirectoryFast(const std::wstring& path) {
        try {
            WIN32_FIND_DATAW findData;
            std::wstring searchPath = path + L"\\*";
            HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
            
            if (hFind == INVALID_HANDLE_VALUE) return;
            
            do {
                if (m_shouldStop.load()) break;
                
                std::wstring fileName = findData.cFileName;
                if (fileName == L"." || fileName == L"..") continue;
                
                std::wstring fullPath = path + L"\\" + fileName;
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Skip system and hidden directories for speed
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) &&
                        !(findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                        fileName != L"System Volume Information" &&
                        fileName != L"$Recycle.Bin" &&
                        fileName != L"Windows" &&
                        fileName != L"Program Files" &&
                        fileName != L"Program Files (x86)") {
                        
                        IndexDirectoryFast(fullPath);
                    }
                } else {
                    // Add file to index
                    FileInfo fileInfo;
                    fileInfo.path = fullPath;
                    fileInfo.name = fileName;
                    fileInfo.lowerName = fileName;
                    std::transform(fileInfo.lowerName.begin(), fileInfo.lowerName.end(), 
                                 fileInfo.lowerName.begin(), ::towlower);
                    
                    fileInfo.size = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                    fileInfo.modified = findData.ftLastWriteTime;
                    
                    size_t dotPos = fileName.find_last_of(L'.');
                    if (dotPos != std::wstring::npos) {
                        fileInfo.extension = fileName.substr(dotPos + 1);
                    }
                    
                    // Add to index
                    {
                        std::lock_guard<std::mutex> lock(m_indexMutex);
                        m_fileIndex.push_back(fileInfo);
                    }
                    
                    m_indexedFiles.fetch_add(1);
                    
                    // Update status every 500 files for responsiveness
                    if (m_indexedFiles.load() % 500 == 0) {
                        SetStatus(L"Indexing... " + std::to_wstring(m_indexedFiles.load()) + 
                                L" files ready for instant search");
                    }
                }
                
            } while (FindNextFileW(hFind, &findData) && !m_shouldStop.load());
            
            FindClose(hFind);
            
        } catch (...) {
            // Skip directories we can't access
        }
    }
    
    std::wstring GetSpecialFolderPath(int csidl) {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, path))) {
            return std::wstring(path);
        }
        return L"";
    }
    
    void SetStatus(const std::wstring& status) {
        if (g_hStatusBar) {
            SetWindowTextW(g_hStatusBar, status.c_str());
        }
    }
};

// Global search engine
std::unique_ptr<InstantSearchEngine> g_searchEngine;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PerformInstantSearch();
void DisplayResults(const std::vector<std::wstring>& results);
std::wstring FormatFileSize(ULONGLONG size);

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
                    wchar_t filePath[MAX_PATH];
                    ListView_GetItemText(g_hResultsList, selectedIndex, 0, filePath, MAX_PATH);
                    
                    // Open file with default application
                    ShellExecuteW(hwnd, L"open", filePath, NULL, NULL, SW_SHOWNORMAL);
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
    g_hSearchEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 20, 400, 35,
        hwnd,
        (HMENU)ID_SEARCH_EDIT,
        GetModuleHandle(NULL),
        NULL
    );

    // Set larger font for search box
    HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Results list view
    g_hResultsList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        20, 70, 500, 400,
        hwnd,
        (HMENU)ID_RESULTS_LIST,
        GetModuleHandle(NULL),
        NULL
    );

    // Configure list view columns
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = 500;
    column.pszText = const_cast<wchar_t*>(L"File Path");
    ListView_InsertColumn(g_hResultsList, 0, &column);

    column.cx = 100;
    column.pszText = const_cast<wchar_t*>(L"Size");
    ListView_InsertColumn(g_hResultsList, 1, &column);

    // Status bar
    g_hStatusBar = CreateWindowW(
        L"STATIC",
        L"Initializing instant search...",
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

    wchar_t searchText[256];
    GetWindowTextW(g_hSearchEdit, searchText, sizeof(searchText) / sizeof(wchar_t));

    std::wstring query(searchText);

    // Clear results if query is too short
    if (query.length() < 1) {
        ListView_DeleteAllItems(g_hResultsList);

        std::wstring status = L"Ready for instant search";
        if (g_searchEngine->GetIndexedFileCount() > 0) {
            status += L" - " + std::to_wstring(g_searchEngine->GetIndexedFileCount()) + L" files indexed";
        }
        SetWindowTextW(g_hStatusBar, status.c_str());
        return;
    }

    // Perform INSTANT search
    auto startTime = std::chrono::high_resolution_clock::now();
    auto results = g_searchEngine->SearchInstant(query);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    DisplayResults(results);

    // Update status with search performance
    std::wstring status = L"Found " + std::to_wstring(results.size()) + L" results in " +
                         std::to_wstring(searchTime.count()) + L"ms";

    if (g_searchEngine->IsIndexing()) {
        status += L" (still indexing " + std::to_wstring(g_searchEngine->GetIndexedFileCount()) + L" files...)";
    } else {
        status += L" (from " + std::to_wstring(g_searchEngine->GetIndexedFileCount()) + L" files)";
    }

    SetWindowTextW(g_hStatusBar, status.c_str());
}

void DisplayResults(const std::vector<std::wstring>& results) {
    ListView_DeleteAllItems(g_hResultsList);

    int index = 0;
    for (const auto& filePath : results) {
        if (index >= 100) break; // Limit display for UI performance

        LVITEMW lvItem = {};
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = index++;
        lvItem.iSubItem = 0;
        lvItem.pszText = const_cast<wchar_t*>(filePath.c_str());
        ListView_InsertItem(g_hResultsList, &lvItem);

        // Add file size (quick lookup)
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(filePath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            ULONGLONG size = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            std::wstring sizeStr = FormatFileSize(size);
            ListView_SetItemText(g_hResultsList, index - 1, 1, const_cast<wchar_t*>(sizeStr.c_str()));
            FindClose(hFind);
        }
    }
}

std::wstring FormatFileSize(ULONGLONG size) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    int unitIndex = 0;
    double displaySize = static_cast<double>(size);

    while (displaySize >= 1024.0 && unitIndex < 4) {
        displaySize /= 1024.0;
        unitIndex++;
    }

    wchar_t buffer[32];
    if (unitIndex == 0) {
        swprintf_s(buffer, L"%d %s", static_cast<int>(displaySize), units[unitIndex]);
    } else {
        swprintf_s(buffer, L"%.1f %s", displaySize, units[unitIndex]);
    }
    return std::wstring(buffer);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"InstantSearchWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassW(&wc);

    // Create main window
    g_hMainWnd = CreateWindowExW(
        0,
        L"InstantSearchWindow",
        L"FastFileSearch - Instant Search (Windows Search Style)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"Failed to create window!", L"Error", MB_OK | MB_ICONERROR);
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
