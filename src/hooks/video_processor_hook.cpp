/**
 * video_processor_hook.cpp - Hook ID3D11VideoProcessor API
 * 
 * 用于诊断 RTX 50 系列绿屏问题
 * 拦截 D3D11 视频处理器的色彩转换调用
 */

#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <string>
#include <cstdio>
#include "../external/minhook/include/MinHook.h"
#include "../include/logger.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace DmitriCompat {

// ============================================================================
// 原始函数指针
// ============================================================================

// ID3D11VideoContext::VideoProcessorBlt
typedef HRESULT(STDMETHODCALLTYPE* PFN_VideoProcessorBlt)(
    ID3D11VideoContext* This,
    ID3D11VideoProcessor* pVideoProcessor,
    ID3D11VideoProcessorOutputView* pView,
    UINT OutputFrame,
    UINT StreamCount,
    const D3D11_VIDEO_PROCESSOR_STREAM* pStreams
);

// ID3D11VideoContext::VideoProcessorSetStreamColorSpace
typedef void(STDMETHODCALLTYPE* PFN_VideoProcessorSetStreamColorSpace)(
    ID3D11VideoContext* This,
    ID3D11VideoProcessor* pVideoProcessor,
    UINT StreamIndex,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace
);

// ID3D11VideoContext::VideoProcessorSetOutputColorSpace
typedef void(STDMETHODCALLTYPE* PFN_VideoProcessorSetOutputColorSpace)(
    ID3D11VideoContext* This,
    ID3D11VideoProcessor* pVideoProcessor,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace
);

static PFN_VideoProcessorBlt g_OriginalVideoProcessorBlt = nullptr;
static PFN_VideoProcessorSetStreamColorSpace g_OriginalSetStreamColorSpace = nullptr;
static PFN_VideoProcessorSetOutputColorSpace g_OriginalSetOutputColorSpace = nullptr;

// ============================================================================
// 辅助函数
// ============================================================================

static const char* GetColorSpaceDesc(const D3D11_VIDEO_PROCESSOR_COLOR_SPACE* cs) {
    static char buffer[256];
    if (!cs) return "NULL";
    
    snprintf(buffer, sizeof(buffer),
        "Usage=%u, RGB_Range=%u, YCbCr_Matrix=%u, YCbCr_xvYCC=%u, Nominal_Range=%u",
        cs->Usage,
        cs->RGB_Range,           // 0=Full(0-255), 1=Limited(16-235)
        cs->YCbCr_Matrix,        // 0=BT.601, 1=BT.709
        cs->YCbCr_xvYCC,         // 0=Conventional, 1=xvYCC
        cs->Nominal_Range        // 0=0-255, 1=16-235, 2=Auto
    );
    return buffer;
}

// ============================================================================
// Hook 函数
// ============================================================================

HRESULT STDMETHODCALLTYPE Hook_VideoProcessorBlt(
    ID3D11VideoContext* This,
    ID3D11VideoProcessor* pVideoProcessor,
    ID3D11VideoProcessorOutputView* pView,
    UINT OutputFrame,
    UINT StreamCount,
    const D3D11_VIDEO_PROCESSOR_STREAM* pStreams
) {
    static int bltCount = 0;
    bltCount++;
    
    // 记录前 100 次调用和每 500 次
    if (bltCount <= 100 || bltCount % 500 == 0) {
        LOG_INFO("🎬 VideoProcessorBlt #%d: Frame=%u, StreamCount=%u", 
            bltCount, OutputFrame, StreamCount);
        
        // 记录每个流的详细信息
        for (UINT i = 0; i < StreamCount && pStreams; i++) {
            const auto& stream = pStreams[i];
            LOG_INFO("  Stream[%u]: Enable=%d, SrcRect=(%d,%d,%d,%d), DstRect=(%d,%d,%d,%d)",
                i,
                stream.Enable,
                stream.pInputSurface ? 1 : 0,
                0, 0, 0, 0  // 简化，避免复杂指针访问
            );
        }
        
        if (bltCount <= 10) {
            Logger::GetInstance().Flush();
        }
    }
    
    HRESULT hr = g_OriginalVideoProcessorBlt(This, pVideoProcessor, pView, OutputFrame, StreamCount, pStreams);
    
    // 记录任何失败
    if (FAILED(hr)) {
        LOG_ERROR("❌ VideoProcessorBlt FAILED! HRESULT=0x%08X, Frame=%u", hr, OutputFrame);
        Logger::GetInstance().Flush();
    }
    
    return hr;
}

void STDMETHODCALLTYPE Hook_VideoProcessorSetStreamColorSpace(
    ID3D11VideoContext* This,
    ID3D11VideoProcessor* pVideoProcessor,
    UINT StreamIndex,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace
) {
    static int callCount = 0;
    callCount++;
    
    // 记录所有调用（这个不会很频繁）
    LOG_INFO("🎨 SetStreamColorSpace #%d: StreamIndex=%u, %s",
        callCount, StreamIndex, GetColorSpaceDesc(pColorSpace));
    
    if (callCount <= 5) {
        Logger::GetInstance().Flush();
    }
    
    g_OriginalSetStreamColorSpace(This, pVideoProcessor, StreamIndex, pColorSpace);
}

void STDMETHODCALLTYPE Hook_VideoProcessorSetOutputColorSpace(
    ID3D11VideoContext* This,
    ID3D11VideoProcessor* pVideoProcessor,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace
) {
    static int callCount = 0;
    callCount++;
    
    // 记录所有调用
    LOG_INFO("🖥️ SetOutputColorSpace #%d: %s",
        callCount, GetColorSpaceDesc(pColorSpace));
    
    if (callCount <= 5) {
        Logger::GetInstance().Flush();
    }
    
    g_OriginalSetOutputColorSpace(This, pVideoProcessor, pColorSpace);
}

// ============================================================================
// 初始化
// ============================================================================

class VideoProcessorHook {
public:
    static VideoProcessorHook& GetInstance() {
        static VideoProcessorHook instance;
        return instance;
    }
    
    bool Initialize() {
        if (initialized_) return true;
        
        LOG_INFO("");
        LOG_INFO("=== Video Processor Hook Initialization ===");
        
        // 获取 ID3D11VideoContext 的 VTable
        if (!HookVideoContext()) {
            LOG_ERROR("Failed to hook ID3D11VideoContext");
            return false;
        }
        
        initialized_ = true;
        LOG_INFO("✓ Video Processor Hook initialized successfully!");
        LOG_INFO("✓ Monitoring VideoProcessorBlt and ColorSpace settings");
        LOG_INFO("=============================================\n");
        Logger::GetInstance().Flush();
        
        return true;
    }
    
    void Shutdown() {
        initialized_ = false;
    }
    
private:
    bool initialized_ = false;
    
    bool HookVideoContext() {
        // 首先创建一个 D3D11 设备
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        D3D_FEATURE_LEVEL featureLevel;
        
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT,  // 重要：请求视频支持
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &device,
            &featureLevel,
            &context
        );
        
        if (FAILED(hr)) {
            LOG_ERROR("Failed to create D3D11 device with video support: 0x%08X", hr);
            return false;
        }
        
        LOG_INFO("Created D3D11 device with video support (Feature Level: 0x%X)", featureLevel);
        
        // 获取 ID3D11VideoDevice
        ID3D11VideoDevice* videoDevice = nullptr;
        hr = device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&videoDevice);
        if (FAILED(hr)) {
            LOG_ERROR("Failed to get ID3D11VideoDevice: 0x%08X", hr);
            device->Release();
            context->Release();
            return false;
        }
        
        LOG_INFO("Got ID3D11VideoDevice interface");
        
        // 获取 ID3D11VideoContext
        ID3D11VideoContext* videoContext = nullptr;
        hr = context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&videoContext);
        if (FAILED(hr)) {
            LOG_ERROR("Failed to get ID3D11VideoContext: 0x%08X", hr);
            videoDevice->Release();
            device->Release();
            context->Release();
            return false;
        }
        
        LOG_INFO("Got ID3D11VideoContext interface");
        
        // 获取 VTable
        void** videoContextVTable = *(void***)videoContext;
        
        LOG_INFO("ID3D11VideoContext VTable at: %p", videoContextVTable);
        
        // Hook VideoProcessorBlt (VTable index 22)
        // ID3D11VideoContext 从 index 7 开始，VideoProcessorBlt 是第 16 个方法
        // 所以 7 + 16 - 1 = 22
        void* bltAddr = videoContextVTable[22];
        if (InstallHook(bltAddr, reinterpret_cast<void*>(&Hook_VideoProcessorBlt),
                        (void**)&g_OriginalVideoProcessorBlt, "VideoProcessorBlt")) {
            LOG_INFO("✓ VideoProcessorBlt hooked at %p", bltAddr);
        }
        
        // Hook VideoProcessorSetStreamColorSpace (VTable index 47)
        // ID3D11VideoContext 从 index 7 开始，SetStreamColorSpace 是第 41 个方法
        // 所以 7 + 41 - 1 = 47
        void* setStreamCSAddr = videoContextVTable[47];
        if (InstallHook(setStreamCSAddr, reinterpret_cast<void*>(&Hook_VideoProcessorSetStreamColorSpace),
                        (void**)&g_OriginalSetStreamColorSpace, "VideoProcessorSetStreamColorSpace")) {
            LOG_INFO("✓ VideoProcessorSetStreamColorSpace hooked at %p", setStreamCSAddr);
        }
        
        // Hook VideoProcessorSetOutputColorSpace (VTable index 48)
        // ID3D11VideoContext 从 index 7 开始，SetOutputColorSpace 是第 42 个方法
        // 所以 7 + 42 - 1 = 48
        void* setOutputCSAddr = videoContextVTable[48];
        if (InstallHook(setOutputCSAddr, reinterpret_cast<void*>(&Hook_VideoProcessorSetOutputColorSpace),
                        (void**)&g_OriginalSetOutputColorSpace, "VideoProcessorSetOutputColorSpace")) {
            LOG_INFO("✓ VideoProcessorSetOutputColorSpace hooked at %p", setOutputCSAddr);
        }
        
        // 清理
        videoContext->Release();
        videoDevice->Release();
        context->Release();
        device->Release();
        
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
// 导出函数
// ============================================================================

extern "C" {
    bool InitializeVideoProcessorHooks() {
        return DmitriCompat::VideoProcessorHook::GetInstance().Initialize();
    }
    
    void ShutdownVideoProcessorHooks() {
        DmitriCompat::VideoProcessorHook::GetInstance().Shutdown();
    }
}
