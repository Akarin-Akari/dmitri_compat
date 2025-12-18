/**
 * main_late_hook.cpp - 使用后期 Hook 策略的主入口
 * 
 * 这个版本使用 "Late Hook" 技术，可以 Hook 已经创建的 D3D11 设备
 * 解决了注入时机问题：即使 DmitriRender 已经初始化完成，我们也能捕获调用
 */

#include <windows.h>
#include <string>
#include <cstdio>
#include <ctime>
#include "../include/logger.h"
#include "../include/config.h"

using namespace DmitriCompat;

// 外部函数声明（来自 late_hook.cpp）
extern "C" {
    bool InitializeLateHooks();
    void ShutdownLateHooks();
}

// 外部函数声明（来自 video_processor_hook.cpp）
extern "C" {
    bool InitializeVideoProcessorHooks();
    void ShutdownVideoProcessorHooks();
}

// 外部函数声明（来自 cuda_hook.cpp）
extern "C" {
    bool InitializeCudaHooks();
    void ShutdownCudaHooks();
}

// 外部函数声明（来自 keyed_mutex_hook.cpp）
namespace DmitriCompat {
    extern void InitKeyedMutexHook();
    extern void CleanupKeyedMutexHook();
}

// 获取 DLL 所在目录
std::string GetDllDirectoryPath() {
    char path[MAX_PATH] = {0};
    HMODULE hm = NULL;

    // 直接使用当前函数的地址来获取模块句柄
    if (GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&GetDllDirectoryPath,
        &hm)) {

        GetModuleFileNameA(hm, path, sizeof(path));
        std::string fullPath(path);
        size_t pos = fullPath.find_last_of("\\/");
        if (pos != std::string::npos) {
            return fullPath.substr(0, pos);
        }
    }
    
    // 如果失败，尝试使用 DLL 名称直接获取
    hm = GetModuleHandleA("libdmitri_late_hook.dll");
    if (hm) {
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
    // ============ DEBUG: 写入调试文件验证初始化过程 ============
    {
        FILE* debugFile = fopen("C:\\Users\\Akari\\AppData\\Roaming\\DmitriRender\\DLL_DEBUG.txt", "a");
        if (debugFile) {
            fprintf(debugFile, "[%lld] Initialize() called\n", (long long)time(NULL));
            fflush(debugFile);
            fclose(debugFile);
        }
    }
    // ============ END DEBUG ============
    
    std::string dllDir = GetDllDirectoryPath();
    std::string configPath = dllDir + "\\config\\config.ini";
    std::string logPath = dllDir + "\\logs\\dmitri_compat.log";

    // ============ DEBUG: 写入路径信息 ============
    {
        FILE* debugFile = fopen("C:\\Users\\Akari\\AppData\\Roaming\\DmitriRender\\DLL_DEBUG.txt", "a");
        if (debugFile) {
            fprintf(debugFile, "[%lld] DLL Dir: %s\n", (long long)time(NULL), dllDir.c_str());
            fprintf(debugFile, "[%lld] Log Path: %s\n", (long long)time(NULL), logPath.c_str());
            fflush(debugFile);
            fclose(debugFile);
        }
    }
    // ============ END DEBUG ============

    // 确保日志目录存在
    CreateDirectoryA((dllDir + "\\logs").c_str(), NULL);

    try {
        // 加载配置
        Config& config = Config::GetInstance();
        config.Load(configPath);

        // 初始化日志
        LogLevel logLevel = static_cast<LogLevel>(config.GetLogLevel());
        Logger::GetInstance().Initialize(logPath, logLevel);

        // 启动横幅
        LOG_INFO("");
        LOG_INFO("╔════════════════════════════════════════════════════════════════╗");
        LOG_INFO("║    DmitriCompat - RTX 50 Compatibility Layer (Late Hook)       ║");
        LOG_INFO("║           Version 0.4.0 - CUDA & Video Processor Diag          ║");
        LOG_INFO("╚════════════════════════════════════════════════════════════════╝");
        LOG_INFO("");
        LOG_INFO("DLL Directory: %s", dllDir.c_str());
        LOG_INFO("Log Path: %s", logPath.c_str());
        LOG_INFO("");
        
        // 显示配置
        LOG_INFO("Configuration:");
        LOG_INFO("  LogLevel: %d", config.GetLogLevel());
        LOG_INFO("");

        // =====================================================================
        // 🚨 RTX 50 兼容性模式：只使用 CUDA Hook + Compute Shader
        // =====================================================================
        // 以下 Hook 在 RTX 50 + DmitriRender 组合下会导致 NvPresent64.dll 崩溃：
        // - KeyedMutex Hook (VTable patching)
        // - Late Hook (D3D11 Device/Context VTable patching)  
        // - Video Processor Hook
        // 
        // 我们只使用 CUDA Hook 来拦截失败的 kernel 并用 Compute Shader 替代
        // =====================================================================
        
        LOG_INFO("🔧 [RTX 50 Mode] Using CUDA-only hooks (D3D11 hooks disabled)");
        LOG_INFO("");

        // 初始化 CUDA Hook（核心！拦截 cuLaunchKernel 进行 Compute Shader 替代）
        LOG_INFO(">>> Initializing CUDA Hooks <<<");
        LOG_INFO("This will intercept cuLaunchKernel and replace with Compute Shader");
        LOG_INFO("");
        
        if (!InitializeCudaHooks()) {
            LOG_ERROR("❌ Failed to initialize CUDA Hooks!");
            return;
        }

        LOG_INFO("");
        LOG_INFO("✅ DmitriCompat v0.4.1 initialized (RTX 50 Mode)");
        LOG_INFO("✅ CUDA Hook active - will use Compute Shader for color conversion");
        LOG_INFO("✅ Play video to see CUDA kernel interception");
        LOG_INFO("");
        Logger::GetInstance().Flush();

    } catch (const std::exception& e) {
        // 备用错误处理
        FILE* f = fopen((dllDir + "\\logs\\error.log").c_str(), "w");
        if (f) {
            fprintf(f, "EXCEPTION: %s\n", e.what());
            fclose(f);
        }
    } catch (...) {
        // 未知异常
    }
}

// 清理函数
void Shutdown() {
    try {
        LOG_INFO("");
        LOG_INFO("╔════════════════════════════════════════════════════════════════╗");
        LOG_INFO("║              DmitriCompat Shutting Down                        ║");
        LOG_INFO("╚════════════════════════════════════════════════════════════════╝");
        LOG_INFO("");

        ShutdownLateHooks();
        Logger::GetInstance().Shutdown();
    } catch (...) {
        // 忽略清理错误
    }
}

// 延迟初始化线程
DWORD WINAPI InitializeThread(LPVOID lpParam) {
    (void)lpParam;
    
    // 等待进程稳定
    Sleep(1000);
    
    Initialize();
    
    return 0;
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            // ============ DEBUG: 验证 DllMain 被调用 ============
            {
                FILE* debugFile = fopen("C:\\Users\\Akari\\AppData\\Roaming\\DmitriRender\\DLLMAIN_DEBUG.txt", "a");
                if (debugFile) {
                    fprintf(debugFile, "DllMain ATTACH called! hModule=%p\n", (void*)hModule);
                    fflush(debugFile);
                    fclose(debugFile);
                }
            }
            // ============ END DEBUG ============
            
            DisableThreadLibraryCalls(hModule);
            CreateThread(NULL, 0, InitializeThread, NULL, 0, NULL);
            break;

        case DLL_PROCESS_DETACH:
            Shutdown();
            break;
    }
    return TRUE;
}
