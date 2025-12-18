#include "logger.h"
#include "config.h"
#include "d3d11_hooks.h"
#include <windows.h>
#include <string>

using namespace DmitriCompat;

// 获取 DLL 所在目录
std::string GetDllDirectoryPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;

    // 使用一个临时变量来避免函数名冲突
    auto getCurrentModule = []() -> void* { return (void*)&GetDllDirectoryPath; };

    if (GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)getCurrentModule(),
        &hm)) {

        GetModuleFileNameA(hm, path, sizeof(path));
        std::string fullPath(path);
        size_t pos = fullPath.find_last_of("\\/");
        if (pos != std::string::npos) {
            return fullPath.substr(0, pos);
        }
    }
    return ".";
}

// 初始化函数
void Initialize() {
    // 获取 DLL 目录
    std::string dllDir = GetDllDirectoryPath();

    // 配置文件路径
    std::string configPath = dllDir + "\\config\\config.ini";

    // 日志文件路径
    std::string logPath = dllDir + "\\logs\\dmitri_compat.log";

    // 加载配置
    Config& config = Config::GetInstance();
    if (!config.Load(configPath)) {
        // 配置文件不存在，使用默认值
        logPath = dllDir + "\\logs\\dmitri_compat.log";
    }

    // 初始化日志
    LogLevel logLevel = static_cast<LogLevel>(config.GetLogLevel());
    Logger::GetInstance().Initialize(logPath, logLevel);

    LOG_INFO("╔════════════════════════════════════════════════════════════════╗");
    LOG_INFO("║          DmitriCompat - RTX 50 Compatibility Layer            ║");
    LOG_INFO("║                    Version 0.1.0 (MVP)                         ║");
    LOG_INFO("╚════════════════════════════════════════════════════════════════╝");
    LOG_INFO("");
    LOG_INFO("DLL Directory: %s", dllDir.c_str());
    LOG_INFO("Config Path: %s", configPath.c_str());
    LOG_INFO("Log Path: %s", logPath.c_str());
    LOG_INFO("");

    // 显示配置
    LOG_INFO("Configuration:");
    LOG_INFO("  [Fixes]");
    LOG_INFO("    TextureFormatConversion: %s",
        config.IsTextureFormatConversionEnabled() ? "Enabled" : "Disabled");
    LOG_INFO("    ColorSpaceCorrection: %s",
        config.IsColorSpaceCorrectionEnabled() ? "Enabled" : "Disabled");
    LOG_INFO("    GPUSync: %s",
        config.IsGPUSyncEnabled() ? "Enabled" : "Disabled");
    LOG_INFO("    ShaderRegisterRemap: %s",
        config.IsShaderRegisterRemapEnabled() ? "Enabled" : "Disabled");
    LOG_INFO("  [Debug]");
    LOG_INFO("    LogLevel: %d", config.GetLogLevel());
    LOG_INFO("    DumpTextures: %s",
        config.IsDumpTexturesEnabled() ? "Enabled" : "Disabled");
    LOG_INFO("    DumpShaders: %s",
        config.IsDumpShadersEnabled() ? "Enabled" : "Disabled");
    LOG_INFO("");

    // 初始化 D3D11 Hooks
    if (!D3D11Hooks::GetInstance().Initialize()) {
        LOG_ERROR("Failed to initialize D3D11 hooks!");
        return;
    }

    LOG_INFO("✓ DmitriCompat initialized successfully");
    LOG_INFO("✓ Waiting for DmitriRender to call D3D11 APIs...\n");
    Logger::GetInstance().Flush();
}

// 清理函数
void Shutdown() {
    LOG_INFO("\n╔════════════════════════════════════════════════════════════════╗");
    LOG_INFO("║                  DmitriCompat Shutting Down                   ║");
    LOG_INFO("╚════════════════════════════════════════════════════════════════╝\n");

    // 关闭 Hooks
    D3D11Hooks::GetInstance().Shutdown();

    // 关闭日志
    Logger::GetInstance().Shutdown();
}

// 延迟初始化线程
DWORD WINAPI InitializeThread(LPVOID lpParam) {
    (void)lpParam;

    // 等待一小段时间，确保主程序已经稳定
    Sleep(500);

    // 现在可以安全地初始化
    Initialize();

    return 0;
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;  // 未使用，避免警告

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // 禁用线程通知以提高性能
            DisableThreadLibraryCalls(hModule);

            // 在单独的线程中初始化，避免 DllMain 限制
            CreateThread(NULL, 0, InitializeThread, NULL, 0, NULL);
            break;

        case DLL_PROCESS_DETACH:
            // 清理
            Shutdown();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
