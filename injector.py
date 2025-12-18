#!/usr/bin/env python3
"""
DmitriCompat DLL 注入器
使用此工具将 dmitri_compat.dll 注入到使用 DmitriRender 的播放器进程中
"""

import sys
import os
import ctypes
from ctypes import wintypes
import time

# Windows API 定义
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
PAGE_READWRITE = 0x04

OpenProcess = kernel32.OpenProcess
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
OpenProcess.restype = wintypes.HANDLE

VirtualAllocEx = kernel32.VirtualAllocEx
VirtualAllocEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]
VirtualAllocEx.restype = wintypes.LPVOID

WriteProcessMemory = kernel32.WriteProcessMemory
WriteProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
WriteProcessMemory.restype = wintypes.BOOL

CreateRemoteThread = kernel32.CreateRemoteThread
CreateRemoteThread.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, wintypes.LPDWORD]
CreateRemoteThread.restype = wintypes.HANDLE

WaitForSingleObject = kernel32.WaitForSingleObject
WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
WaitForSingleObject.restype = wintypes.DWORD

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

GetModuleHandleW = kernel32.GetModuleHandleW
GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
GetModuleHandleW.restype = wintypes.HMODULE

GetProcAddress = kernel32.GetProcAddress
GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]
GetProcAddress.restype = wintypes.LPVOID


def inject_dll(pid, dll_path):
    """注入 DLL 到目标进程"""

    # 检查 DLL 文件
    if not os.path.exists(dll_path):
        print(f"错误: DLL 文件不存在: {dll_path}")
        return False

    dll_path = os.path.abspath(dll_path)
    print(f"DLL 路径: {dll_path}")

    # 打开目标进程
    print(f"打开进程 PID={pid}...")
    h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        error = ctypes.get_last_error()
        print(f"错误: 无法打开进程 (错误代码: {error})")
        print("提示: 请以管理员权限运行此脚本")
        return False

    try:
        # 在目标进程中分配内存
        dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
        size = len(dll_path_bytes)

        print(f"在目标进程中分配 {size} 字节内存...")
        remote_mem = VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote_mem:
            print("错误: 无法分配远程内存")
            return False

        # 写入 DLL 路径
        print("写入 DLL 路径...")
        bytes_written = ctypes.c_size_t()
        if not WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
            print("错误: 无法写入远程内存")
            return False

        # 获取 LoadLibraryA 地址
        print("获取 LoadLibraryA 地址...")
        kernel32_handle = GetModuleHandleW("kernel32.dll")
        load_library_addr = GetProcAddress(kernel32_handle, b"LoadLibraryA")
        if not load_library_addr:
            print("错误: 无法获取 LoadLibraryA 地址")
            return False

        # 创建远程线程
        print("创建远程线程...")
        h_thread = CreateRemoteThread(h_process, None, 0, load_library_addr, remote_mem, 0, None)
        if not h_thread:
            print("错误: 无法创建远程线程")
            return False

        # 等待线程完成
        print("等待注入完成...")
        WaitForSingleObject(h_thread, 5000)  # 5 秒超时
        CloseHandle(h_thread)

        print("✓ DLL 注入成功!")
        return True

    finally:
        CloseHandle(h_process)


def find_process_by_name(process_name):
    """通过进程名查找 PID"""
    import psutil

    for proc in psutil.process_iter(['pid', 'name']):
        try:
            if proc.info['name'].lower() == process_name.lower():
                return proc.info['pid']
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    return None


def main():
    print("=" * 60)
    print("DmitriCompat DLL 注入器")
    print("=" * 60)
    print()

    # 检查参数
    if len(sys.argv) < 2:
        print("用法:")
        print("  方式1 (通过 PID): injector.py <PID>")
        print("  方式2 (通过进程名): injector.py <进程名.exe>")
        print()
        print("示例:")
        print("  injector.py 12345")
        print("  injector.py PotPlayerMini64.exe")
        print()
        return

    # 确定 PID
    arg = sys.argv[1]
    if arg.isdigit():
        pid = int(arg)
    else:
        # 尝试通过进程名查找
        try:
            import psutil
            pid = find_process_by_name(arg)
            if not pid:
                print(f"错误: 找不到进程 '{arg}'")
                print("\n当前运行的进程:")
                for proc in psutil.process_iter(['pid', 'name']):
                    try:
                        print(f"  [{proc.info['pid']}] {proc.info['name']}")
                    except:
                        pass
                return
            print(f"找到进程: {arg} (PID={pid})")
        except ImportError:
            print("错误: 需要安装 psutil 库")
            print("运行: pip install psutil")
            return

    # DLL 路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.join(script_dir, "build", "bin", "dmitri_compat.dll")

    if not os.path.exists(dll_path):
        dll_path = os.path.join(script_dir, "dmitri_compat.dll")

    if not os.path.exists(dll_path):
        print("错误: 找不到 dmitri_compat.dll")
        print("请先运行 build.bat 构建项目")
        return

    # 注入
    print()
    if inject_dll(pid, dll_path):
        print()
        print("=" * 60)
        print("注入成功!")
        print("=" * 60)
        print()
        print("接下来:")
        print("1. 在播放器中打开一个视频")
        print("2. 检查日志文件: build/bin/logs/dmitri_compat.log")
        print("3. 查看是否有 D3D11 API 调用记录")
        print()
    else:
        print()
        print("注入失败!")
        print()


if __name__ == "__main__":
    main()
