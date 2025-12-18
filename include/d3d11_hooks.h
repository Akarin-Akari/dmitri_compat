#pragma once

#include <d3d11.h>
#include <dxgi.h>

namespace DmitriCompat {

// D3D11 Hook 管理器
class D3D11Hooks {
public:
    static D3D11Hooks& GetInstance();

    bool Initialize();
    void Shutdown();

private:
    D3D11Hooks() = default;
    ~D3D11Hooks() = default;

    D3D11Hooks(const D3D11Hooks&) = delete;
    D3D11Hooks& operator=(const D3D11Hooks&) = delete;

    bool HookD3D11Functions();
    bool HookDXGIFunctions();

    bool initialized_ = false;
};

// Hook 函数声明
// D3D11CreateDevice 的函数指针类型
typedef HRESULT(WINAPI* PFN_D3D11CreateDevice)(
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
);

// Hook 函数
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
);

// 设备方法 Hook
class ID3D11DeviceHook {
public:
    static void HookDevice(ID3D11Device* device);

    // CreateTexture2D Hook
    static HRESULT STDMETHODCALLTYPE Hook_CreateTexture2D(
        ID3D11Device* This,
        const D3D11_TEXTURE2D_DESC* pDesc,
        const D3D11_SUBRESOURCE_DATA* pInitialData,
        ID3D11Texture2D** ppTexture2D
    );
};

// SwapChain Hook
class IDXGISwapChainHook {
public:
    static void HookSwapChain(IDXGISwapChain* swapChain);

    // Present Hook
    static HRESULT STDMETHODCALLTYPE Hook_Present(
        IDXGISwapChain* This,
        UINT SyncInterval,
        UINT Flags
    );
};

} // namespace DmitriCompat
