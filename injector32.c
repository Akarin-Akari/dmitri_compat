#include <windows.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: injector32.exe <PID> <DLL_PATH>\n");
        return 1;
    }

    DWORD pid = atoi(argv[1]);
    char *dll_path = argv[2];

    printf("PID: %d\n", pid);
    printf("DLL: %s\n", dll_path);

    // Open process
    HANDLE h_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h_process) {
        printf("ERROR: Cannot open process (error %d)\n", GetLastError());
        return 1;
    }
    printf("Process opened\n");

    // Get LoadLibraryA address
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID load_library = (LPVOID)GetProcAddress(kernel32, "LoadLibraryA");
    printf("LoadLibraryA: 0x%p\n", load_library);

    // Allocate memory
    SIZE_T dll_path_len = strlen(dll_path) + 1;
    LPVOID remote_mem = VirtualAllocEx(h_process, NULL, dll_path_len, 
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_mem) {
        printf("ERROR: VirtualAllocEx failed (error %d)\n", GetLastError());
        CloseHandle(h_process);
        return 1;
    }
    printf("Allocated at: 0x%p\n", remote_mem);

    // Write DLL path
    SIZE_T bytes_written;
    if (!WriteProcessMemory(h_process, remote_mem, dll_path, dll_path_len, &bytes_written)) {
        printf("ERROR: WriteProcessMemory failed (error %d)\n", GetLastError());
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        return 1;
    }
    printf("Wrote %zu bytes\n", bytes_written);

    // Create remote thread
    DWORD thread_id;
    HANDLE h_thread = CreateRemoteThread(h_process, NULL, 0,
                                         (LPTHREAD_START_ROUTINE)load_library,
                                         remote_mem, 0, &thread_id);
    if (!h_thread) {
        printf("ERROR: CreateRemoteThread failed (error %d)\n", GetLastError());
        VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
        CloseHandle(h_process);
        return 1;
    }
    printf("Thread created (TID: %d)\n", thread_id);

    // Wait for thread
    WaitForSingleObject(h_thread, INFINITE);

    // Get result
    DWORD exit_code;
    if (GetExitCodeThread(h_thread, &exit_code)) {
        printf("\nLoadLibraryA returned: 0x%X\n", exit_code);
        if (exit_code == 0) {
            printf("FAILED: LoadLibrary returned NULL\n");
            printf("Possible causes:\n");
            printf("  - DLL dependencies missing\n");
            printf("  - DllMain returned FALSE\n");
        } else {
            printf("SUCCESS: DLL loaded at 0x%X\n", exit_code);
        }
    }

    // Cleanup
    CloseHandle(h_thread);
    VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE);
    CloseHandle(h_process);

    return exit_code == 0 ? 1 : 0;
}
