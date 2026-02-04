#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    SIZE_T memoryUsage;
};

// List of critical processes to never kill
const std::vector<std::wstring> CRITICAL_PROCESSES = {
    L"System", L"Idle", L"wininit.exe", L"services.exe", L"lsass.exe", 
    L"winlogon.exe", L"csrss.exe", L"smss.exe", L"axeman.exe",
    L"explorer.exe", L"svchost.exe"
};

bool IsCritical(const std::wstring& name) {
    for (const auto& critical : CRITICAL_PROCESSES) {
        if (_wcsicmp(name.c_str(), critical.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

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

void KillProcess(DWORD pid, const std::wstring& name) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        if (TerminateProcess(hProcess, 1)) {
            std::wcout << L"[AXE] Successfully terminated " << name << L" (PID: " << pid << L")" << std::endl;
        } else {
            std::wcerr << L"[ERROR] Failed to terminate " << name << L" (PID: " << pid << L")" << std::endl;
        }
        CloseHandle(hProcess);
    } else {
        std::wcerr << L"[ERROR] Could not open process " << name << L" (PID: " << pid << L") for termination." << std::endl;
    }
}

int main() {
    const int THRESHOLD_PERCENT = 90;
    std::cout << "Axeman OOM Killer started. Threshold: " << THRESHOLD_PERCENT << "%" << std::endl;

    while (true) {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            std::cout << "Memory Load: " << memInfo.dwMemoryLoad << "%" << std::endl;

            if (memInfo.dwMemoryLoad >= THRESHOLD_PERCENT) {
                std::cout << "[TRIGGER] Memory usage high! Hunting for the biggest consumer..." << std::endl;
                ProcessInfo target = FindBiggestProcess();
                if (target.pid != 0) {
                    std::wcout << L"[TARGET] Found " << target.name << L" using " 
                               << (target.memoryUsage / 1024 / 1024) << L" MB" << std::endl;
                    KillProcess(target.pid, target.name);
                } else {
                    std::cout << "[WARN] No suitable target found to kill." << std::endl;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}
