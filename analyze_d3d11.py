#!/usr/bin/env python3
"""
详细分析 dmitriRenderBase.dll 的 D3D11 导入
"""

import pefile
import io
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

dll_path = "x64/dmitriRenderBase.dll"
pe = pefile.PE(dll_path)

print("="*80)
print("dmitriRenderBase.dll DirectX 相关导入分析")
print("="*80)

dx_dlls = ['d3d11.dll', 'dxgi.dll', 'dxva2.dll']

for entry in pe.DIRECTORY_ENTRY_IMPORT:
    dll_name = entry.dll.decode('utf-8')

    if dll_name.lower() in dx_dlls:
        print(f"\n【{dll_name}】")
        for imp in entry.imports:
            if imp.name:
                func_name = imp.name.decode('utf-8')
                print(f"  - {func_name}")
            else:
                print(f"  - Ordinal: {imp.ordinal}")

        # 添加说明
        if dll_name.lower() == 'd3d11.dll':
            print("\n  说明:")
            print("  D3D11CreateDevice - 创建 Direct3D 11 设备")
            print("  这是 D3D11 应用的核心函数，用于初始化图形设备")
            print("  RTX 50 系列完全支持 D3D11，所以问题可能在设备创建的参数或后续调用")

        elif dll_name.lower() == 'dxgi.dll':
            print("\n  说明:")
            print("  CreateDXGIFactory/CreateDXGIFactory1 - 创建 DXGI 工厂对象")
            print("  DXGI 用于管理显示适配器、交换链等")
            print("  可能的问题点: 枚举适配器、选择输出格式")

        elif dll_name.lower() == 'dxva2.dll':
            print("\n  说明:")
            print("  DXVA2CreateVideoService - DirectX 视频加速服务")
            print("  用于硬件加速视频解码/编码")
            print("  RTX 50 支持 DXVA2，但可能在某些格式上有变化")

print("\n" + "="*80)
print("潜在问题分析")
print("="*80)

print("""
1. 纹理格式支持
   - RTX 50 可能不支持某些旧的纹理格式
   - 需要检查创建纹理时使用的 DXGI_FORMAT

2. 特性级别 (Feature Level)
   - D3D11CreateDevice 需要指定特性级别
   - 如果代码硬编码了旧的特性级别，可能导致问题

3. 驱动兼容性
   - RTX 50 的 Blackwell 架构可能需要新的代码路径
   - 某些 D3D11 扩展可能已被弃用

4. 颜色空间/像素格式
   - 视频补帧涉及 YUV <-> RGB 转换
   - DXVA2 的某些格式可能在新架构上行为不同

5. 同步问题
   - GPU 异步执行可能有新的时序要求
   - 需要适当的 Flush/Sync 调用
""")

print("="*80)
print("推荐的 Hook 点")
print("="*80)

print("""
基于以上分析，建议 Hook 以下 D3D11 API:

【关键 API】
1. D3D11CreateDevice
   - 拦截设备创建
   - 记录请求的特性级别
   - 检查返回的设备能力

2. ID3D11Device::CreateTexture2D
   - 检查纹理格式
   - 转换不兼容的格式
   - 记录纹理参数

3. IDXGISwapChain::Present
   - 检查呈现参数
   - 添加同步点
   - 验证颜色空间

4. DXVA2CreateVideoService
   - 检查视频处理器创建参数
   - 验证支持的格式

【次要 API】
5. ID3D11DeviceContext::CopyResource
6. ID3D11DeviceContext::Map/Unmap
7. CreateDXGIFactory1

【调试策略】
- 先实现日志记录，不做修改
- 对比在 RTX 30 和 RTX 50 上的调用差异
- 逐步添加兼容性转换
""")

print("\n" + "="*80)
