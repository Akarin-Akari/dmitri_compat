/**
 * keyed_mutex_hook.cpp - 假的 KeyedMutex 实现 + VTable Hook
 * 
 * 问题：我们用 0x2 (SHARED) 创建纹理来绕过 RTX 50 的 0x900 问题
 *       但 DmitriRender 直接在纹理上调用 QueryInterface 获取 KeyedMutex
 *       0x2 纹理没有 KeyedMutex 接口，QueryInterface 返回 E_NOINTERFACE → 崩溃
 * 
 * 解决方案：Hook 纹理的 VTable，让 QueryInterface 返回我们的假 KeyedMutex
 */

#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <atomic>
#include <unordered_map>
#include "../include/logger.h"

namespace DmitriCompat {

// ============================================================================
// 假的 IDXGIKeyedMutex 实现
// ============================================================================

class FakeKeyedMutex : public IDXGIKeyedMutex {
private:
    std::atomic<ULONG> m_refCount;
    IUnknown* m_pOwner;
    
public:
    FakeKeyedMutex(IUnknown* pOwner) 
        : m_refCount(1), m_pOwner(pOwner) {
        LOG_INFO("🔧 [FakeKeyedMutex] Created for owner %p", pOwner);
    }
    
    virtual ~FakeKeyedMutex() {
        LOG_INFO("🔧 [FakeKeyedMutex] Destroyed");
    }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIKeyedMutex)) {
            *ppvObject = static_cast<IDXGIKeyedMutex*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject)) {
            *ppvObject = static_cast<IDXGIDeviceSubObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG ref = --m_refCount;
        if (ref == 0) delete this;
        return ref;
    }
    
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetParent(REFIID, void**) override { return E_NOTIMPL; }
    
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) override {
        if (m_pOwner) return m_pOwner->QueryInterface(riid, ppDevice);
        return E_NOTIMPL;
    }
    
    HRESULT STDMETHODCALLTYPE AcquireSync(UINT64 Key, DWORD dwMilliseconds) override {
        static int count = 0;
        if (++count <= 5) LOG_INFO("🔒 [FakeKeyedMutex] AcquireSync(Key=%llu) → S_OK", Key);
        return S_OK;
    }
    
    HRESULT STDMETHODCALLTYPE ReleaseSync(UINT64 Key) override {
        static int count = 0;
        if (++count <= 5) LOG_INFO("🔓 [FakeKeyedMutex] ReleaseSync(Key=%llu) → S_OK", Key);
        return S_OK;
    }
};

// ============================================================================
// VTable Hook 数据
// ============================================================================

typedef HRESULT(STDMETHODCALLTYPE* QueryInterface_t)(IUnknown* This, REFIID riid, void** ppvObject);

struct TextureHookData {
    QueryInterface_t OriginalQueryInterface;
    FakeKeyedMutex* FakeMutex;
    void** OriginalVTable;
};

static std::unordered_map<IUnknown*, TextureHookData> g_hookedTextures;
static CRITICAL_SECTION g_hookLock;
static bool g_initialized = false;

// ============================================================================
// Hooked QueryInterface
// ============================================================================

HRESULT STDMETHODCALLTYPE Hooked_QueryInterface(IUnknown* This, REFIID riid, void** ppvObject) {
    EnterCriticalSection(&g_hookLock);
    auto it = g_hookedTextures.find(This);
    if (it != g_hookedTextures.end()) {
        TextureHookData& data = it->second;
        
        if (riid == __uuidof(IDXGIKeyedMutex)) {
            LOG_INFO("🎯 [VTable Hook] QueryInterface for KeyedMutex → FakeKeyedMutex");
            *ppvObject = data.FakeMutex;
            data.FakeMutex->AddRef();
            LeaveCriticalSection(&g_hookLock);
            return S_OK;
        }
        
        QueryInterface_t originalQI = data.OriginalQueryInterface;
        LeaveCriticalSection(&g_hookLock);
        return originalQI(This, riid, ppvObject);
    }
    LeaveCriticalSection(&g_hookLock);
    
    LOG_ERROR("❌ [VTable Hook] Unknown texture in Hooked_QueryInterface!");
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

// ============================================================================
// 公共 API
// ============================================================================

void InitKeyedMutexHook() {
    if (!g_initialized) {
        InitializeCriticalSection(&g_hookLock);
        g_initialized = true;
        LOG_INFO("🔧 [KeyedMutex Hook] Initialized with VTable patching");
    }
}

void CleanupKeyedMutexHook() {
    if (g_initialized) {
        EnterCriticalSection(&g_hookLock);
        for (auto& pair : g_hookedTextures) {
            if (pair.second.FakeMutex) pair.second.FakeMutex->Release();
        }
        g_hookedTextures.clear();
        LeaveCriticalSection(&g_hookLock);
        DeleteCriticalSection(&g_hookLock);
        g_initialized = false;
    }
}

void RegisterTextureForFakeKeyedMutex(ID3D11Texture2D* pTexture) {
    if (!pTexture || !g_initialized) return;
    
    EnterCriticalSection(&g_hookLock);
    
    if (g_hookedTextures.find(pTexture) != g_hookedTextures.end()) {
        LeaveCriticalSection(&g_hookLock);
        return;
    }
    
    void** vtable = *(void***)pTexture;
    QueryInterface_t originalQI = (QueryInterface_t)vtable[0];
    FakeKeyedMutex* fakeMutex = new FakeKeyedMutex(pTexture);
    
    TextureHookData data;
    data.OriginalQueryInterface = originalQI;
    data.FakeMutex = fakeMutex;
    data.OriginalVTable = vtable;
    g_hookedTextures[pTexture] = data;
    
    DWORD oldProtect;
    if (VirtualProtect(vtable, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        vtable[0] = (void*)Hooked_QueryInterface;
        VirtualProtect(vtable, sizeof(void*), oldProtect, &oldProtect);
        LOG_INFO("✅ [VTable Hook] Hooked texture %p (QI: %p → %p)", pTexture, originalQI, Hooked_QueryInterface);
    } else {
        LOG_ERROR("❌ [VTable Hook] VirtualProtect failed for %p", pTexture);
        delete fakeMutex;
        g_hookedTextures.erase(pTexture);
    }
    
    LeaveCriticalSection(&g_hookLock);
}

IDXGIKeyedMutex* GetFakeKeyedMutex(ID3D11Texture2D* pTexture) {
    if (!pTexture || !g_initialized) return nullptr;
    EnterCriticalSection(&g_hookLock);
    IDXGIKeyedMutex* result = nullptr;
    auto it = g_hookedTextures.find(pTexture);
    if (it != g_hookedTextures.end()) {
        result = it->second.FakeMutex;
        result->AddRef();
    }
    LeaveCriticalSection(&g_hookLock);
    return result;
}

} // namespace DmitriCompat
