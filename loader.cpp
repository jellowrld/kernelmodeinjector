#include <windows.h>
#include <iostream>
#include <psapi.h>
#include <comdlg.h>
#include <vector>
#include <string>

#define IOCTL_INJECT_DLL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct INJECTION_REQUEST {
    ULONG TargetPid;
    WCHAR DllPath[260];
};

ULONG SelectTargetProcess() {
    std::vector<std::pair<ULONG, std::wstring>> processes;
    DWORD aProcesses[1024], cbNeeded = 0;
    ULONG cb = 0;

    if (EnumProcesses(aProcesses, sizeof(aProcesses), &cb)) {
        for (ULONG i = 0; i < cb / sizeof(ULONG); i++) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
            if (hProcess) {
                char processName[MAX_PATH];
                if (GetProcessImageFileName(hProcess, processName, MAX_PATH)) {
                    std::wstring wProcessName(processName, processName + strlen(processName));
                    processes.push_back({ aProcesses[i], wProcessName });
                }
                CloseHandle(hProcess);
            }
        }
    }

    std::cout << "Select a process (PID):" << std::endl;
    for (size_t i = 0; i < processes.size(); i++) {
        std::cout << i + 1 << ". PID: " << processes[i].first << " - Name: " << processes[i].second << std::endl;
    }

    int choice;
    std::cout << "Enter your choice: ";
    std::cin >> choice;
    if (choice > 0 && choice <= processes.size()) {
        return processes[choice - 1].first;
    }

    return 0;
}

void SelectDllPath(WCHAR* dllPath) {
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = NULL;
    ofn.lpstrFile = dllPath;
    ofn.lpstrInitialDir = L"C:\\";
    ofn.lpstrTitle = L"Select DLL file";
    ofn.nMaxFile = 260;  // Max file size (in chars)
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn)) {
        std::cout << "Selected DLL path: " << dllPath << std::endl;
    }
}

int main() {
    HANDLE hDevice = CreateFileW(L"\\\\.\\NGSCat", GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open driver: " << GetLastError() << std::endl;
        return 1;
    }

    INJECTION_REQUEST Request = { 0 };
    ULONG targetPid = SelectTargetProcess();
    if (targetPid == 0) {
        std::cerr << "No process selected." << std::endl;
        return 1;
    }

    Request.TargetPid = targetPid;

    WCHAR dllPath[260];
    std::fill(dllPath, dllPath + 260, L'\0');  // Initialize buffer to null
    SelectDllPath(dllPath);

    if (wcscpy_s(Request.DllPath, dllPath) != 0) {
        std::cerr << "Failed to copy DLL path." << std::endl;
        return 1;
    }

    DWORD BytesReturned;
    if (!DeviceIoControl(hDevice, IOCTL_INJECT_DLL, &Request, sizeof(Request),
        NULL, 0, &BytesReturned, NULL)) {
        std::cerr << "IOCTL failed: " << GetLastError() << std::endl;
    }
    else {
        std::cout << "DLL injection requested successfully!" << std::endl;
    }

    CloseHandle(hDevice);
    return 0;
}
