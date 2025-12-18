#include "d3d11_hooks.h"
#include "logger.h"
#include "config.h"
#include "../external/minhook/include/MinHook.h"
#include <sstream>

namespace DmitriCompat {

// 原始函数指针
static PFN_D3D11CreateDevice g_OriginalD3D11CreateDevice = nullptr;

// 设备方法原始指针
typedef HRESULT(STDMETHODCALLTYPE* PFN_CreateTexture2D)(
    ID3D11Device* This,
    const D3D11_TEXTURE2D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture2D** ppTexture2D
);
static PFN_CreateTexture2D g_OriginalCreateTexture2D = nullptr;

// SwapChain 原始指针
typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(
    IDXGISwapChain* This,
    UINT SyncInterval,
    UINT Flags
);
static PFN_Present g_OriginalPresent = nullptr;

// 辅助函数：获取 DXGI_FORMAT 名称
const char* GetFormatName(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_UNKNOWN: return "UNKNOWN";
        case DXGI_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        case DXGI_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case DXGI_FORMAT_R10G10B10A2_UNORM: return "R10G10B10A2_UNORM";
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
        case DXGI_FORMAT_NV12: return "NV12 (Video)";
        case DXGI_FORMAT_P010: return "P010 (Video)";
        case DXGI_FORMAT_YUY2: return "YUY2 (Video)";
        case DXGI_FORMAT_AYUV: return "AYUV (Video)";
        case DXGI_FORMAT_420_OPAQUE: return "420_OPAQUE (Video)";
        default: {
            static char buffer[32];
            snprintf(buffer, sizeof(buffer), "Format_%d", (int)format);
            return buffer;
        }
    }
}

// 辅助函数：获取特性级别名称
const char* GetFeatureLevelName(D3D_FEATURE_LEVEL level) {
    switch (level) {
        case D3D_FEATURE_LEVEL_9_1: return "9_1";
        case D3D_FEATURE_LEVEL_9_2: return "9_2";
        case D3D_FEATURE_LEVEL_9_3: return "9_3";
        case D3D_FEATURE_LEVEL_10_0: return "10_0";
        case D3D_FEATURE_LEVEL_10_1: return "10_1";
        case D3D_FEATURE_LEVEL_11_0: return "11_0";
        case D3D_FEATURE_LEVEL_11_1: return "11_1";
        case D3D_FEATURE_LEVEL_12_0: return "12_0";
        case D3D_FEATURE_LEVEL_12_1: return "12_1";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// D3D11CreateDevice Hook
// ============================================================================

HRESULT WINAPI Hook_D3D11CreateDevice(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
) {
    LOG_INFO("=== D3D11CreateDevice Called ===");
    LOG_INFO("  Adapter: %p", pAdapter);
    LOG_INFO("  DriverType: %d (0=UNKNOWN, 1=HARDWARE, 2=REFERENCE, 3=NULL, 4=SOFTWARE, 5=WARP)", DriverType);
    LOG_INFO("  Flags: 0x%X", Flags);
    LOG_INFO("  SDKVersion: %u", SDKVersion);

    // 记录请求的特性级别
    if (pFeatureLevels && FeatureLevels > 0) {
        LOG_INFO("  Requested Feature Levels (%u):", FeatureLevels);
        for (UINT i = 0; i < FeatureLevels; i++) {
            LOG_INFO("    [%u] %s", i, GetFeatureLevelName(pFeatureLevels[i]));
        }
    }

    // 调用原始函数
    HRESULT hr = g_OriginalD3D11CreateDevice(
        pAdapter, DriverType, Software, Flags,
        pFeatureLevels, FeatureLevels, SDKVersion,
        ppDevice, pFeatureLevel, ppImmediateContext
    );

    if (SUCCEEDED(hr)) {
        LOG_INFO("  ✓ Device Created Successfully");
        if (pFeatureLevel) {
            LOG_INFO("  Selected Feature Level: %s", GetFeatureLevelName(*pFeatureLevel));
        }

        // Hook 设备方法
        if (ppDevice && *ppDevice) {
            ID3D11DeviceHook::HookDevice(*ppDevice);
        }
    } else {
        LOG_ERROR("  ✗ Device Creation Failed: HRESULT = 0x%08X", hr);
    }

    LOG_INFO("=================================\n");
    Logger::GetInstance().Flush();

    return hr;
}

// ============================================================================
// CreateTexture2D Hook
// ============================================================================

HRESULT STDMETHODCALLTYPE ID3D11DeviceHook::Hook_CreateTexture2D(
    ID3D11Device* This,
    const D3D11_TEXTURE2D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture2D** ppTexture2D
) {
    if (pDesc) {
        LOG_VERBOSE("CreateTexture2D: %ux%u, Format=%s, MipLevels=%u, Usage=%d, BindFlags=0x%X",
            pDesc->Width, pDesc->Height,
            GetFormatName(pDesc->Format),
            pDesc->MipLevels,
            pDesc->Usage,
            pDesc->BindFlags
        );

        // 检查视频格式
        bool isVideoFormat = (pDesc->Format == DXGI_FORMAT_NV12 ||
                               pDesc->Format == DXGI_FORMAT_P010 ||
                               pDesc->Format == DXGI_FORMAT_YUY2 ||
                               pDesc->Format == DXGI_FORMAT_AYUV ||
                               pDesc->Format == DXGI_FORMAT_420_OPAQUE);

        if (isVideoFormat) {
            LOG_INFO("⚠️  Video format texture detected: %s", GetFormatName(pDesc->Format));
        }
    }

    // 调用原始函数
    HRESULT hr = g_OriginalCreateTexture2D(This, pDesc, pInitialData, ppTexture2D);

    if (FAILED(hr)) {
        LOG_ERROR("CreateTexture2D failed: HRESULT = 0x%08X", hr);
        if (pDesc) {
            LOG_ERROR("  Format was: %s", GetFormatName(pDesc->Format));
        }
    }

    return hr;
}

void ID3D11DeviceHook::HookDevice(ID3D11Device* device) {
    if (!device) return;

    // 获取 vtable
    void** vtable = *(void***)device;

    // CreateTexture2D 是 vtable 的第 5 个方法
    // (0=QueryInterface, 1=AddRef, 2=Release, 3=GetDevice, 4=GetPrivateData, 5=CreateTexture2D)
    void* createTexture2DFunc = vtable[5];

    if (g_OriginalCreateTexture2D == nullptr) {
        MH_STATUS status = MH_CreateHook(
            createTexture2DFunc,
            reinterpret_cast<LPVOID>(&Hook_CreateTexture2D),
            reinterpret_cast<void**>(&g_OriginalCreateTexture2D)
        );

        if (status == MH_OK) {
            status = MH_EnableHook(createTexture2DFunc);
            if (status == MH_OK) {
                LOG_INFO("✓ CreateTexture2D hooked successfully");
            } else {
                LOG_ERROR("Failed to enable CreateTexture2D hook: %d", status);
            }
        } else {
            LOG_ERROR("Failed to create CreateTexture2D hook: %d", status);
        }
    }
}

// ============================================================================
// Present Hook
// ============================================================================

HRESULT STDMETHODCALLTYPE IDXGISwapChainHook::Hook_Present(
    IDXGISwapChain* This,
    UINT SyncInterval,
    UINT Flags
) {
    static int frameCount = 0;
    frameCount++;

    // 每 60 帧记录一次（避免日志过多）
    if (frameCount % 60 == 0) {
        LOG_VERBOSE("Present called (Frame %d): SyncInterval=%u, Flags=0x%X",
            frameCount, SyncInterval, Flags);
    }

    // 这里可以添加颜色空间修复或同步逻辑
    // if (Config::GetInstance().IsColorSpaceCorrectionEnabled()) {
    //     // TODO: 添加颜色校正
    // }

    HRESULT hr = g_OriginalPresent(This, SyncInterval, Flags);

    if (FAILED(hr) && frameCount % 60 == 0) {
        LOG_ERROR("Present failed: HRESULT = 0x%08X", hr);
    }

    return hr;
}

void IDXGISwapChainHook::HookSwapChain(IDXGISwapChain* swapChain) {
    if (!swapChain) return;

    // 获取 vtable
    void** vtable = *(void***)swapChain;

    // Present 是 IDXGISwapChain vtable 的第 8 个方法
    void* presentFunc = vtable[8];

    if (g_OriginalPresent == nullptr) {
        MH_STATUS status = MH_CreateHook(
            presentFunc,
            reinterpret_cast<LPVOID>(&Hook_Present),
            reinterpret_cast<void**>(&g_OriginalPresent)
        );

        if (status == MH_OK) {
            status = MH_EnableHook(presentFunc);
            if (status == MH_OK) {
                LOG_INFO("✓ Present hooked successfully");
            } else {
                LOG_ERROR("Failed to enable Present hook: %d", status);
            }
        } else {
            LOG_ERROR("Failed to create Present hook: %d", status);
        }
    }
}

// ============================================================================
// D3D11Hooks 管理器
// ============================================================================

D3D11Hooks& D3D11Hooks::GetInstance() {
    static D3D11Hooks instance;
    return instance;
}

bool D3D11Hooks::Initialize() {
    if (initialized_) {
        return true;
    }

    LOG_INFO("Initializing D3D11 Hooks...");

    // 初始化 MinHook
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        LOG_ERROR("MinHook initialization failed: %d", status);
        return false;
    }

    // Hook D3D11 函数
    if (!HookD3D11Functions()) {
        LOG_ERROR("Failed to hook D3D11 functions");
        return false;
    }

    initialized_ = true;
    LOG_INFO("D3D11 Hooks initialized successfully\n");
    return true;
}

void D3D11Hooks::Shutdown() {
    if (!initialized_) {
        return;
    }

    LOG_INFO("Shutting down D3D11 Hooks...");

    // 禁用所有 Hook
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    initialized_ = false;
    LOG_INFO("D3D11 Hooks shut down\n");
}

bool D3D11Hooks::HookD3D11Functions() {
    // 获取 d3d11.dll
    HMODULE d3d11Module = GetModuleHandleA("d3d11.dll");
    if (!d3d11Module) {
        d3d11Module = LoadLibraryA("d3d11.dll");
        if (!d3d11Module) {
            LOG_ERROR("Failed to load d3d11.dll");
            return false;
        }
    }

    // 获取 D3D11CreateDevice 函数地址
    void* d3d11CreateDeviceAddr = reinterpret_cast<void*>(GetProcAddress(d3d11Module, "D3D11CreateDevice"));
    if (!d3d11CreateDeviceAddr) {
        LOG_ERROR("Failed to find D3D11CreateDevice");
        return false;
    }

    // Hook D3D11CreateDevice
    MH_STATUS status = MH_CreateHook(
        d3d11CreateDeviceAddr,
        reinterpret_cast<LPVOID>(&Hook_D3D11CreateDevice),
        reinterpret_cast<void**>(&g_OriginalD3D11CreateDevice)
    );

    if (status != MH_OK) {
        LOG_ERROR("Failed to create D3D11CreateDevice hook: %d", status);
        return false;
    }

    status = MH_EnableHook(d3d11CreateDeviceAddr);
    if (status != MH_OK) {
        LOG_ERROR("Failed to enable D3D11CreateDevice hook: %d", status);
        return false;
    }

    LOG_INFO("✓ D3D11CreateDevice hooked at %p", d3d11CreateDeviceAddr);
    return true;
}

bool D3D11Hooks::HookDXGIFunctions() {
    // DXGI hook 目前通过 SwapChain 对象的 vtable 实现
    // 在创建设备后会自动 hook
    return true;
}

} // namespace DmitriCompat
