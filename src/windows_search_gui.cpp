#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <searchapi.h>
#include <oledb.h>
#include <propkey.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <chrono>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// Control IDs
#define ID_SEARCH_EDIT      1001
#define ID_RESULTS_LIST     1002
#define ID_STATUS_BAR       1003

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hSearchEdit = NULL;
HWND g_hResultsList = NULL;
HWND g_hStatusBar = NULL;

// Search engine
class WindowsSearchEngine {
private:
    ISearchManager* m_pSearchManager;
    ISearchCatalogManager* m_pCatalogManager;
    ISearchQueryHelper* m_pQueryHelper;
    std::atomic<bool> m_isInitialized;
    std::thread m_backgroundIndexer;
    std::atomic<bool> m_shouldStop;
    
    // Fast in-memory index for instant results
    struct FileInfo {
        std::wstring path;
        std::wstring name;
        ULONGLONG size;
        FILETIME modified;
        std::wstring extension;
    };
    
    std::vector<FileInfo> m_fileIndex;
    std::mutex m_indexMutex;
    std::atomic<size_t> m_indexedFiles;
    
public:
    WindowsSearchEngine() : m_pSearchManager(nullptr), m_pCatalogManager(nullptr), 
                           m_pQueryHelper(nullptr), m_isInitialized(false), m_shouldStop(false), m_indexedFiles(0) {
        CoInitialize(NULL);
    }
    
    ~WindowsSearchEngine() {
        Shutdown();
        CoUninitialize();
    }
    
    bool Initialize() {
        // Try Windows Search API first
        HRESULT hr = CoCreateInstance(CLSID_SearchManager, NULL, CLSCTX_LOCAL_SERVER,
                                    IID_ISearchManager, (void**)&m_pSearchManager);
        
        if (SUCCEEDED(hr) && m_pSearchManager) {
            hr = m_pSearchManager->GetCatalog(L"SystemIndex", &m_pCatalogManager);
            if (SUCCEEDED(hr) && m_pCatalogManager) {
                hr = m_pCatalogManager->GetQueryHelper(&m_pQueryHelper);
                if (SUCCEEDED(hr)) {
                    m_isInitialized.store(true);
                    SetStatus(L"Connected to Windows Search Service");
                }
            }
        }
        
        // Start background indexer regardless of Windows Search availability
        StartBackgroundIndexing();
        
        SetStatus(L"Ready - Start typing to search");
        return true;
    }
    
    void Shutdown() {
        m_shouldStop.store(true);
        if (m_backgroundIndexer.joinable()) {
            m_backgroundIndexer.join();
        }
        
        if (m_pQueryHelper) { m_pQueryHelper->Release(); m_pQueryHelper = nullptr; }
        if (m_pCatalogManager) { m_pCatalogManager->Release(); m_pCatalogManager = nullptr; }
        if (m_pSearchManager) { m_pSearchManager->Release(); m_pSearchManager = nullptr; }
    }
    
    std::vector<std::wstring> SearchInstant(const std::wstring& query) {
        std::vector<std::wstring> results;
        
        if (query.empty() || query.length() < 2) {
            return results;
        }
        
        // First try Windows Search API for instant results
        if (m_isInitialized.load() && m_pQueryHelper) {
            results = SearchWithWindowsAPI(query);
            if (!results.empty()) {
                return results;
            }
        }
        
        // Fallback to our fast in-memory index
        results = SearchInMemoryIndex(query);
        
        return results;
    }
    
    size_t GetIndexedFileCount() const {
        return m_indexedFiles.load();
    }
    
private:
    std::vector<std::wstring> SearchWithWindowsAPI(const std::wstring& query) {
        std::vector<std::wstring> results;
        
        try {
            // Build SQL query for Windows Search
            std::wstring sqlQuery = L"SELECT System.ItemPathDisplay FROM SystemIndex WHERE ";
            sqlQuery += L"CONTAINS(System.FileName, '\"" + query + L"*\"') ";
            sqlQuery += L"ORDER BY System.DateModified DESC";
            
            ISearchQueryHelper* pQueryHelper = m_pQueryHelper;
            if (!pQueryHelper) return results;
            
            LPWSTR pszConnectionString = nullptr;
            LPWSTR pszQueryString = nullptr;
            
            HRESULT hr = pQueryHelper->get_ConnectionString(&pszConnectionString);
            if (SUCCEEDED(hr)) {
                hr = pQueryHelper->GenerateSQLFromUserQuery(query.c_str(), &pszQueryString);
                if (SUCCEEDED(hr) && pszQueryString) {
                    // Execute query (simplified - would need full OLE DB implementation)
                    results = ExecuteSearchQuery(pszConnectionString, pszQueryString);
                }
            }
            
            if (pszConnectionString) CoTaskMemFree(pszConnectionString);
            if (pszQueryString) CoTaskMemFree(pszQueryString);
            
        } catch (...) {
            // Fall back to memory search
        }
        
        return results;
    }
    
    std::vector<std::wstring> SearchInMemoryIndex(const std::wstring& query) {
        std::vector<std::wstring> results;
        std::lock_guard<std::mutex> lock(m_indexMutex);
        
        std::wstring lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::towlower);
        
        for (const auto& file : m_fileIndex) {
            std::wstring lowerName = file.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
            
            if (lowerName.find(lowerQuery) != std::wstring::npos) {
                results.push_back(file.path);
                if (results.size() >= 100) break; // Limit results for performance
            }
        }
        
        return results;
    }
    
    std::vector<std::wstring> ExecuteSearchQuery(LPCWSTR connectionString, LPCWSTR query) {
        std::vector<std::wstring> results;
        // This would require full OLE DB implementation
        // For now, return empty and rely on memory index
        return results;
    }
    
    void StartBackgroundIndexing() {
        m_backgroundIndexer = std::thread([this]() {
            BackgroundIndexingWorker();
        });
    }
    
    void BackgroundIndexingWorker() {
        SetStatus(L"Building search index in background...");
        
        // Get all drives
        DWORD drives = GetLogicalDrives();
        std::vector<std::wstring> drivePaths;
        
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i)) {
                wchar_t drive[4] = { L'A' + i, L':', L'\\', L'\0' };
                drivePaths.push_back(drive);
            }
        }
        
        // Index common user directories first for faster initial results
        std::vector<std::wstring> priorityPaths = {
            GetUserDocumentsPath(),
            GetUserDesktopPath(),
            GetUserDownloadsPath()
        };
        
        // Index priority paths first
        for (const auto& path : priorityPaths) {
            if (!path.empty() && !m_shouldStop.load()) {
                IndexDirectory(path, true);
            }
        }
        
        // Then index all drives
        for (const auto& drive : drivePaths) {
            if (!m_shouldStop.load()) {
                IndexDirectory(drive, false);
            }
        }
        
        SetStatus(L"Search index ready - " + std::to_wstring(m_indexedFiles.load()) + L" files indexed");
    }
    
    void IndexDirectory(const std::wstring& path, bool isPriority) {
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
                    // Skip system directories for performance
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) &&
                        fileName != L"System Volume Information" &&
                        fileName != L"$Recycle.Bin") {
                        IndexDirectory(fullPath, isPriority);
                    }
                } else {
                    // Add file to index
                    FileInfo fileInfo;
                    fileInfo.path = fullPath;
                    fileInfo.name = fileName;
                    fileInfo.size = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                    fileInfo.modified = findData.ftLastWriteTime;
                    
                    size_t dotPos = fileName.find_last_of(L'.');
                    if (dotPos != std::wstring::npos) {
                        fileInfo.extension = fileName.substr(dotPos + 1);
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(m_indexMutex);
                        m_fileIndex.push_back(fileInfo);
                    }
                    
                    m_indexedFiles.fetch_add(1);
                    
                    // Update status periodically
                    if (m_indexedFiles.load() % 1000 == 0) {
                        SetStatus(L"Indexing: " + std::to_wstring(m_indexedFiles.load()) + L" files found");
                    }
                }
                
            } while (FindNextFileW(hFind, &findData) && !m_shouldStop.load());
            
            FindClose(hFind);
            
        } catch (...) {
            // Skip directories we can't access
        }
    }
    
    std::wstring GetUserDocumentsPath() {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path))) {
            return std::wstring(path);
        }
        return L"";
    }
    
    std::wstring GetUserDesktopPath() {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, path))) {
            return std::wstring(path);
        }
        return L"";
    }
    
    std::wstring GetUserDownloadsPath() {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, path))) {
            return std::wstring(path) + L"\\Downloads";
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
std::unique_ptr<WindowsSearchEngine> g_searchEngine;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PerformLiveSearch();
void DisplayResults(const std::vector<std::wstring>& results);
std::wstring FormatFileSize(ULONGLONG size);

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateControls(hwnd);
            
            // Initialize search engine
            g_searchEngine = std::make_unique<WindowsSearchEngine>();
            g_searchEngine->Initialize();
            break;
            
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            // Resize controls
            MoveWindow(g_hSearchEdit, 20, 20, width - 40, 30, TRUE);
            MoveWindow(g_hResultsList, 20, 60, width - 40, height - 120, TRUE);
            MoveWindow(g_hStatusBar, 0, height - 25, width, 25, TRUE);
            break;
        }
        
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hSearchEdit) {
                // Live search as user types
                PerformLiveSearch();
            }
            break;
            
        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->hwndFrom == g_hResultsList && pnmh->code == NM_DBLCLK) {
                int selectedIndex = ListView_GetNextItem(g_hResultsList, -1, LVNI_SELECTED);
                if (selectedIndex >= 0) {
                    wchar_t filePath[MAX_PATH];
                    ListView_GetItemTextW(g_hResultsList, selectedIndex, 0, filePath, MAX_PATH);
                    
                    // Open file
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
    // Search box with placeholder-like behavior
    g_hSearchEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 20, 400, 30,
        hwnd,
        (HMENU)ID_SEARCH_EDIT,
        GetModuleHandle(NULL),
        NULL
    );

    // Set font for search box
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Results list view
    g_hResultsList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        20, 60, 500, 400,
        hwnd,
        (HMENU)ID_RESULTS_LIST,
        GetModuleHandle(NULL),
        NULL
    );

    // Add columns to list view
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = 400;
    column.pszText = const_cast<wchar_t*>(L"File Path");
    ListView_InsertColumn(g_hResultsList, 0, &column);

    column.cx = 100;
    column.pszText = const_cast<wchar_t*>(L"Size");
    ListView_InsertColumn(g_hResultsList, 1, &column);

    // Status bar
    g_hStatusBar = CreateWindowW(
        L"STATIC",
        L"Initializing search engine...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0,
        hwnd,
        (HMENU)ID_STATUS_BAR,
        GetModuleHandle(NULL),
        NULL
    );

    // Set focus to search box
    SetFocus(g_hSearchEdit);
}

void PerformLiveSearch() {
    if (!g_searchEngine) return;

    wchar_t searchText[256];
    GetWindowTextW(g_hSearchEdit, searchText, sizeof(searchText) / sizeof(wchar_t));

    std::wstring query(searchText);
    if (query.length() < 2) {
        ListView_DeleteAllItems(g_hResultsList);
        return;
    }

    // Perform instant search
    auto results = g_searchEngine->SearchInstant(query);
    DisplayResults(results);

    // Update status
    std::wstring status = L"Found " + std::to_wstring(results.size()) + L" results";
    if (g_searchEngine->GetIndexedFileCount() > 0) {
        status += L" (from " + std::to_wstring(g_searchEngine->GetIndexedFileCount()) + L" indexed files)";
    }
    SetWindowTextW(g_hStatusBar, status.c_str());
}

void DisplayResults(const std::vector<std::wstring>& results) {
    ListView_DeleteAllItems(g_hResultsList);

    int index = 0;
    for (const auto& filePath : results) {
        if (index >= 100) break; // Limit display for performance

        LVITEMW lvItem = {};
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = index++;
        lvItem.iSubItem = 0;
        lvItem.pszText = const_cast<wchar_t*>(filePath.c_str());
        ListView_InsertItem(g_hResultsList, &lvItem);

        // Add file size
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(filePath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            ULONGLONG size = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            std::wstring sizeStr = FormatFileSize(size);
            ListView_SetItemTextW(g_hResultsList, index - 1, 1, const_cast<wchar_t*>(sizeStr.c_str()));
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
    // Initialize COM
    CoInitialize(NULL);

    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FastFileSearchWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassW(&wc);

    // Create main window
    g_hMainWnd = CreateWindowExW(
        0,
        L"FastFileSearchWindow",
        L"FastFileSearch - Instant File Search",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"Failed to create window!", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
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

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
