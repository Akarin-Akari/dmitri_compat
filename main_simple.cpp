#include <windows.h>
#include <fstream>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        // Create log directory if needed
        CreateDirectoryA("C:\\Users\\Akari\\AppData\\Roaming\\DmitriRender\\dmitri_compat\\build_32bit\\bin\\logs", NULL);

        // Simple file log
        std::ofstream log("C:\\Users\\Akari\\AppData\\Roaming\\DmitriRender\\dmitri_compat\\build_32bit\\bin\\logs\\simple.txt");
        log << "DmitriCompat DLL loaded into drtm.exe!\n";
        log << "Process ID: " << GetCurrentProcessId() << "\n";
        log.close();
        
        MessageBoxA(NULL, "DmitriCompat Simple loaded into drtm.exe!", "Success", MB_OK);
    }
    return TRUE;
}
