#!/usr/bin/env python3
"""
DmitriRender DLL 分析工具
分析 DLL 的依赖、导入函数和关键信息
"""

import pefile
import sys
import os

# 设置UTF-8编码输出
import io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

def analyze_dll(dll_path):
    """分析 DLL 文件"""
    if not os.path.exists(dll_path):
        print(f"错误: 文件不存在: {dll_path}")
        return

    print(f"\n{'='*80}")
    print(f"分析文件: {dll_path}")
    print(f"{'='*80}\n")

    try:
        pe = pefile.PE(dll_path)

        # 基本信息
        print("【基本信息】")
        print(f"Machine Type: {hex(pe.FILE_HEADER.Machine)}")
        if pe.FILE_HEADER.Machine == 0x8664:
            print("  -> x64 架构")
        elif pe.FILE_HEADER.Machine == 0x14c:
            print("  -> x86 架构")

        print(f"时间戳: {pe.FILE_HEADER.TimeDateStamp}")
        print(f"节数量: {pe.FILE_HEADER.NumberOfSections}")

        # 导入的 DLL
        print("\n【导入的 DLL】")
        if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
            for entry in pe.DIRECTORY_ENTRY_IMPORT:
                dll_name = entry.dll.decode('utf-8')
                print(f"\n{dll_name}")

                # 识别关键 DLL
                dll_lower = dll_name.lower()
                if 'd3d' in dll_lower:
                    print("  ⚠️  DirectX 相关")
                if 'cuda' in dll_lower or 'nvcuda' in dll_lower:
                    print("  ⚠️  CUDA 相关")
                if 'opencl' in dll_lower:
                    print("  ⚠️  OpenCL 相关")
                if 'nvapi' in dll_lower:
                    print("  ⚠️  NVIDIA API 相关")

                # 列出前10个导入函数
                funcs = []
                for imp in entry.imports:
                    if imp.name:
                        funcs.append(imp.name.decode('utf-8'))
                    else:
                        funcs.append(f"Ordinal:{imp.ordinal}")

                if len(funcs) <= 10:
                    for func in funcs:
                        print(f"  - {func}")
                else:
                    print(f"  导入了 {len(funcs)} 个函数 (显示前10个):")
                    for func in funcs[:10]:
                        print(f"  - {func}")
                    print(f"  ... 还有 {len(funcs) - 10} 个函数")
        else:
            print("  无导入表")

        # 导出函数
        print("\n【导出的函数】")
        if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
            exports = []
            for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                if exp.name:
                    exports.append(exp.name.decode('utf-8'))
                else:
                    exports.append(f"Ordinal:{exp.ordinal}")

            print(f"导出了 {len(exports)} 个函数:")
            for exp_name in exports[:20]:  # 显示前20个
                print(f"  - {exp_name}")
            if len(exports) > 20:
                print(f"  ... 还有 {len(exports) - 20} 个函数")
        else:
            print("  无导出表")

        # 节信息
        print("\n【节信息】")
        for section in pe.sections:
            name = section.Name.decode('utf-8').rstrip('\x00')
            print(f"{name:10s} - 虚拟大小: {section.Misc_VirtualSize:8d}, 原始大小: {section.SizeOfRawData:8d}")

        # 资源信息（版本信息）
        if hasattr(pe, 'VS_VERSIONINFO'):
            print("\n【版本信息】")
            if hasattr(pe, 'FileInfo'):
                for fileinfo in pe.FileInfo:
                    if hasattr(fileinfo, 'StringTable'):
                        for st in fileinfo.StringTable:
                            for entry in st.entries.items():
                                print(f"  {entry[0].decode('utf-8')}: {entry[1].decode('utf-8')}")

        print("\n" + "="*80 + "\n")

    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    # 分析所有相关 DLL
    dlls = [
        "x64/dmitriRender.dll",
        "x64/dmitriRenderBase.dll",
        "x64/oqorimis.dll"
    ]

    for dll in dlls:
        analyze_dll(dll)
