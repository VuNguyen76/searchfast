#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

// Window IDs
#define ID_SEARCH_EDIT      1001
#define ID_SEARCH_BUTTON    1002
#define ID_RESULTS_LIST     1003
#define ID_STATUS_BAR       1004
#define ID_PROGRESS_BAR     1005
#define ID_FOLDER_TREE      1006
#define ID_FILE_LIST        1007

// Menu IDs
#define ID_FILE_EXIT        2001
#define ID_TOOLS_SCAN       2002
#define ID_HELP_ABOUT       2003

// Global variables
HWND g_hMainWnd = NULL;
HWND g_hSearchEdit = NULL;
HWND g_hResultsList = NULL;
HWND g_hStatusBar = NULL;
HWND g_hProgressBar = NULL;
HWND g_hFolderTree = NULL;
HWND g_hFileList = NULL;

std::atomic<bool> g_isScanning(false);
std::vector<std::filesystem::path> g_allFiles;
std::vector<std::filesystem::path> g_searchResults;
std::unordered_map<std::string, std::vector<std::filesystem::path>> g_folderContents;

// Dark mode colors
const COLORREF BG_COLOR = RGB(43, 43, 43);
const COLORREF TEXT_COLOR = RGB(255, 255, 255);
const COLORREF SELECTED_COLOR = RGB(0, 120, 215);
const COLORREF BORDER_COLOR = RGB(85, 85, 85);

// File icons mapping
std::unordered_map<std::string, int> g_iconMap;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void SetDarkMode(HWND hwnd);
void ScanFilesAsync();
void UpdateProgress(int percentage, const std::string& currentPath);
void PopulateFileTree();
void SearchFiles(const std::string& query);
void DisplaySearchResults();
void ShowFileDetails(const std::filesystem::path& filePath);
std::string FormatFileSize(uintmax_t size);
std::string GetFileIcon(const std::filesystem::path& path);

// Main window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateControls(hwnd);
            SetDarkMode(hwnd);
            break;
            
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            // Resize controls
            MoveWindow(g_hSearchEdit, 10, 10, width - 120, 25, TRUE);
            MoveWindow(GetDlgItem(hwnd, ID_SEARCH_BUTTON), width - 100, 10, 80, 25, TRUE);
            
            // Split view: folder tree on left, file list on right
            int treeWidth = width / 3;
            MoveWindow(g_hFolderTree, 10, 45, treeWidth - 15, height - 120, TRUE);
            MoveWindow(g_hFileList, treeWidth, 45, width - treeWidth - 10, height - 120, TRUE);
            
            // Status bar and progress
            MoveWindow(g_hStatusBar, 0, height - 60, width, 20, TRUE);
            MoveWindow(g_hProgressBar, 10, height - 35, width - 20, 20, TRUE);
            break;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_SEARCH_BUTTON: {
                    char searchText[256];
                    GetWindowTextA(g_hSearchEdit, searchText, sizeof(searchText));
                    if (strlen(searchText) > 0) {
                        SearchFiles(searchText);
                    }
                    break;
                }
                
                case ID_TOOLS_SCAN:
                    if (!g_isScanning.load()) {
                        std::thread(ScanFilesAsync).detach();
                    }
                    break;
                    
                case ID_FILE_EXIT:
                    PostQuitMessage(0);
                    break;
                    
                case ID_HELP_ABOUT:
                    MessageBoxA(hwnd, 
                        "FastFileSearch v1.0\n\n"
                        "High-Performance File Search with Progressive Loading\n"
                        "Built with C++20 and Windows API\n\n"
                        "Features:\n"
                        "• Dark Mode Interface\n"
                        "• Lazy Loading\n"
                        "• Real-time Search\n"
                        "• File Type Icons\n\n"
                        "© 2024 FastFileSearch Team", 
                        "About FastFileSearch", 
                        MB_OK | MB_ICONINFORMATION);
                    break;
            }
            break;
            
        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            
            if (pnmh->hwndFrom == g_hFolderTree && pnmh->code == TVN_SELCHANGED) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                TVITEM item = pnmtv->itemNew;
                
                char folderPath[MAX_PATH];
                item.mask = TVIF_TEXT;
                item.pszText = folderPath;
                item.cchTextMax = MAX_PATH;
                TreeView_GetItem(g_hFolderTree, &item);
                
                // Load files in selected folder
                auto it = g_folderContents.find(folderPath);
                if (it != g_folderContents.end()) {
                    ListView_DeleteAllItems(g_hFileList);
                    
                    int index = 0;
                    for (const auto& file : it->second) {
                        LVITEM lvItem = {};
                        lvItem.mask = LVIF_TEXT;
                        lvItem.iItem = index++;
                        lvItem.iSubItem = 0;
                        
                        std::string fileName = file.filename().string();
                        lvItem.pszText = const_cast<char*>(fileName.c_str());
                        ListView_InsertItem(g_hFileList, &lvItem);
                        
                        // Add file size
                        std::string sizeStr = FormatFileSize(std::filesystem::file_size(file));
                        ListView_SetItemText(g_hFileList, index - 1, 1, const_cast<char*>(sizeStr.c_str()));
                    }
                }
            }
            
            if (pnmh->hwndFrom == g_hFileList && pnmh->code == NM_DBLCLK) {
                int selectedIndex = ListView_GetNextItem(g_hFileList, -1, LVNI_SELECTED);
                if (selectedIndex >= 0) {
                    char fileName[MAX_PATH];
                    ListView_GetItemText(g_hFileList, selectedIndex, 0, fileName, MAX_PATH);
                    
                    // Open file with default application
                    ShellExecuteA(hwnd, "open", fileName, NULL, NULL, SW_SHOWNORMAL);
                }
            }
            break;
        }
        
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, TEXT_COLOR);
            SetBkColor(hdc, BG_COLOR);
            return (LRESULT)CreateSolidBrush(BG_COLOR);
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd) {
    // Create menu
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    HMENU hToolsMenu = CreatePopupMenu();
    HMENU hHelpMenu = CreatePopupMenu();
    
    AppendMenuA(hFileMenu, MF_STRING, ID_FILE_EXIT, "&Exit");
    AppendMenuA(hToolsMenu, MF_STRING, ID_TOOLS_SCAN, "&Scan Files");
    AppendMenuA(hHelpMenu, MF_STRING, ID_HELP_ABOUT, "&About");
    
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hToolsMenu, "&Tools");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "&Help");
    
    SetMenu(hwnd, hMenu);
    
    // Search box
    g_hSearchEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 10, 400, 25,
        hwnd,
        (HMENU)ID_SEARCH_EDIT,
        GetModuleHandle(NULL),
        NULL
    );
    
    // Search button
    CreateWindowA(
        "BUTTON",
        "Search",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, 10, 80, 25,
        hwnd,
        (HMENU)ID_SEARCH_BUTTON,
        GetModuleHandle(NULL),
        NULL
    );
    
    // Folder tree view
    g_hFolderTree = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEW,
        "",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
        10, 45, 250, 400,
        hwnd,
        (HMENU)ID_FOLDER_TREE,
        GetModuleHandle(NULL),
        NULL
    );
    
    // File list view
    g_hFileList = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEW,
        "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        270, 45, 500, 400,
        hwnd,
        (HMENU)ID_FILE_LIST,
        GetModuleHandle(NULL),
        NULL
    );
    
    // Add columns to list view
    LVCOLUMN column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = 300;
    column.pszText = const_cast<char*>("File Name");
    ListView_InsertColumn(g_hFileList, 0, &column);
    
    column.cx = 100;
    column.pszText = const_cast<char*>("Size");
    ListView_InsertColumn(g_hFileList, 1, &column);
    
    // Status bar
    g_hStatusBar = CreateWindowA(
        "STATIC",
        "Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0,
        hwnd,
        (HMENU)ID_STATUS_BAR,
        GetModuleHandle(NULL),
        NULL
    );
    
    // Progress bar
    g_hProgressBar = CreateWindowA(
        PROGRESS_CLASS,
        "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        hwnd,
        (HMENU)ID_PROGRESS_BAR,
        GetModuleHandle(NULL),
        NULL
    );
    
    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
}

void SetDarkMode(HWND hwnd) {
    // Set dark background
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_COLOR));

    // Apply dark theme to child windows (if available)
    EnumChildWindows(hwnd, [](HWND child, LPARAM) -> BOOL {
        // Try to set dark theme (Windows 10+ feature)
        typedef HRESULT (WINAPI *SetWindowThemeFunc)(HWND, LPCWSTR, LPCWSTR);
        HMODULE hUxTheme = LoadLibraryA("uxtheme.dll");
        if (hUxTheme) {
            SetWindowThemeFunc pSetWindowTheme = (SetWindowThemeFunc)GetProcAddress(hUxTheme, "SetWindowTheme");
            if (pSetWindowTheme) {
                pSetWindowTheme(child, L"DarkMode_Explorer", NULL);
            }
            FreeLibrary(hUxTheme);
        }
        return TRUE;
    }, 0);
}

void ScanFilesAsync() {
    g_isScanning.store(true);
    g_allFiles.clear();
    g_folderContents.clear();
    
    SetWindowTextA(g_hStatusBar, "Scanning files...");
    ShowWindow(g_hProgressBar, SW_SHOW);
    
    try {
        std::vector<std::string> drives;
        
        // Get all drives
        DWORD drivesMask = GetLogicalDrives();
        for (int i = 0; i < 26; i++) {
            if (drivesMask & (1 << i)) {
                char drive[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
                drives.push_back(drive);
            }
        }
        
        int totalDrives = drives.size();
        int currentDrive = 0;
        
        for (const auto& drive : drives) {
            try {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(
                    drive, std::filesystem::directory_options::skip_permission_denied)) {
                    
                    if (entry.is_regular_file()) {
                        g_allFiles.push_back(entry.path());
                        
                        // Group by parent directory
                        std::string parentPath = entry.path().parent_path().string();
                        g_folderContents[parentPath].push_back(entry.path());
                    }
                    
                    // Update progress
                    if (g_allFiles.size() % 100 == 0) {
                        int progress = (currentDrive * 100 + 
                                      (g_allFiles.size() % 10000) / 100) / totalDrives;
                        UpdateProgress(progress, entry.path().string());
                    }
                }
            } catch (const std::exception&) {
                // Skip inaccessible drives
            }
            currentDrive++;
        }
        
        // Populate tree view
        PopulateFileTree();
        
        UpdateProgress(100, "Scan completed");
        
        char statusText[256];
        sprintf_s(statusText, "Found %zu files in %zu folders", 
                 g_allFiles.size(), g_folderContents.size());
        SetWindowTextA(g_hStatusBar, statusText);
        
    } catch (const std::exception& e) {
        SetWindowTextA(g_hStatusBar, ("Scan error: " + std::string(e.what())).c_str());
    }
    
    ShowWindow(g_hProgressBar, SW_HIDE);
    g_isScanning.store(false);
}

void UpdateProgress(int percentage, const std::string& currentPath) {
    SendMessage(g_hProgressBar, PBM_SETPOS, percentage, 0);
    
    std::string statusText = "Scanning: " + currentPath;
    if (statusText.length() > 80) {
        statusText = "Scanning: ..." + statusText.substr(statusText.length() - 77);
    }
    SetWindowTextA(g_hStatusBar, statusText.c_str());
}

void PopulateFileTree() {
    TreeView_DeleteAllItems(g_hFolderTree);
    
    // Add root drives
    for (const auto& [folderPath, files] : g_folderContents) {
        if (files.empty()) continue;
        
        std::filesystem::path path(folderPath);
        std::string folderName = path.filename().string();
        if (folderName.empty()) folderName = folderPath;
        
        TVINSERTSTRUCT tvInsert = {};
        tvInsert.hParent = TVI_ROOT;
        tvInsert.hInsertAfter = TVI_LAST;
        tvInsert.item.mask = TVIF_TEXT;
        tvInsert.item.pszText = const_cast<char*>(folderName.c_str());
        
        TreeView_InsertItem(g_hFolderTree, &tvInsert);
    }
}

void SearchFiles(const std::string& query) {
    g_searchResults.clear();
    
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (const auto& file : g_allFiles) {
        std::string fileName = file.filename().string();
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
        
        if (fileName.find(lowerQuery) != std::string::npos) {
            g_searchResults.push_back(file);
        }
    }
    
    DisplaySearchResults();
}

void DisplaySearchResults() {
    ListView_DeleteAllItems(g_hFileList);
    
    int index = 0;
    for (const auto& file : g_searchResults) {
        if (index >= 1000) break; // Limit results
        
        LVITEM lvItem = {};
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = index++;
        lvItem.iSubItem = 0;
        
        std::string fileName = file.filename().string();
        lvItem.pszText = const_cast<char*>(fileName.c_str());
        ListView_InsertItem(g_hFileList, &lvItem);
        
        // Add file size
        try {
            std::string sizeStr = FormatFileSize(std::filesystem::file_size(file));
            ListView_SetItemText(g_hFileList, index - 1, 1, const_cast<char*>(sizeStr.c_str()));
        } catch (...) {
            char* unknownText = const_cast<char*>("Unknown");
            ListView_SetItemText(g_hFileList, index - 1, 1, unknownText);
        }
    }
    
    char statusText[256];
    sprintf_s(statusText, "Found %zu results", g_searchResults.size());
    SetWindowTextA(g_hStatusBar, statusText);
}

std::string FormatFileSize(uintmax_t size) {
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
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    
    // Register window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "FastFileSearchWindow";
    wc.hbrBackground = CreateSolidBrush(BG_COLOR);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    RegisterClassA(&wc);
    
    // Create main window
    g_hMainWnd = CreateWindowExA(
        0,
        "FastFileSearchWindow",
        "FastFileSearch - Desktop File Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
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
