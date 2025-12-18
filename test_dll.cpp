#include <windows.h>

// 最简单的测试 DLL
// 只在 DllMain 中弹出消息框，不做任何 Hook

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // 简单的消息框，证明 DLL 被加载了
            MessageBoxA(NULL,
                "DmitriCompat Test DLL loaded successfully!\n\n"
                "This is a test to verify DLL injection works.\n"
                "Click OK to continue.",
                "DmitriCompat Test",
                MB_OK | MB_ICONINFORMATION);
            break;

        case DLL_PROCESS_DETACH:
            MessageBoxA(NULL, "DmitriCompat Test DLL unloaded", "DmitriCompat Test", MB_OK);
            break;
    }
    return TRUE;
}
