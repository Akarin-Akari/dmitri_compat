/**
 * compute_shader_replacement.cpp - 用 Compute Shader 替代 CUDA kernel
 * 
 * 这个模块拦截 CUDA kernel 调用，用 D3D11 Compute Shader 执行相同的功能
 * 主要功能：NV12 → BGRA 颜色空间转换
 */

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <unordered_map>
#include "../include/logger.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace DmitriCompat {

// ============================================================================
// Compute Shader 执行器
// ============================================================================

class ComputeShaderReplacement {
private:
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;
    ID3D11ComputeShader* m_pNV12toBGRA = nullptr;
    ID3D11SamplerState* m_pSampler = nullptr;
    bool m_initialized = false;
    
    // 内嵌的 Compute Shader 代码 (避免文件依赖)
    static const char* GetNV12toBGRAShaderCode() {
        return R"(
// NV12 to BGRA Compute Shader
Texture2D<float> texY : register(t0);
Texture2D<float2> texUV : register(t1);
RWTexture2D<float4> outputTex : register(u0);
SamplerState linearSampler : register(s0);

// BT.709 YUV to RGB
static const float3x3 YUVtoRGB = float3x3(
    1.0,  0.0,      1.5748,
    1.0, -0.1873,  -0.4681,
    1.0,  1.8556,   0.0
);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    outputTex.GetDimensions(width, height);
    
    if (DTid.x >= width || DTid.y >= height)
        return;
    
    float2 uv = float2(DTid.x + 0.5, DTid.y + 0.5) / float2(width, height);
    
    float Y = texY.SampleLevel(linearSampler, uv, 0);
    float2 UV = texUV.SampleLevel(linearSampler, uv, 0);
    float U = UV.x - 0.5;
    float V = UV.y - 0.5;
    
    float3 yuv = float3(Y, U, V);
    float3 rgb = mul(YUVtoRGB, yuv);
    rgb = saturate(rgb);
    
    outputTex[DTid.xy] = float4(rgb.b, rgb.g, rgb.r, 1.0);
}
)";
    }

public:
    static ComputeShaderReplacement& GetInstance() {
        static ComputeShaderReplacement instance;
        return instance;
    }
    
    bool Initialize(ID3D11Device* pDevice) {
        if (m_initialized) return true;
        if (!pDevice) return false;
        
        m_pDevice = pDevice;
        m_pDevice->AddRef();
        m_pDevice->GetImmediateContext(&m_pContext);
        
        // 编译 Compute Shader
        ID3DBlob* pBlob = nullptr;
        ID3DBlob* pError = nullptr;
        
        const char* shaderCode = GetNV12toBGRAShaderCode();
        HRESULT hr = D3DCompile(
            shaderCode, strlen(shaderCode),
            "NV12toBGRA", nullptr, nullptr,
            "main", "cs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
            &pBlob, &pError
        );
        
        if (FAILED(hr)) {
            if (pError) {
                LOG_ERROR("❌ [CS Replacement] Shader compile error: %s", (char*)pError->GetBufferPointer());
                pError->Release();
            }
            return false;
        }
        
        hr = m_pDevice->CreateComputeShader(
            pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
            nullptr, &m_pNV12toBGRA
        );
        pBlob->Release();
        
        if (FAILED(hr)) {
            LOG_ERROR("❌ [CS Replacement] CreateComputeShader failed: 0x%08X", hr);
            return false;
        }
        
        // 创建采样器
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        
        hr = m_pDevice->CreateSamplerState(&samplerDesc, &m_pSampler);
        if (FAILED(hr)) {
            LOG_ERROR("❌ [CS Replacement] CreateSamplerState failed: 0x%08X", hr);
            return false;
        }
        
        m_initialized = true;
        LOG_INFO("✅ [CS Replacement] Compute Shader initialized successfully!");
        return true;
    }
    
    void Shutdown() {
        if (m_pSampler) { m_pSampler->Release(); m_pSampler = nullptr; }
        if (m_pNV12toBGRA) { m_pNV12toBGRA->Release(); m_pNV12toBGRA = nullptr; }
        if (m_pContext) { m_pContext->Release(); m_pContext = nullptr; }
        if (m_pDevice) { m_pDevice->Release(); m_pDevice = nullptr; }
        m_initialized = false;
    }
    
    // 执行 NV12 → BGRA 转换
    bool ConvertNV12toBGRA(
        ID3D11ShaderResourceView* pYSRV,
        ID3D11ShaderResourceView* pUVSRV,
        ID3D11UnorderedAccessView* pOutputUAV,
        UINT width, UINT height
    ) {
        if (!m_initialized || !m_pContext || !m_pNV12toBGRA) {
            LOG_ERROR("❌ [CS Replacement] Not initialized!");
            return false;
        }
        
        // 设置 Compute Shader
        m_pContext->CSSetShader(m_pNV12toBGRA, nullptr, 0);
        
        // 设置输入
        ID3D11ShaderResourceView* srvs[2] = { pYSRV, pUVSRV };
        m_pContext->CSSetShaderResources(0, 2, srvs);
        
        // 设置输出
        m_pContext->CSSetUnorderedAccessViews(0, 1, &pOutputUAV, nullptr);
        
        // 设置采样器
        m_pContext->CSSetSamplers(0, 1, &m_pSampler);
        
        // 计算线程组数量 (每个线程组 16x16)
        UINT groupsX = (width + 15) / 16;
        UINT groupsY = (height + 15) / 16;
        
        // 执行
        m_pContext->Dispatch(groupsX, groupsY, 1);
        
        // 清理绑定
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        m_pContext->CSSetShaderResources(0, 2, nullSRVs);
        m_pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        
        LOG_INFO("🎨 [CS Replacement] Executed NV12→BGRA conversion (%ux%u)", width, height);
        return true;
    }
    
    // 从 D3D11 纹理直接执行转换（自动创建 SRV/UAV）
    bool ConvertNV12toBGRAFromTextures(
        ID3D11Texture2D* pNV12Texture,
        ID3D11Texture2D* pOutputTexture
    ) {
        if (!m_initialized || !m_pDevice || !m_pContext) {
            LOG_ERROR("❌ [CS Replacement] Not initialized for texture conversion!");
            return false;
        }
        
        // 获取纹理描述
        D3D11_TEXTURE2D_DESC nv12Desc, outDesc;
        pNV12Texture->GetDesc(&nv12Desc);
        pOutputTexture->GetDesc(&outDesc);
        
        LOG_INFO("🔄 [CS Replacement] Converting textures:");
        LOG_INFO("   NV12: %ux%u, Format=%u", nv12Desc.Width, nv12Desc.Height, nv12Desc.Format);
        LOG_INFO("   Output: %ux%u, Format=%u", outDesc.Width, outDesc.Height, outDesc.Format);
        
        // 创建 Y 平面 SRV (NV12 第一个 subresource)
        ID3D11ShaderResourceView* pYSRV = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC ySrvDesc = {};
        ySrvDesc.Format = DXGI_FORMAT_R8_UNORM;
        ySrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        ySrvDesc.Texture2D.MostDetailedMip = 0;
        ySrvDesc.Texture2D.MipLevels = 1;
        
        HRESULT hr = m_pDevice->CreateShaderResourceView(pNV12Texture, &ySrvDesc, &pYSRV);
        if (FAILED(hr)) {
            LOG_ERROR("❌ [CS Replacement] Failed to create Y SRV: 0x%08X", hr);
            return false;
        }
        
        // 创建 UV 平面 SRV (NV12 第二个 subresource)
        ID3D11ShaderResourceView* pUVSRV = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC uvSrvDesc = {};
        uvSrvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        uvSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        uvSrvDesc.Texture2D.MostDetailedMip = 0;
        uvSrvDesc.Texture2D.MipLevels = 1;
        
        // NV12 的 UV 平面是 array slice 1
        // 但对于单个 NV12 纹理，需要使用 DXGI_FORMAT_NV12 视图
        // 实际上需要创建两个视图分别看 Y 和 UV
        
        hr = m_pDevice->CreateShaderResourceView(pNV12Texture, &uvSrvDesc, &pUVSRV);
        if (FAILED(hr)) {
            // 尝试使用相同的纹理但不同解释
            LOG_ERROR("❌ [CS Replacement] Failed to create UV SRV: 0x%08X", hr);
            pYSRV->Release();
            return false;
        }
        
        // 创建输出 UAV
        ID3D11UnorderedAccessView* pOutputUAV = nullptr;
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = outDesc.Format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        
        hr = m_pDevice->CreateUnorderedAccessView(pOutputTexture, &uavDesc, &pOutputUAV);
        if (FAILED(hr)) {
            LOG_ERROR("❌ [CS Replacement] Failed to create Output UAV: 0x%08X", hr);
            pYSRV->Release();
            pUVSRV->Release();
            return false;
        }
        
        // 执行转换
        bool success = ConvertNV12toBGRA(pYSRV, pUVSRV, pOutputUAV, outDesc.Width, outDesc.Height);
        
        // 清理
        pOutputUAV->Release();
        pUVSRV->Release();
        pYSRV->Release();
        
        return success;
    }
    
    // 获取设备（供外部使用）
    ID3D11Device* GetDevice() const { return m_pDevice; }
    ID3D11DeviceContext* GetContext() const { return m_pContext; }
    
    bool IsInitialized() const { return m_initialized; }
};

// ============================================================================
// 公共 API
// ============================================================================

static bool g_csReplacementEnabled = false;
static ID3D11Device* g_cachedDevice = nullptr;

bool InitializeComputeShaderReplacement(ID3D11Device* pDevice) {
    if (!pDevice) return false;
    
    g_cachedDevice = pDevice;
    g_cachedDevice->AddRef();
    
    if (ComputeShaderReplacement::GetInstance().Initialize(pDevice)) {
        g_csReplacementEnabled = true;
        LOG_INFO("✅ [CS Replacement] Enabled - CUDA calls will be replaced with Compute Shaders");
        return true;
    }
    
    return false;
}

void ShutdownComputeShaderReplacement() {
    ComputeShaderReplacement::GetInstance().Shutdown();
    if (g_cachedDevice) {
        g_cachedDevice->Release();
        g_cachedDevice = nullptr;
    }
    g_csReplacementEnabled = false;
}

bool IsComputeShaderReplacementEnabled() {
    return g_csReplacementEnabled;
}

// 尝试用 Compute Shader 替代 CUDA kernel
// 返回 true 表示已替代，false 表示让原始 CUDA 执行
bool TryReplaceWithComputeShader(
    void* func,
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    void** kernelParams
) {
    (void)func;
    (void)kernelParams;
    
    if (!g_csReplacementEnabled) return false;
    
    // 分析 grid/block 维度来判断这是什么类型的 kernel
    // DmitriRender 的 NV12→BGRA kernel 通常是:
    // grid=(width/16, height/16, 1), block=(16, 16, 1)
    
    bool looksLikeColorConversion = (
        blockDimX == 16 && blockDimY == 16 && blockDimZ == 1 &&
        gridDimZ == 1
    );
    
    if (looksLikeColorConversion) {
        LOG_INFO("🔄 [CS Replacement] Detected potential color conversion kernel");
        LOG_INFO("   Grid: (%u, %u, %u), Block: (%u, %u, %u)",
            gridDimX, gridDimY, gridDimZ,
            blockDimX, blockDimY, blockDimZ);
        
        // 这里返回 true 表示我们"接管"了这个 kernel
        // 实际转换在 ExecuteNV12ToBGRAConversion 中执行
        return true;
    }
    
    return false;
}

// 执行 NV12 到 BGRA 的转换
// pNV12: NV12 格式的源纹理
// pBGRA: BGRA 格式的目标纹理
bool ExecuteNV12ToBGRAConversion(
    ID3D11Texture2D* pNV12,
    ID3D11Texture2D* pBGRA
) {
    if (!g_csReplacementEnabled) {
        LOG_ERROR("❌ [CS Replacement] Not enabled!");
        return false;
    }
    
    if (!pNV12 || !pBGRA) {
        LOG_ERROR("❌ [CS Replacement] Null texture pointer!");
        return false;
    }
    
    return ComputeShaderReplacement::GetInstance().ConvertNV12toBGRAFromTextures(pNV12, pBGRA);
}

// 获取追踪到的最近的 NV12 和 BGRA 纹理用于转换
// 返回是否找到有效的纹理对
bool GetTrackedTexturesForConversion(
    ID3D11Texture2D** ppNV12Out,
    ID3D11Texture2D** ppBGRAOut
);

// 执行自动纹理转换（使用追踪到的纹理）
bool ExecuteAutoConversion() {
    ID3D11Texture2D* pNV12 = nullptr;
    ID3D11Texture2D* pBGRA = nullptr;
    
    if (!GetTrackedTexturesForConversion(&pNV12, &pBGRA)) {
        LOG_ERROR("❌ [CS Replacement] No tracked textures found for conversion!");
        return false;
    }
    
    return ExecuteNV12ToBGRAConversion(pNV12, pBGRA);
}

} // namespace DmitriCompat

