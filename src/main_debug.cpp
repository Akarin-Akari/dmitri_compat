#include <windows.h>
#include <string>
#include <cstdio>
#include "../external/minhook/include/MinHook.h"
#include "../include/logger.h"
#include "../include/config.h"
#include "../include/d3d11_hooks.h"

using namespace DmitriCompat;

// 超简化版本 - 只测试基本初始化
// 逐步添加功能来定位崩溃点

// 调试级别：
// LEVEL 0: 只弹MessageBox（已知可行）
// LEVEL 1: 尝试创建日志文件 ✓
// LEVEL 2: 尝试加载Config ✓
// LEVEL 3: 尝试初始化MinHook ✓
// LEVEL 4: 尝试初始化Logger类 ✓
// LEVEL 5: 尝试初始化Config类（完整INI解析） ✓
// LEVEL 6: 尝试初始化D3D11Hooks（最可能的崩溃点！） ✓
// LEVEL 7: 完整模拟原版Initialize()流程（所有组件一起） ✓
// LEVEL 8: 生产版本 - 初始化后保持运行，捕获实际D3D11调用

#define DEBUG_LEVEL 8

std::string GetDllDirectoryPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;

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

void TestLevel1_LogFile() {
    // 测试：能否创建日志文件？
    std::string dllDir = GetDllDirectoryPath();
    std::string logDir = dllDir + "\\logs";
    std::string logPath = logDir + "\\test_debug.log";

    // 创建logs目录
    CreateDirectoryA(logDir.c_str(), NULL);

    // 尝试写入日志
    FILE* f = fopen(logPath.c_str(), "w");
    if (f) {
        fprintf(f, "DEBUG: Log file creation successful!\n");
        fprintf(f, "DLL Directory: %s\n", dllDir.c_str());
        fclose(f);
        MessageBoxA(NULL,
            "Level 1 PASSED!\n\nLog file created successfully.\nCheck: logs/test_debug.log",
            "Debug Level 1", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(NULL,
            "Level 1 FAILED!\n\nCannot create log file.",
            "Debug Level 1", MB_OK | MB_ICONERROR);
    }
}

DWORD WINAPI InitializeThread(LPVOID lpParam) {
    (void)lpParam;
    Sleep(500);

#if DEBUG_LEVEL == 0
    MessageBoxA(NULL, "Level 0: Basic DllMain works!", "Debug Level 0", MB_OK);
#elif DEBUG_LEVEL == 1
    TestLevel1_LogFile();
#elif DEBUG_LEVEL == 2
    // Test Config loading
    std::string dllDir = GetDllDirectoryPath();
    std::string configPath = dllDir + "\\config\\config.ini";
    std::string logPath = dllDir + "\\logs\\test_debug.log";

    CreateDirectoryA((dllDir + "\\logs").c_str(), NULL);

    FILE* f = fopen(logPath.c_str(), "w");
    if (f) {
        fprintf(f, "DEBUG Level 2: Testing Config loading\n");
        fprintf(f, "Config path: %s\n", configPath.c_str());

        // 检查config文件是否存在
        FILE* configFile = fopen(configPath.c_str(), "r");
        if (configFile) {
            fprintf(f, "Config file exists!\n");
            fclose(configFile);

            // 尝试简单读取config文件内容
            configFile = fopen(configPath.c_str(), "r");
            char line[256];
            int lineCount = 0;
            while (fgets(line, sizeof(line), configFile) && lineCount < 10) {
                fprintf(f, "  Line %d: %s", lineCount++, line);
            }
            fclose(configFile);

            MessageBoxA(NULL,
                "Level 2 PASSED!\n\nConfig file found and readable.\nCheck logs/test_debug.log",
                "Debug Level 2", MB_OK | MB_ICONINFORMATION);
        } else {
            fprintf(f, "Config file NOT found (will use defaults)\n");
            MessageBoxA(NULL,
                "Level 2 PASSED (with warning)!\n\nConfig file not found, but this is OK.\nWill use default values.",
                "Debug Level 2", MB_OK | MB_ICONWARNING);
        }

        fclose(f);
    } else {
        MessageBoxA(NULL, "Level 2 FAILED!\n\nCannot create log file.", "Debug Level 2", MB_OK | MB_ICONERROR);
    }
#elif DEBUG_LEVEL == 3
    // Test MinHook initialization
    std::string dllDir = GetDllDirectoryPath();
    std::string logPath = dllDir + "\\logs\\test_debug.log";

    CreateDirectoryA((dllDir + "\\logs").c_str(), NULL);

    FILE* f = fopen(logPath.c_str(), "w");
    if (f) {
        fprintf(f, "DEBUG Level 3: Testing MinHook initialization\n");

        // 尝试初始化MinHook
        fprintf(f, "Calling MH_Initialize()...\n");
        MH_STATUS status = MH_Initialize();

        if (status == MH_OK) {
            fprintf(f, "MH_Initialize() SUCCESS!\n");

            // 立即反初始化
            fprintf(f, "Calling MH_Uninitialize()...\n");
            MH_STATUS uninit_status = MH_Uninitialize();
            fprintf(f, "MH_Uninitialize() returned: %d\n", uninit_status);

            fclose(f);

            MessageBoxA(NULL,
                "Level 3 PASSED!\n\nMinHook initialized successfully!\nCheck logs/test_debug.log",
                "Debug Level 3", MB_OK | MB_ICONINFORMATION);
        } else {
            fprintf(f, "MH_Initialize() FAILED! Status: %d\n", status);
            fclose(f);

            char errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                "Level 3 FAILED!\n\nMinHook initialization failed.\nStatus: %d\nCheck logs/test_debug.log",
                status);
            MessageBoxA(NULL, errorMsg, "Debug Level 3", MB_OK | MB_ICONERROR);
        }
    } else {
        MessageBoxA(NULL, "Level 3 FAILED!\n\nCannot create log file.", "Debug Level 3", MB_OK | MB_ICONERROR);
    }
#elif DEBUG_LEVEL == 4
    // Test Logger class initialization
    std::string dllDir = GetDllDirectoryPath();
    std::string logPath = dllDir + "\\logs\\test_logger.log";

    CreateDirectoryA((dllDir + "\\logs").c_str(), NULL);

    // 先用简单文件写入来记录过程
    FILE* f = fopen((dllDir + "\\logs\\test_debug.log").c_str(), "w");
    if (f) {
        fprintf(f, "DEBUG Level 4: Testing Logger class initialization\n");
        fprintf(f, "Log path: %s\n", logPath.c_str());
        fprintf(f, "Calling Logger::Initialize()...\n");
        fflush(f);

        // 尝试初始化Logger类
        bool success = false;
        try {
            Logger::GetInstance().Initialize(logPath, LogLevel::Info);
            fprintf(f, "Logger::Initialize() SUCCESS!\n");
            fflush(f);

            // 尝试写入日志
            LOG_INFO("Test message from Level 4");
            fprintf(f, "LOG_INFO() called\n");
            fflush(f);

            // 刷新日志
            Logger::GetInstance().Flush();
            fprintf(f, "Logger::Flush() called\n");
            fflush(f);

            // 关闭Logger
            Logger::GetInstance().Shutdown();
            fprintf(f, "Logger::Shutdown() called\n");
            fflush(f);

            success = true;
        } catch (const std::exception& e) {
            fprintf(f, "EXCEPTION caught: %s\n", e.what());
            fflush(f);
        } catch (...) {
            fprintf(f, "UNKNOWN EXCEPTION caught\n");
            fflush(f);
        }

        fclose(f);

        if (success) {
            MessageBoxA(NULL,
                "Level 4 PASSED!\n\nLogger class initialized successfully!\nCheck logs/test_logger.log and test_debug.log",
                "Debug Level 4", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL,
                "Level 4 FAILED!\n\nLogger initialization threw exception.\nCheck logs/test_debug.log",
                "Debug Level 4", MB_OK | MB_ICONERROR);
        }
    } else {
        MessageBoxA(NULL, "Level 4 FAILED!\n\nCannot create debug log file.", "Debug Level 4", MB_OK | MB_ICONERROR);
    }
#elif DEBUG_LEVEL == 5
    // Test Config class initialization
    std::string dllDir = GetDllDirectoryPath();
    std::string configPath = dllDir + "\\config\\config.ini";
    std::string logPath = dllDir + "\\logs\\test_debug.log";

    CreateDirectoryA((dllDir + "\\logs").c_str(), NULL);

    FILE* f = fopen(logPath.c_str(), "w");
    if (f) {
        fprintf(f, "DEBUG Level 5: Testing Config class initialization\n");
        fprintf(f, "Config path: %s\n", configPath.c_str());
        fprintf(f, "Calling Config::Load()...\n");
        fflush(f);

        bool success = false;
        try {
            Config& config = Config::GetInstance();
            fprintf(f, "Config::GetInstance() SUCCESS\n");
            fflush(f);

            bool loaded = config.Load(configPath);
            fprintf(f, "Config::Load() returned: %s\n", loaded ? "true" : "false");
            fflush(f);

            // 尝试读取配置值
            bool textureConv = config.IsTextureFormatConversionEnabled();
            bool colorSpace = config.IsColorSpaceCorrectionEnabled();
            int logLevel = config.GetLogLevel();

            fprintf(f, "Config values:\n");
            fprintf(f, "  TextureFormatConversion: %d\n", textureConv);
            fprintf(f, "  ColorSpaceCorrection: %d\n", colorSpace);
            fprintf(f, "  LogLevel: %d\n", logLevel);
            fflush(f);

            success = true;
        } catch (const std::exception& e) {
            fprintf(f, "EXCEPTION caught: %s\n", e.what());
            fflush(f);
        } catch (...) {
            fprintf(f, "UNKNOWN EXCEPTION caught\n");
            fflush(f);
        }

        fclose(f);

        if (success) {
            MessageBoxA(NULL,
                "Level 5 PASSED!\n\nConfig class initialized successfully!\nCheck logs/test_debug.log",
                "Debug Level 5", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL,
                "Level 5 FAILED!\n\nConfig initialization threw exception.\nCheck logs/test_debug.log",
                "Debug Level 5", MB_OK | MB_ICONERROR);
        }
    } else {
        MessageBoxA(NULL, "Level 5 FAILED!\n\nCannot create debug log file.", "Debug Level 5", MB_OK | MB_ICONERROR);
    }
#elif DEBUG_LEVEL == 6
    // Test D3D11Hooks initialization - THE FINAL TEST!
    std::string dllDir = GetDllDirectoryPath();
    std::string logPath = dllDir + "\\logs\\test_debug.log";

    CreateDirectoryA((dllDir + "\\logs").c_str(), NULL);

    FILE* f = fopen(logPath.c_str(), "w");
    if (f) {
        fprintf(f, "DEBUG Level 6: Testing D3D11Hooks initialization\n");
        fprintf(f, "This is the FINAL test - most likely crash point!\n");
        fprintf(f, "\n");

        bool success = false;
        try {
            // 先初始化MinHook（D3D11Hooks需要）
            fprintf(f, "Step 1: Initializing MinHook...\n");
            fflush(f);

            MH_STATUS status = MH_Initialize();
            if (status != MH_OK) {
                fprintf(f, "  MinHook initialization FAILED! Status: %d\n", status);
                fclose(f);
                MessageBoxA(NULL, "Level 6 FAILED!\n\nMinHook init failed.", "Debug Level 6", MB_OK | MB_ICONERROR);
                return 0;
            }
            fprintf(f, "  MinHook initialized OK\n");
            fflush(f);

            // 尝试初始化D3D11Hooks
            fprintf(f, "\nStep 2: Calling D3D11Hooks::Initialize()...\n");
            fflush(f);

            bool hookResult = D3D11Hooks::GetInstance().Initialize();

            fprintf(f, "  D3D11Hooks::Initialize() returned: %s\n", hookResult ? "true" : "false");
            fflush(f);

            if (hookResult) {
                fprintf(f, "\nStep 3: D3D11Hooks initialization SUCCESS!\n");
                fflush(f);

                // 关闭Hooks
                fprintf(f, "\nStep 4: Calling D3D11Hooks::Shutdown()...\n");
                fflush(f);

                D3D11Hooks::GetInstance().Shutdown();

                fprintf(f, "  D3D11Hooks::Shutdown() completed\n");
                fflush(f);

                success = true;
            } else {
                fprintf(f, "\nStep 3: D3D11Hooks initialization FAILED!\n");
                fflush(f);
            }

            // 反初始化MinHook
            fprintf(f, "\nStep 5: Uninitializing MinHook...\n");
            fflush(f);

            MH_Uninitialize();

            fprintf(f, "  MinHook uninitialized\n");
            fflush(f);

        } catch (const std::exception& e) {
            fprintf(f, "\nEXCEPTION caught: %s\n", e.what());
            fflush(f);
        } catch (...) {
            fprintf(f, "\nUNKNOWN EXCEPTION caught\n");
            fflush(f);
        }

        fclose(f);

        if (success) {
            MessageBoxA(NULL,
                "Level 6 PASSED!\n\nD3D11Hooks initialized successfully!\n\nThis means the original crash is NOT in initialization,\nbut might be triggered later during actual D3D11 calls.\n\nCheck logs/test_debug.log",
                "Debug Level 6 - SUCCESS!", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL,
                "Level 6 FAILED!\n\nD3D11Hooks initialization failed or threw exception.\nCheck logs/test_debug.log for details.",
                "Debug Level 6 - FAILED", MB_OK | MB_ICONERROR);
        }
    } else {
        MessageBoxA(NULL, "Level 6 FAILED!\n\nCannot create debug log file.", "Debug Level 6", MB_OK | MB_ICONERROR);
    }
#elif DEBUG_LEVEL == 7
    // Test FULL Initialize() - exactly like the original!
    // 完全模拟原版Initialize()的流程

    std::string dllDir = GetDllDirectoryPath();
    std::string configPath = dllDir + "\\config\\config.ini";
    std::string logPath = dllDir + "\\logs\\dmitri_compat.log";

    FILE* debugFile = fopen((dllDir + "\\logs\\test_debug.log").c_str(), "w");
    if (!debugFile) {
        MessageBoxA(NULL, "Level 7 FAILED!\n\nCannot create debug log.", "Debug Level 7", MB_OK | MB_ICONERROR);
        return 0;
    }

    fprintf(debugFile, "DEBUG Level 7: Full Initialize() simulation\n");
    fprintf(debugFile, "Simulating EXACT original Initialize() flow\n\n");
    fflush(debugFile);

    bool success = false;
    try {
        // Step 1: Load Config
        fprintf(debugFile, "Step 1: Loading Config...\n");
        fflush(debugFile);

        Config& config = Config::GetInstance();
        if (!config.Load(configPath)) {
            fprintf(debugFile, "  Config file not found, using defaults\n");
        } else {
            fprintf(debugFile, "  Config loaded successfully\n");
        }
        fflush(debugFile);

        // Step 2: Initialize Logger
        fprintf(debugFile, "\nStep 2: Initializing Logger...\n");
        fflush(debugFile);

        LogLevel logLevel = static_cast<LogLevel>(config.GetLogLevel());
        Logger::GetInstance().Initialize(logPath, logLevel);

        fprintf(debugFile, "  Logger initialized\n");
        fflush(debugFile);

        // Step 3: Write ALL the LOG_INFO calls like original
        fprintf(debugFile, "\nStep 3: Writing log messages (like original)...\n");
        fflush(debugFile);

        LOG_INFO("╔════════════════════════════════════════════════════════════════╗");
        LOG_INFO("║          DmitriCompat - RTX 50 Compatibility Layer            ║");
        LOG_INFO("║                    Version 0.1.0 (MVP)                         ║");
        LOG_INFO("╚════════════════════════════════════════════════════════════════╝");
        LOG_INFO("");
        LOG_INFO("DLL Directory: %s", dllDir.c_str());
        LOG_INFO("Config Path: %s", configPath.c_str());
        LOG_INFO("Log Path: %s", logPath.c_str());
        LOG_INFO("");

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

        fprintf(debugFile, "  All LOG_INFO calls completed\n");
        fflush(debugFile);

        // Step 4: Initialize D3D11 Hooks
        fprintf(debugFile, "\nStep 4: Initializing D3D11Hooks...\n");
        fflush(debugFile);

        if (!D3D11Hooks::GetInstance().Initialize()) {
            fprintf(debugFile, "  D3D11Hooks initialization FAILED!\n");
            fflush(debugFile);
            LOG_ERROR("Failed to initialize D3D11 hooks!");
            fclose(debugFile);
            MessageBoxA(NULL, "Level 7 FAILED!\n\nD3D11Hooks init failed.", "Debug Level 7", MB_OK | MB_ICONERROR);
            return 0;
        }

        fprintf(debugFile, "  D3D11Hooks initialized successfully\n");
        fflush(debugFile);

        LOG_INFO("✓ DmitriCompat initialized successfully");
        LOG_INFO("✓ Waiting for DmitriRender to call D3D11 APIs...\n");
        Logger::GetInstance().Flush();

        fprintf(debugFile, "\nStep 5: Cleanup...\n");
        fflush(debugFile);

        // Cleanup
        D3D11Hooks::GetInstance().Shutdown();
        Logger::GetInstance().Shutdown();

        fprintf(debugFile, "  All cleanup completed\n");
        fflush(debugFile);

        success = true;

    } catch (const std::exception& e) {
        fprintf(debugFile, "\nEXCEPTION caught: %s\n", e.what());
        fflush(debugFile);
    } catch (...) {
        fprintf(debugFile, "\nUNKNOWN EXCEPTION caught\n");
        fflush(debugFile);
    }

    fclose(debugFile);

    if (success) {
        MessageBoxA(NULL,
            "Level 7 PASSED!\n\nFull Initialize() completed successfully!\n\nIf this works but original crashes, the problem is in\nDllMain restrictions or timing issues.\n\nCheck logs/test_debug.log and logs/dmitri_compat.log",
            "Debug Level 7 - SUCCESS!", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(NULL,
            "Level 7 FAILED!\n\nFull Initialize() threw exception.\nCheck logs/test_debug.log for details.",
            "Debug Level 7 - FAILED", MB_OK | MB_ICONERROR);
    }
#elif DEBUG_LEVEL == 8
    // PRODUCTION VERSION - Keep hooks active!
    // 生产版本 - 保持Hooks激活，捕获实际的D3D11调用

    std::string dllDir = GetDllDirectoryPath();
    std::string configPath = dllDir + "\\config\\config.ini";
    std::string logPath = dllDir + "\\logs\\dmitri_compat.log";

    try {
        // Load Config
        Config& config = Config::GetInstance();
        config.Load(configPath);

        // Initialize Logger
        LogLevel logLevel = static_cast<LogLevel>(config.GetLogLevel());
        Logger::GetInstance().Initialize(logPath, logLevel);

        // Log startup banner
        LOG_INFO("╔════════════════════════════════════════════════════════════════╗");
        LOG_INFO("║          DmitriCompat - RTX 50 Compatibility Layer            ║");
        LOG_INFO("║                    Version 0.1.0 (MVP)                         ║");
        LOG_INFO("╚════════════════════════════════════════════════════════════════╝");
        LOG_INFO("");
        LOG_INFO("DLL Directory: %s", dllDir.c_str());
        LOG_INFO("Config Path: %s", configPath.c_str());
        LOG_INFO("Log Path: %s", logPath.c_str());
        LOG_INFO("");

        // Display configuration
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

        // Initialize D3D11 Hooks
        if (!D3D11Hooks::GetInstance().Initialize()) {
            LOG_ERROR("Failed to initialize D3D11 hooks!");
            return 0;
        }

        LOG_INFO("✓ DmitriCompat initialized successfully");
        LOG_INFO("✓ Waiting for DmitriRender to call D3D11 APIs...");
        LOG_INFO("✓ Hooks are now ACTIVE and monitoring D3D11 calls");
        LOG_INFO("");
        Logger::GetInstance().Flush();

        // NOTE: No cleanup here! Hooks stay active until DLL_PROCESS_DETACH

    } catch (const std::exception& e) {
        // Fallback error handling
        MessageBoxA(NULL, e.what(), "DmitriCompat Error", MB_OK | MB_ICONERROR);
    } catch (...) {
        MessageBoxA(NULL, "Unknown error during initialization", "DmitriCompat Error", MB_OK | MB_ICONERROR);
    }
#endif

    return 0;
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(NULL, 0, InitializeThread, NULL, 0, NULL);
            break;

        case DLL_PROCESS_DETACH:
#if DEBUG_LEVEL == 8
            // Production cleanup
            try {
                LOG_INFO("");
                LOG_INFO("╔════════════════════════════════════════════════════════════════╗");
                LOG_INFO("║                  DmitriCompat Shutting Down                   ║");
                LOG_INFO("╚════════════════════════════════════════════════════════════════╝");
                LOG_INFO("");

                D3D11Hooks::GetInstance().Shutdown();
                Logger::GetInstance().Shutdown();
            } catch (...) {
                // Ignore cleanup errors during detach
            }
#endif
            break;
    }
    return TRUE;
}

