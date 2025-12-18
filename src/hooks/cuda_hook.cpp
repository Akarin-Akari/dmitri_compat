/**
 * cuda_hook.cpp - Hook CUDA Driver API
 * 
 * 用于诊断 RTX 50 系列绿屏问题
 * DmitriRender 使用 CUDA 进行帧插值和色彩转换
 */

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <d3d11.h>
#include "../external/minhook/include/MinHook.h"
#include "../include/logger.h"

// 外部声明：Compute Shader 替代模块
namespace DmitriCompat {
    extern bool InitializeComputeShaderReplacement(ID3D11Device* pDevice);
    extern void ShutdownComputeShaderReplacement();
    extern bool IsComputeShaderReplacementEnabled();
    extern bool ExecuteNV12ToBGRAConversion(ID3D11Texture2D* pNV12, ID3D11Texture2D* pBGRA);
}

// CUDA Driver API 类型定义
typedef int CUresult;
typedef void* CUcontext;
typedef void* CUdevice;
typedef void* CUmodule;
typedef void* CUfunction;
typedef void* CUstream;
typedef void* CUdeviceptr;
typedef void* CUgraphicsResource;
typedef unsigned int CUarray_format;

#define CUDA_SUCCESS 0

// CUDA_MEMCPY2D 结构
typedef struct {
    size_t srcXInBytes;
    size_t srcY;
    int srcMemoryType;  // CU_MEMORYTYPE_*
    const void* srcHost;
    CUdeviceptr srcDevice;
    void* srcArray;
    size_t srcPitch;
    
    size_t dstXInBytes;
    size_t dstY;
    int dstMemoryType;
    void* dstHost;
    CUdeviceptr dstDevice;
    void* dstArray;
    size_t dstPitch;
    
    size_t WidthInBytes;
    size_t Height;
} MY_CUDA_MEMCPY2D;

namespace DmitriCompat {

// ============================================================================
// 原始函数指针
// ============================================================================

typedef CUresult (*PFN_cuInit)(unsigned int flags);
typedef CUresult (*PFN_cuCtxCreate)(CUcontext* pctx, unsigned int flags, CUdevice dev);
typedef CUresult (*PFN_cuModuleLoad)(CUmodule* module, const char* fname);
typedef CUresult (*PFN_cuModuleLoadData)(CUmodule* module, const void* image);
typedef CUresult (*PFN_cuModuleLoadDataEx)(CUmodule* module, const void* image, 
    unsigned int numOptions, void* options, void** optionValues);
typedef CUresult (*PFN_cuModuleGetFunction)(CUfunction* hfunc, CUmodule hmod, const char* name);
typedef CUresult (*PFN_cuLaunchKernel)(
    CUfunction f,
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    CUstream hStream,
    void** kernelParams,
    void** extra
);
typedef CUresult (*PFN_cuMemcpy2D)(const MY_CUDA_MEMCPY2D* pCopy);
typedef CUresult (*PFN_cuMemAlloc)(CUdeviceptr* dptr, size_t bytesize);
typedef CUresult (*PFN_cuGraphicsD3D11RegisterResource)(
    CUgraphicsResource* pCudaResource,
    void* pD3DResource,  // ID3D11Resource*
    unsigned int Flags
);
typedef CUresult (*PFN_cuGraphicsMapResources)(
    unsigned int count,
    CUgraphicsResource* resources,
    CUstream hStream
);
typedef CUresult (*PFN_cuGraphicsUnmapResources)(
    unsigned int count,
    CUgraphicsResource* resources,
    CUstream hStream
);

static PFN_cuInit g_Original_cuInit = nullptr;
static PFN_cuCtxCreate g_Original_cuCtxCreate = nullptr;
static PFN_cuModuleLoad g_Original_cuModuleLoad = nullptr;
static PFN_cuModuleLoadData g_Original_cuModuleLoadData = nullptr;
static PFN_cuModuleLoadDataEx g_Original_cuModuleLoadDataEx = nullptr;
static PFN_cuModuleGetFunction g_Original_cuModuleGetFunction = nullptr;
static PFN_cuLaunchKernel g_Original_cuLaunchKernel = nullptr;
static PFN_cuMemcpy2D g_Original_cuMemcpy2D = nullptr;
static PFN_cuMemAlloc g_Original_cuMemAlloc = nullptr;
static PFN_cuGraphicsD3D11RegisterResource g_Original_cuGraphicsD3D11RegisterResource = nullptr;
static PFN_cuGraphicsMapResources g_Original_cuGraphicsMapResources = nullptr;
static PFN_cuGraphicsUnmapResources g_Original_cuGraphicsUnmapResources = nullptr;

// 统计计数器
static int g_cuInitCount = 0;
static int g_cuCtxCreateCount = 0;
static int g_cuModuleLoadCount = 0;
static int g_cuLaunchKernelCount = 0;
static int g_cuLaunchKernelFailedCount = 0;  // 失败计数
static int g_cuMemcpy2DCount = 0;
static int g_cuMemAllocCount = 0;
static int g_cuGraphicsRegisterCount = 0;
static int g_cuGraphicsMapCount = 0;

// ============================================================================
// D3D11 纹理追踪 (用于 Compute Shader 替代)
// ============================================================================

struct TextureInfo {
    ID3D11Resource* pD3DResource = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    bool isNV12 = false;        // Y/UV 平面源
    bool isBGRA = false;        // 输出目标
};

static std::unordered_map<CUgraphicsResource, TextureInfo> g_cudaToD3DMap;
static std::mutex g_textureMapMutex;
static ID3D11Device* g_cachedD3DDevice = nullptr;
static bool g_csReplacementAttempted = false;

// 活动映射的纹理 (用于 cuGraphicsMapResources 追踪)
static std::vector<CUgraphicsResource> g_activeMappedResources;
static ID3D11Texture2D* g_lastNV12Texture = nullptr;
static ID3D11Texture2D* g_lastBGRATexture = nullptr;

// 供 compute_shader_replacement.cpp 调用
bool GetTrackedTexturesForConversion(
    ID3D11Texture2D** ppNV12Out,
    ID3D11Texture2D** ppBGRAOut
) {
    std::lock_guard<std::mutex> lock(g_textureMapMutex);
    
    if (!g_lastNV12Texture || !g_lastBGRATexture) {
        // 尝试从映射中查找
        ID3D11Texture2D* pNV12 = nullptr;
        ID3D11Texture2D* pBGRA = nullptr;
        
        for (const auto& pair : g_cudaToD3DMap) {
            const TextureInfo& info = pair.second;
            if (info.isNV12 && !pNV12) {
                D3D11_RESOURCE_DIMENSION dim;
                info.pD3DResource->GetType(&dim);
                if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
                    pNV12 = static_cast<ID3D11Texture2D*>(info.pD3DResource);
                }
            }
            if (info.isBGRA && !pBGRA) {
                D3D11_RESOURCE_DIMENSION dim;
                info.pD3DResource->GetType(&dim);
                if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
                    pBGRA = static_cast<ID3D11Texture2D*>(info.pD3DResource);
                }
            }
        }
        
        if (pNV12 && pBGRA) {
            g_lastNV12Texture = pNV12;
            g_lastBGRATexture = pBGRA;
        }
    }
    
    if (ppNV12Out) *ppNV12Out = g_lastNV12Texture;
    if (ppBGRAOut) *ppBGRAOut = g_lastBGRATexture;
    
    return (g_lastNV12Texture != nullptr && g_lastBGRATexture != nullptr);
}

// ============================================================================
// Hook 函数
// ============================================================================

CUresult Hook_cuInit(unsigned int flags) {
    g_cuInitCount++;
    LOG_INFO("🔥 cuInit #%d: flags=0x%X", g_cuInitCount, flags);
    Logger::GetInstance().Flush();
    
    CUresult result = g_Original_cuInit(flags);
    
    if (result != CUDA_SUCCESS) {
        LOG_ERROR("❌ cuInit FAILED: result=%d", result);
    } else {
        LOG_INFO("✓ cuInit SUCCESS");
    }
    Logger::GetInstance().Flush();
    
    return result;
}

CUresult Hook_cuCtxCreate(CUcontext* pctx, unsigned int flags, CUdevice dev) {
    g_cuCtxCreateCount++;
    LOG_INFO("🔥 cuCtxCreate #%d: flags=0x%X, device=%p", g_cuCtxCreateCount, flags, dev);
    Logger::GetInstance().Flush();
    
    CUresult result = g_Original_cuCtxCreate(pctx, flags, dev);
    
    if (result != CUDA_SUCCESS) {
        LOG_ERROR("❌ cuCtxCreate FAILED: result=%d", result);
    } else {
        LOG_INFO("✓ cuCtxCreate SUCCESS: context=%p", pctx ? *pctx : nullptr);
    }
    Logger::GetInstance().Flush();
    
    return result;
}

CUresult Hook_cuModuleLoad(CUmodule* module, const char* fname) {
    g_cuModuleLoadCount++;
    LOG_INFO("🔥 cuModuleLoad #%d: file=%s", g_cuModuleLoadCount, fname ? fname : "NULL");
    Logger::GetInstance().Flush();
    
    CUresult result = g_Original_cuModuleLoad(module, fname);
    
    if (result != CUDA_SUCCESS) {
        LOG_ERROR("❌ cuModuleLoad FAILED: result=%d", result);
    }
    
    return result;
}

CUresult Hook_cuModuleLoadData(CUmodule* module, const void* image) {
    g_cuModuleLoadCount++;
    LOG_INFO("🔥 cuModuleLoadData #%d: image=%p", g_cuModuleLoadCount, image);
    Logger::GetInstance().Flush();
    
    // ========================================================================
    // RTX 50 兼容性修复：使用 cuModuleLoadDataEx 添加 JIT 选项
    // ========================================================================
    // cuModuleLoadData 不支持 JIT 选项，所以我们转换为 cuModuleLoadDataEx 调用
    // 添加 CU_JIT_TARGET 强制指定目标架构
    // ========================================================================
    
    // 定义 JIT 选项
    // CU_JIT_TARGET = 0 (第一个枚举值)
    // CU_TARGET_COMPUTE_89 = 89 (Ada Lovelace / RTX 40 系列)
    // 对于 RTX 50 Blackwell，理论上是 compute_100 或 compute_120
    // 但我们先尝试让它 fallback 到兼容模式
    
    // 首先尝试原始调用
    CUresult result = g_Original_cuModuleLoadData(module, image);
    
    if (result == CUDA_SUCCESS) {
        LOG_INFO("✓ cuModuleLoadData SUCCESS: module=%p", module ? *module : nullptr);
    } else {
        LOG_ERROR("❌ cuModuleLoadData FAILED: result=%d", result);
        LOG_INFO("   💡 [RTX 50 Fix] Trying cuModuleLoadDataEx with JIT options...");
        Logger::GetInstance().Flush();
        
        // 尝试使用 cuModuleLoadDataEx 带 JIT 选项
        // CU_JIT_FALLBACK_STRATEGY = 7, CU_PREFER_PTX = 1 (优先使用 PTX 重新编译)
        const int CU_JIT_FALLBACK_STRATEGY = 7;
        const int CU_PREFER_PTX = 1;
        
        unsigned int jitOptions[] = { CU_JIT_FALLBACK_STRATEGY };
        void* jitOptionValues[] = { (void*)(uintptr_t)CU_PREFER_PTX };
        
        CUresult retryResult = g_Original_cuModuleLoadDataEx(
            module, image, 1, jitOptions, jitOptionValues
        );
        
        if (retryResult == CUDA_SUCCESS) {
            LOG_INFO("✅ [RTX 50 Fix] cuModuleLoadDataEx with JIT fallback SUCCEEDED!");
            result = retryResult;
        } else {
            LOG_ERROR("❌ [RTX 50 Fix] cuModuleLoadDataEx also FAILED: result=%d", retryResult);
        }
    }
    
    return result;
}

CUresult Hook_cuModuleLoadDataEx(CUmodule* module, const void* image, 
    unsigned int numOptions, void* options, void** optionValues) {
    g_cuModuleLoadCount++;
    LOG_INFO("🔥 cuModuleLoadDataEx #%d: image=%p, numOptions=%u", 
        g_cuModuleLoadCount, image, numOptions);
    Logger::GetInstance().Flush();
    
    // ========================================================================
    // RTX 50 兼容性修复：添加 JIT fallback 选项
    // ========================================================================
    
    // 首先尝试原始调用
    CUresult result = g_Original_cuModuleLoadDataEx(module, image, numOptions, options, optionValues);
    
    if (result == CUDA_SUCCESS) {
        LOG_INFO("✓ cuModuleLoadDataEx SUCCESS: module=%p", module ? *module : nullptr);
    } else {
        LOG_ERROR("❌ cuModuleLoadDataEx FAILED: result=%d, numOptions=%u", result, numOptions);
        
        // 如果失败，尝试添加 PTX fallback 选项重试
        if (numOptions < 10) {  // 防止栈溢出
            LOG_INFO("   💡 [RTX 50 Fix] Retrying with extended JIT options...");
            Logger::GetInstance().Flush();
            
            const int CU_JIT_FALLBACK_STRATEGY = 7;
            const int CU_PREFER_PTX = 1;
            
            // 创建扩展选项数组
            unsigned int extOptions[12];
            void* extValues[12];
            
            // 复制原始选项
            for (unsigned int i = 0; i < numOptions && i < 10; i++) {
                extOptions[i] = ((unsigned int*)options)[i];
                extValues[i] = optionValues[i];
            }
            
            // 添加 fallback 策略
            extOptions[numOptions] = CU_JIT_FALLBACK_STRATEGY;
            extValues[numOptions] = (void*)(uintptr_t)CU_PREFER_PTX;
            
            CUresult retryResult = g_Original_cuModuleLoadDataEx(
                module, image, numOptions + 1, extOptions, extValues
            );
            
            if (retryResult == CUDA_SUCCESS) {
                LOG_INFO("✅ [RTX 50 Fix] cuModuleLoadDataEx with extended options SUCCEEDED!");
                result = retryResult;
            } else {
                LOG_ERROR("❌ [RTX 50 Fix] Extended options also FAILED: result=%d", retryResult);
            }
        }
    }
    
    return result;
}

CUresult Hook_cuModuleGetFunction(CUfunction* hfunc, CUmodule hmod, const char* name) {
    static int getfuncCount = 0;
    getfuncCount++;
    
    // 记录所有函数名！这对找到 NV12->RGB kernel 非常重要
    LOG_INFO("🔍 cuModuleGetFunction #%d: name=\"%s\"", getfuncCount, name ? name : "NULL");
    Logger::GetInstance().Flush();
    
    CUresult result = g_Original_cuModuleGetFunction(hfunc, hmod, name);
    
    if (result != CUDA_SUCCESS) {
        LOG_ERROR("❌ cuModuleGetFunction FAILED: name=%s, result=%d", name, result);
    }
    
    return result;
}

CUresult Hook_cuLaunchKernel(
    CUfunction f,
    unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes,
    CUstream hStream,
    void** kernelParams,
    void** extra
) {
    g_cuLaunchKernelCount++;
    
    // ========================================================================
    // RTX 50 兼容模式：func=NULL 时返回成功（假装 kernel 执行成功）
    // ========================================================================
    // 注意：不能在这里调用任何 D3D11 API，否则会导致 NvPresent64.dll 崩溃
    // ========================================================================
    
    bool funcIsNull = (f == nullptr);
    
    // 记录前 100 次和每 500 次
    if (g_cuLaunchKernelCount <= 100 || g_cuLaunchKernelCount % 500 == 0) {
        if (funcIsNull) {
            LOG_ERROR("🚀 cuLaunchKernel #%d: func=NULL! grid=(%u,%u,%u), block=(%u,%u,%u)",
                g_cuLaunchKernelCount,
                gridDimX, gridDimY, gridDimZ,
                blockDimX, blockDimY, blockDimZ);
        } else {
            LOG_INFO("🚀 cuLaunchKernel #%d: func=%p, grid=(%u,%u,%u), block=(%u,%u,%u)",
                g_cuLaunchKernelCount, f,
                gridDimX, gridDimY, gridDimZ,
                blockDimX, blockDimY, blockDimZ);
        }
        
        if (g_cuLaunchKernelCount <= 10) {
            Logger::GetInstance().Flush();
        }
    }
    
    // 核心修复：如果函数指针为 NULL，直接返回成功
    // 这让 DmitriRender 以为 kernel 执行成功，避免错误处理流程
    if (funcIsNull) {
        g_cuLaunchKernelFailedCount++;
        
        // 统计信息（每 100 次打印一次）
        if (g_cuLaunchKernelFailedCount <= 5 || g_cuLaunchKernelFailedCount % 100 == 0) {
            LOG_INFO("🔧 [RTX 50 Mode] Bypassing NULL kernel #%d (block=%ux%u)",
                g_cuLaunchKernelFailedCount, blockDimX, blockDimY);
        }
        
        // 返回成功，让程序继续运行
        return CUDA_SUCCESS;
    }
    
    // 函数指针有效，正常调用
    CUresult result = g_Original_cuLaunchKernel(
        f, gridDimX, gridDimY, gridDimZ,
        blockDimX, blockDimY, blockDimZ,
        sharedMemBytes, hStream, kernelParams, extra
    );
    
    if (result != CUDA_SUCCESS && g_cuLaunchKernelCount <= 50) {
        LOG_ERROR("❌ cuLaunchKernel #%d FAILED: result=%d", g_cuLaunchKernelCount, result);
    }
    
    return result;
}

CUresult Hook_cuMemcpy2D(const MY_CUDA_MEMCPY2D* pCopy) {
    g_cuMemcpy2DCount++;
    
    // 记录前 50 次和每 200 次
    if (g_cuMemcpy2DCount <= 50 || g_cuMemcpy2DCount % 200 == 0) {
        if (pCopy) {
            LOG_INFO("📋 cuMemcpy2D #%d: %zux%zu bytes, srcType=%d, dstType=%d",
                g_cuMemcpy2DCount,
                pCopy->WidthInBytes, pCopy->Height,
                pCopy->srcMemoryType, pCopy->dstMemoryType);
        }
    }
    
    return g_Original_cuMemcpy2D(pCopy);
}

CUresult Hook_cuMemAlloc(CUdeviceptr* dptr, size_t bytesize) {
    g_cuMemAllocCount++;
    
    // 记录大于 1MB 的分配
    if (bytesize >= 1024 * 1024 || g_cuMemAllocCount <= 20) {
        LOG_INFO("💾 cuMemAlloc #%d: size=%zu bytes (%.2f MB)",
            g_cuMemAllocCount, bytesize, bytesize / (1024.0 * 1024.0));
    }
    
    return g_Original_cuMemAlloc(dptr, bytesize);
}

CUresult Hook_cuGraphicsD3D11RegisterResource(
    CUgraphicsResource* pCudaResource,
    void* pD3DResource,
    unsigned int Flags
) {
    g_cuGraphicsRegisterCount++;
    
    // ========================================================================
    // 简化版本：只记录日志，不做 D3D11 查询
    // ========================================================================
    
    if (g_cuGraphicsRegisterCount <= 20 || g_cuGraphicsRegisterCount % 100 == 0) {
        LOG_INFO("🔗 cuGraphicsD3D11RegisterResource #%d: D3D11Resource=%p, flags=0x%X",
            g_cuGraphicsRegisterCount, pD3DResource, Flags);
    }
    
    CUresult result = g_Original_cuGraphicsD3D11RegisterResource(pCudaResource, pD3DResource, Flags);
    
    if (result != CUDA_SUCCESS && g_cuGraphicsRegisterCount <= 20) {
        LOG_ERROR("❌ cuGraphicsD3D11RegisterResource FAILED: result=%d", result);
    }
    
    if (g_cuGraphicsRegisterCount <= 10) {
        Logger::GetInstance().Flush();
    }
    
    return result;
}

CUresult Hook_cuGraphicsMapResources(
    unsigned int count,
    CUgraphicsResource* resources,
    CUstream hStream
) {
    g_cuGraphicsMapCount++;
    
    if (g_cuGraphicsMapCount <= 50 || g_cuGraphicsMapCount % 200 == 0) {
        LOG_INFO("📌 cuGraphicsMapResources #%d: count=%u", g_cuGraphicsMapCount, count);
    }
    
    return g_Original_cuGraphicsMapResources(count, resources, hStream);
}

CUresult Hook_cuGraphicsUnmapResources(
    unsigned int count,
    CUgraphicsResource* resources,
    CUstream hStream
) {
    // 不记录 Unmap，太频繁
    return g_Original_cuGraphicsUnmapResources(count, resources, hStream);
}

// ============================================================================
// 初始化
// ============================================================================

class CudaHook {
public:
    static CudaHook& GetInstance() {
        static CudaHook instance;
        return instance;
    }
    
    bool Initialize() {
        if (initialized_) return true;
        
        LOG_INFO("");
        LOG_INFO("=== CUDA Hook Initialization ===");
        LOG_INFO("Looking for nvcuda.dll...");
        
        // 获取 nvcuda.dll
        HMODULE hCuda = GetModuleHandleA("nvcuda.dll");
        if (!hCuda) {
            // 尝试加载
            hCuda = LoadLibraryA("nvcuda.dll");
        }
        
        if (!hCuda) {
            LOG_ERROR("Failed to find nvcuda.dll - CUDA not available");
            return false;
        }
        
        LOG_INFO("Found nvcuda.dll at %p", hCuda);
        
        // Hook 所有关键 API
        bool success = true;
        success &= HookCudaFunction(hCuda, "cuInit", (void*)Hook_cuInit, (void**)&g_Original_cuInit);
        success &= HookCudaFunction(hCuda, "cuCtxCreate_v2", (void*)Hook_cuCtxCreate, (void**)&g_Original_cuCtxCreate);
        success &= HookCudaFunction(hCuda, "cuModuleLoad", (void*)Hook_cuModuleLoad, (void**)&g_Original_cuModuleLoad);
        success &= HookCudaFunction(hCuda, "cuModuleLoadData", (void*)Hook_cuModuleLoadData, (void**)&g_Original_cuModuleLoadData);
        success &= HookCudaFunction(hCuda, "cuModuleLoadDataEx", (void*)Hook_cuModuleLoadDataEx, (void**)&g_Original_cuModuleLoadDataEx);
        success &= HookCudaFunction(hCuda, "cuModuleGetFunction", (void*)Hook_cuModuleGetFunction, (void**)&g_Original_cuModuleGetFunction);
        success &= HookCudaFunction(hCuda, "cuLaunchKernel", (void*)Hook_cuLaunchKernel, (void**)&g_Original_cuLaunchKernel);
        success &= HookCudaFunction(hCuda, "cuMemcpy2D_v2", (void*)Hook_cuMemcpy2D, (void**)&g_Original_cuMemcpy2D);
        success &= HookCudaFunction(hCuda, "cuMemAlloc_v2", (void*)Hook_cuMemAlloc, (void**)&g_Original_cuMemAlloc);
        success &= HookCudaFunction(hCuda, "cuGraphicsD3D11RegisterResource", 
            (void*)Hook_cuGraphicsD3D11RegisterResource, (void**)&g_Original_cuGraphicsD3D11RegisterResource);
        success &= HookCudaFunction(hCuda, "cuGraphicsMapResources", 
            (void*)Hook_cuGraphicsMapResources, (void**)&g_Original_cuGraphicsMapResources);
        success &= HookCudaFunction(hCuda, "cuGraphicsUnmapResources", 
            (void*)Hook_cuGraphicsUnmapResources, (void**)&g_Original_cuGraphicsUnmapResources);
        
        initialized_ = true;
        LOG_INFO("=================================");
        LOG_INFO("✓ CUDA Hook initialized! Monitoring all CUDA calls");
        LOG_INFO("=================================\n");
        Logger::GetInstance().Flush();
        
        return true;
    }
    
    void Shutdown() {
        if (!initialized_) return;
        
        LOG_INFO("");
        LOG_INFO("=== CUDA Hook Statistics ===");
        LOG_INFO("  cuInit: %d", g_cuInitCount);
        LOG_INFO("  cuCtxCreate: %d", g_cuCtxCreateCount);
        LOG_INFO("  cuModuleLoad: %d", g_cuModuleLoadCount);
        LOG_INFO("  cuLaunchKernel: %d", g_cuLaunchKernelCount);
        LOG_INFO("  cuMemcpy2D: %d", g_cuMemcpy2DCount);
        LOG_INFO("  cuMemAlloc: %d", g_cuMemAllocCount);
        LOG_INFO("  cuGraphicsRegister: %d", g_cuGraphicsRegisterCount);
        LOG_INFO("  cuGraphicsMap: %d", g_cuGraphicsMapCount);
        LOG_INFO("============================\n");
        Logger::GetInstance().Flush();
        
        initialized_ = false;
    }
    
private:
    bool initialized_ = false;
    
    bool HookCudaFunction(HMODULE hCuda, const char* name, void* detour, void** original) {
        void* target = (void*)GetProcAddress(hCuda, name);
        if (!target) {
            LOG_INFO("  [SKIP] %s not found", name);
            return true;  // 不是错误，可能版本不同
        }
        
        MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status != MH_OK) {
            LOG_ERROR("  [FAIL] %s: MH_CreateHook failed (%d)", name, status);
            return false;
        }
        
        status = MH_EnableHook(target);
        if (status != MH_OK) {
            LOG_ERROR("  [FAIL] %s: MH_EnableHook failed (%d)", name, status);
            return false;
        }
        
        LOG_INFO("  ✓ %s hooked at %p", name, target);
        return true;
    }
};

} // namespace DmitriCompat

// ============================================================================
// 导出函数
// ============================================================================

extern "C" {
    bool InitializeCudaHooks() {
        return DmitriCompat::CudaHook::GetInstance().Initialize();
    }
    
    void ShutdownCudaHooks() {
        DmitriCompat::CudaHook::GetInstance().Shutdown();
    }
}
