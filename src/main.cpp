#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <filesystem>
#include <iomanip>

using namespace Gdiplus;

// --- Constants & Globals ---
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_TOGGLE 1002

const UINT ID_TRAY_ICON = 1;
const wchar_t* CLASS_NAME = L"AxemanTrayClass";
const wchar_t* APP_TITLE = L"Axeman";

HWND g_hWindow = NULL;
NOTIFYICONDATA g_nid = {};
HICON g_hIconActive = NULL;
HICON g_hIconPaused = NULL;
std::atomic<bool> g_running(true);
std::atomic<bool> g_paused(false);

// Configuration
int g_checkIntervalMs = 500;
int g_thresholdPercent = 90;
std::wstring g_exePath;

// --- Helper Functions ---

HICON LoadPNGAsIcon(const std::wstring& path) {
    Bitmap* bitmap = Bitmap::FromFile(path.c_str());
    if (bitmap && bitmap->GetLastStatus() == Ok) {
        HICON hIcon = NULL;
        bitmap->GetHICON(&hIcon);
        delete bitmap;
        return hIcon;
    }
    delete bitmap;
    return NULL;
}

std::wstring GetExeDirectory() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::filesystem::path path(buffer);
    return path.parent_path().wstring();
}

void LogKill(const std::wstring& processName, DWORD pid, SIZE_T memoryFreed) {
    std::filesystem::path logPath(g_exePath);
    logPath /= L"axeman.log";

    std::wofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        logFile << std::put_time(&timeinfo, L"%Y-%m-%d %H:%M:%S") 
                << L" - KILLED: " << processName
                << L" (PID: " << pid << L")"
                << L" Freed: " << (memoryFreed / 1024 / 1024) << L" MB" << std::endl;
    }
}

void LoadConfig() {
    std::filesystem::path iniPath(g_exePath);
    iniPath /= L"axeman.ini";

    // Use default if file doesn't exist, otherwise read
    g_checkIntervalMs = GetPrivateProfileInt(L"Settings", L"IntervalMS", 500, iniPath.c_str());
    g_thresholdPercent = GetPrivateProfileInt(L"Settings", L"ThresholdPercent", 90, iniPath.c_str());
}

void ShowNotification(const std::wstring& title, const std::wstring& message) {
    wcscpy_s(g_nid.szInfoTitle, title.c_str());
    wcscpy_s(g_nid.szInfo, message.c_str());
    
    UINT oldFlags = g_nid.uFlags;
    g_nid.uFlags |= NIF_INFO;
    g_nid.dwInfoFlags = NIIF_WARNING; 
    
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);

    // Reset flags to prevent repeat notifications on status updates
    g_nid.uFlags = oldFlags;
}

// --- Axeman Logic ---

const std::vector<std::wstring> CRITICAL_PROCESSES = {
    L"System", L"Idle", L"wininit.exe", L"services.exe", L"lsass.exe", 
    L"winlogon.exe", L"csrss.exe", L"smss.exe", L"axeman.exe",
    L"explorer.exe", L"svchost.exe", L"dwm.exe", L"memory_compression"
};

bool IsCritical(const std::wstring& name) {
    for (const auto& critical : CRITICAL_PROCESSES) {
        if (_wcsicmp(name.c_str(), critical.c_str()) == 0) return true;
    }
    return false;
}

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    SIZE_T memoryUsage;
};

ProcessInfo FindBiggestProcess() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return { 0, L"", 0 };

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    ProcessInfo biggest = { 0, L"", 0 };

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (IsCritical(pe32.szExeFile)) continue;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    if (pmc.WorkingSetSize > biggest.memoryUsage) {
                        biggest.pid = pe32.th32ProcessID;
                        biggest.name = pe32.szExeFile;
                        biggest.memoryUsage = pmc.WorkingSetSize;
                    }
                }
                CloseHandle(hProcess);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return biggest;
}

void MonitorThread() {
    while (g_running) {
        if (!g_paused) {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                if (memInfo.dwMemoryLoad >= (DWORD)g_thresholdPercent) {
                    ProcessInfo target = FindBiggestProcess();
                    if (target.pid != 0) {
                        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, target.pid);
                        if (hProcess) {
                            if (TerminateProcess(hProcess, 1)) {
                                LogKill(target.name, target.pid, target.memoryUsage);
                                
                                std::wstringstream msg;
                                msg << L"Terminated " << target.name << L"\nFreed " 
                                    << (target.memoryUsage / 1024 / 1024) << L" MB";
                                ShowNotification(L"Axeman Triggered", msg.str());
                            }
                            CloseHandle(hProcess);
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(g_checkIntervalMs));
    }
}

// --- UI / Tray Logic ---

void UpdateTrayIcon() {
    std::wstring status = L"Axeman: ";
    status += g_paused ? L"Disabled" : L"Watching";
    wcscpy_s(g_nid.szTip, status.c_str());
    
    g_nid.hIcon = g_paused ? g_hIconPaused : g_hIconActive;
    g_nid.uFlags |= NIF_ICON;

    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    
    g_nid.uFlags &= ~NIF_INFO;
}

void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    
    if (g_paused) {
        AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Enable Axeman");
    } else {
        AppendMenu(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Disable Axeman");
    }
    
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(hwnd, pt);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            g_running = false;
            DestroyWindow(hwnd);
            break;
        case ID_TRAY_TOGGLE:
            g_paused = !g_paused;
            UpdateTrayIcon();
            break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_exePath = GetExeDirectory();
    LoadConfig();

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Try to load custom PNG icons from EXE folder
    std::filesystem::path activePath(g_exePath);
    activePath /= L"axe.png";
    std::filesystem::path inactivePath(g_exePath);
    inactivePath /= L"axe-inactive.png";

    g_hIconActive = LoadPNGAsIcon(activePath.wstring());
    g_hIconPaused = LoadPNGAsIcon(inactivePath.wstring());

    // Fallback to system icons if PNGs are missing
    if (!g_hIconActive) g_hIconActive = LoadIcon(NULL, IDI_SHIELD);
    if (!g_hIconPaused) g_hIconPaused = LoadIcon(NULL, IDI_APPLICATION);
    if (!g_hIconActive) g_hIconActive = LoadIcon(NULL, IDI_APPLICATION);

    // Register Window Class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Create Hidden Window
    g_hWindow = CreateWindowEx(0, CLASS_NAME, APP_TITLE, 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    if (g_hWindow == NULL) return 0;

    // Init Tray Icon
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = g_hWindow;
    g_nid.uID = ID_TRAY_ICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_hIconActive;

    wcscpy_s(g_nid.szTip, L"Axeman: Watching");
    
    Shell_NotifyIcon(NIM_ADD, &g_nid);

    // Start Worker Thread
    std::thread worker(MonitorThread);

    // Message Loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    g_running = false;
    if (worker.joinable()) worker.join();
    Shell_NotifyIcon(NIM_DELETE, &g_nid);

    if (g_hIconActive) DestroyIcon(g_hIconActive);
    if (g_hIconPaused) DestroyIcon(g_hIconPaused);
    GdiplusShutdown(gdiplusToken);

    return 0;
}