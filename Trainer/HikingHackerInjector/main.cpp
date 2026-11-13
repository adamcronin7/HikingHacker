#include <iostream>
#include <windows.h>

DWORD getPID()
{
    // Finds the process ID of the game using the window name
    DWORD PID;
    HWND gameWindow = FindWindow(NULL, L"Hiking Hacker");
    GetWindowThreadProcessId(gameWindow, &PID);
    return PID;
}

void loadDll(HANDLE process, const wchar_t* dllPath)
{
    // Write the dll name to the game's memory
    int nameLen = wcslen(dllPath) + 1;
    LPVOID remoteString = VirtualAllocEx(process, NULL, nameLen * 2, MEM_COMMIT, PAGE_EXECUTE);
    WriteProcessMemory(process, remoteString, dllPath, nameLen * 2, NULL);

    // Get the address of LoadLibraryW()
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    LPVOID funcAdr = GetProcAddress(k32, "LoadLibraryW");

    // Create the thread to load the DLL using LoadLibraryW()
    HANDLE thread = CreateRemoteThread(process, NULL, NULL, (LPTHREAD_START_ROUTINE)funcAdr, remoteString, NULL, NULL);

    // Let the thread finish and clean up the thread
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

int main()
{
    // Get the process ID and obtain the process handle
    DWORD PID = getPID();
    HANDLE process = OpenProcess(
        PROCESS_VM_OPERATION |
        PROCESS_VM_READ |
        PROCESS_VM_WRITE,
        FALSE, PID);
    if (process == NULL) {
        printf("Error: %d, when trying to open PID: %d\n", GetLastError(), PID);
    }
    loadDll(process, L"C:\\update_func_call_hook.dll");
}