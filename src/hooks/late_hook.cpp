/**
 * late_hook.cpp - 后期注入 Hook 策略
 * 
 * 问题：DmitriRender 在我们注入之前就已经创建了 D3D11 设备
 * 解决方案：
 *   1. 创建一个临时的 D3D11 设备来获取 VTable
 *   2. 使用这个 VTable 来 Hook 全局的 D3D11 方法
 *   3. 这样无论设备何时创建，我们的 Hook 都能生效
 *   
 * 这是一种"trampoline"技术，通过 Hook 共享的 COM 接口 VTable
 */

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <cstdio>
#include "../external/minhook/include/MinHook.h"
#include "../include/logger.h"
#include "../include/config.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// 外部声明：KeyedMutex Hook 函数
namespace DmitriCompat {
    extern void InitKeyedMutexHook();
    extern void RegisterTextureForFakeKeyedMutex(ID3D11Texture2D* pTexture);
    extern IDXGIKeyedMutex* GetFakeKeyedMutex(ID3D11Texture2D* pTexture);
}

namespace DmitriCompat {

// ============================================================================
// 全局变量
// ============================================================================

// 原始函数指针
static HRESULT(STDMETHODCALLTYPE* g_OriginalCreateTexture2D)(
    ID3D11Device* This,
    const D3D11_TEXTURE2D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture2D** ppTexture2D
) = nullptr;

static HRESULT(STDMETHODCALLTYPE* g_OriginalPresent)(
    IDXGISwapChain* This,
    UINT SyncInterval,
    UINT Flags
) = nullptr;

static void(STDMETHODCALLTYPE* g_OriginalDraw)(
    ID3D11DeviceContext* This,
    UINT VertexCount,
    UINT StartVertexLocation
) = nullptr;

static void(STDMETHODCALLTYPE* g_OriginalDrawIndexed)(
    ID3D11DeviceContext* This,
    UINT IndexCount,
    UINT StartIndexLocation,
    INT BaseVertexLocation
) = nullptr;

static HRESULT(STDMETHODCALLTYPE* g_OriginalMap)(
    ID3D11DeviceContext* This,
    ID3D11Resource* pResource,
    UINT Subresource,
    D3D11_MAP MapType,
    UINT MapFlags,
    D3D11_MAPPED_SUBRESOURCE* pMappedResource
) = nullptr;

// ============================================================================
// 辅助函数
// ============================================================================

static const char* GetFormatName(DXGI_FORMAT format) {
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

// ============================================================================
// Hook 函数实现
// ============================================================================

HRESULT STDMETHODCALLTYPE Hook_CreateTexture2D_Late(
    ID3D11Device* This,
    const D3D11_TEXTURE2D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture2D** ppTexture2D
) {
    static int textureCount = 0;
    textureCount++;
    
    // =========================================================================
    // RTX 50 系列兼容性修复 - 只记录日志，不修改纹理
    // =========================================================================
    // 注意：之前的 v3 workaround (0x2 + FakeKeyedMutex) 会导致 NvPresent64.dll 崩溃！
    // 现在我们只记录信息，让 CUDA Hook + Compute Shader 方案处理颜色转换。
    // =========================================================================
    
    if (pDesc) {
        // 检查是否是视频格式
        bool isVideoFormat = (
            pDesc->Format == DXGI_FORMAT_NV12 ||
            pDesc->Format == DXGI_FORMAT_P010 ||
            pDesc->Format == DXGI_FORMAT_YUY2 ||
            pDesc->Format == DXGI_FORMAT_AYUV ||
            pDesc->Format == DXGI_FORMAT_420_OPAQUE
        );
        
        // 始终记录前 50 个纹理和所有视频格式纹理
        bool shouldLog = textureCount <= 50 || isVideoFormat || pDesc->MiscFlags == 0x900;
        if (shouldLog) {
            LOG_INFO("🎨 Texture #%d: %ux%u, Format=%s, Usage=%d, Bind=0x%X, Misc=0x%X%s", 
                textureCount,
                pDesc->Width, pDesc->Height, 
                GetFormatName(pDesc->Format),
                pDesc->Usage,
                pDesc->BindFlags,
                pDesc->MiscFlags,
                isVideoFormat ? " [VIDEO]" : "");
        }
        
        // 每 100 个纹理记录一次统计
        if (textureCount % 100 == 0) {
            LOG_INFO("📈 Total textures created so far: %d", textureCount);
            Logger::GetInstance().Flush();
        }
    }
    
    // 直接调用原始函数，不做任何修改
    HRESULT hr = g_OriginalCreateTexture2D(This, pDesc, pInitialData, ppTexture2D);
    
    // 记录失败情况（仅用于诊断）
    if (FAILED(hr) && pDesc) {
        LOG_ERROR("❌ CreateTexture2D FAILED! HRESULT=0x%08X, Size=%ux%u, Format=%s, Misc=0x%X", 
            hr, pDesc->Width, pDesc->Height, GetFormatName(pDesc->Format), pDesc->MiscFlags);
        
        // 如果是 0x900 失败，记录提示
        if (pDesc->MiscFlags == 0x900) {
            LOG_INFO("   💡 [RTX 50] 0x900 纹理失败是预期行为，CUDA Hook 会处理颜色转换");
        }
        Logger::GetInstance().Flush();
    }
    
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_Present_Late(
    IDXGISwapChain* This,
    UINT SyncInterval,
    UINT Flags
) {
    static int frameCount = 0;
    frameCount++;
    
    // 每 100 帧记录一次
    if (frameCount % 100 == 0) {
        LOG_INFO("📊 Frame %d presented (SyncInterval=%u, Flags=0x%X)", 
            frameCount, SyncInterval, Flags);
        Logger::GetInstance().Flush();
    }
    
    return g_OriginalPresent(This, SyncInterval, Flags);
}

void STDMETHODCALLTYPE Hook_Draw_Late(
    ID3D11DeviceContext* This,
    UINT VertexCount,
    UINT StartVertexLocation
) {
    static int drawCount = 0;
    drawCount++;
    
    // 只记录前几次和每 1000 次
    if (drawCount <= 5 || drawCount % 1000 == 0) {
        LOG_VERBOSE("Draw called (#%d): VertexCount=%u, Start=%u", 
            drawCount, VertexCount, StartVertexLocation);
    }
    
    g_OriginalDraw(This, VertexCount, StartVertexLocation);
}

void STDMETHODCALLTYPE Hook_DrawIndexed_Late(
    ID3D11DeviceContext* This,
    UINT IndexCount,
    UINT StartIndexLocation,
    INT BaseVertexLocation
) {
    static int drawCount = 0;
    drawCount++;
    
    if (drawCount <= 5 || drawCount % 1000 == 0) {
        LOG_VERBOSE("DrawIndexed called (#%d): IndexCount=%u", 
            drawCount, IndexCount);
    }
    
    g_OriginalDrawIndexed(This, IndexCount, StartIndexLocation, BaseVertexLocation);
}

HRESULT STDMETHODCALLTYPE Hook_Map_Late(
    ID3D11DeviceContext* This,
    ID3D11Resource* pResource,
    UINT Subresource,
    D3D11_MAP MapType,
    UINT MapFlags,
    D3D11_MAPPED_SUBRESOURCE* pMappedResource
) {
    static int mapCount = 0;
    mapCount++;
    
    // Map 调用可能很频繁，只记录前几次
    if (mapCount <= 10) {
        LOG_INFO("📝 Map called (#%d): Resource=%p, MapType=%d", 
            mapCount, pResource, MapType);
    }
    
    return g_OriginalMap(This, pResource, Subresource, MapType, MapFlags, pMappedResource);
}

// ============================================================================
// 后期 Hook 初始化
// ============================================================================

class LateHook {
public:
    static LateHook& GetInstance() {
        static LateHook instance;
        return instance;
    }
    
    bool Initialize() {
        if (initialized_) return true;
        
        LOG_INFO("=== Late Hook Initialization ===");
        LOG_INFO("Strategy: Create dummy D3D11 device to get VTable addresses");
        
        // 初始化 MinHook
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            LOG_ERROR("MinHook initialization failed: %d", status);
            return false;
        }
        
        // 创建临时设备获取 VTable
        if (!CreateDummyDeviceAndHook()) {
            LOG_ERROR("Failed to create dummy device or install hooks");
            return false;
        }
        
        initialized_ = true;
        LOG_INFO("✓ Late Hook initialization successful!");
        LOG_INFO("✓ Now monitoring ALL D3D11 calls in this process");
        LOG_INFO("================================\n");
        Logger::GetInstance().Flush();
        
        return true;
    }
    
    void Shutdown() {
        if (!initialized_) return;
        
        LOG_INFO("Late Hook shutting down...");
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        initialized_ = false;
    }
    
private:
    bool initialized_ = false;
    
    bool CreateDummyDeviceAndHook() {
        // 创建一个隐藏窗口用于 SwapChain
        WNDCLASSEXA wc = {sizeof(WNDCLASSEXA), CS_CLASSDC, DefWindowProcA, 0, 0,
                          GetModuleHandleA(NULL), NULL, NULL, NULL, NULL,
                          "DmitriCompat_Dummy", NULL};
        RegisterClassExA(&wc);
        
        HWND hwnd = CreateWindowExA(
            0, wc.lpszClassName, "DmitriCompat Dummy",
            WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
            NULL, NULL, wc.hInstance, NULL
        );
        
        if (!hwnd) {
            LOG_ERROR("Failed to create dummy window");
            return false;
        }
        
        // 设置 SwapChain 描述
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = 100;
        sd.BufferDesc.Height = 100;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        
        D3D_FEATURE_LEVEL featureLevel;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapChain = nullptr;
        
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,                    // Adapter
            D3D_DRIVER_TYPE_HARDWARE,   // Driver type
            nullptr,                    // Software
            0,                          // Flags
            nullptr,                    // Feature levels
            0,                          // Num feature levels
            D3D11_SDK_VERSION,
            &sd,
            &swapChain,
            &device,
            &featureLevel,
            &context
        );
        
        if (FAILED(hr)) {
            LOG_ERROR("Failed to create dummy D3D11 device: 0x%08X", hr);
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }
        
        LOG_INFO("Created dummy D3D11 device (Feature Level: 0x%X)", featureLevel);
        
        // 获取 VTable 地址
        void** deviceVTable = *(void***)device;
        void** contextVTable = *(void***)context;
        void** swapChainVTable = *(void***)swapChain;
        
        LOG_INFO("VTable addresses obtained:");
        LOG_INFO("  Device VTable: %p", deviceVTable);
        LOG_INFO("  Context VTable: %p", contextVTable);
        LOG_INFO("  SwapChain VTable: %p", swapChainVTable);
        
        // Hook CreateTexture2D (Device vtable index 5)
        void* createTexture2DAddr = deviceVTable[5];
        if (InstallHook(createTexture2DAddr, reinterpret_cast<void*>(&Hook_CreateTexture2D_Late), 
                        (void**)&g_OriginalCreateTexture2D, "CreateTexture2D")) {
            LOG_INFO("✓ CreateTexture2D hooked at %p", createTexture2DAddr);
        }
        
        // Hook Present (SwapChain vtable index 8)
        void* presentAddr = swapChainVTable[8];
        if (InstallHook(presentAddr, reinterpret_cast<void*>(&Hook_Present_Late),
                        (void**)&g_OriginalPresent, "Present")) {
            LOG_INFO("✓ Present hooked at %p", presentAddr);
        }
        
        // Hook Draw (Context vtable index 13)
        void* drawAddr = contextVTable[13];
        if (InstallHook(drawAddr, reinterpret_cast<void*>(&Hook_Draw_Late),
                        (void**)&g_OriginalDraw, "Draw")) {
            LOG_INFO("✓ Draw hooked at %p", drawAddr);
        }
        
        // Hook DrawIndexed (Context vtable index 12)
        void* drawIndexedAddr = contextVTable[12];
        if (InstallHook(drawIndexedAddr, reinterpret_cast<void*>(&Hook_DrawIndexed_Late),
                        (void**)&g_OriginalDrawIndexed, "DrawIndexed")) {
            LOG_INFO("✓ DrawIndexed hooked at %p", drawIndexedAddr);
        }
        
        // Hook Map (Context vtable index 14)
        void* mapAddr = contextVTable[14];
        if (InstallHook(mapAddr, reinterpret_cast<void*>(&Hook_Map_Late),
                        (void**)&g_OriginalMap, "Map")) {
            LOG_INFO("✓ Map hooked at %p", mapAddr);
        }
        
        // 释放临时资源（Hook 已经安装，不再需要这些对象）
        swapChain->Release();
        context->Release();
        device->Release();
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        
        return true;
    }
    
    bool InstallHook(void* target, void* detour, void** original, const char* name) {
        MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status != MH_OK) {
            LOG_ERROR("Failed to create %s hook: %d", name, status);
            return false;
        }
        
        status = MH_EnableHook(target);
        if (status != MH_OK) {
            LOG_ERROR("Failed to enable %s hook: %d", name, status);
            return false;
        }
        
        return true;
    }
};

} // namespace DmitriCompat

// ============================================================================
// 导出函数供 main.cpp 调用
// ============================================================================

extern "C" {
    bool InitializeLateHooks() {
        return DmitriCompat::LateHook::GetInstance().Initialize();
    }
    
    void ShutdownLateHooks() {
        DmitriCompat::LateHook::GetInstance().Shutdown();
    }
}
