#!/usr/bin/env python3
"""
改进的 DLL 注入器 - 带详细错误检查
"""

import ctypes
from ctypes import wintypes
import sys
import os

# Windows API 定义
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
PAGE_READWRITE = 0x04

def inject_dll(pid, dll_path):
    print(f"\n{'='*60}")
    print(f"DmitriCompat DLL Injector")
    print(f"{'='*60}\n")

    # 检查 DLL 文件
    if not os.path.exists(dll_path):
        print(f"[ERROR] DLL not found: {dll_path}")
        return False

    dll_path = os.path.abspath(dll_path)
    print(f"[1/8] DLL Path: {dll_path}")
    print(f"[1/8] DLL Size: {os.path.getsize(dll_path)} bytes")

    # 检查运行时库
    dll_dir = os.path.dirname(dll_path)
    runtime_libs = ['libgcc_s_seh-1.dll', 'libstdc++-6.dll']
    for lib in runtime_libs:
        lib_path = os.path.join(dll_dir, lib)
        if os.path.exists(lib_path):
            print(f"[1/8] Runtime: {lib} - OK")
        else:
            print(f"[1/8] Runtime: {lib} - MISSING!")

    # 打开进程
    print(f"\n[2/8] Opening process PID={pid}...")
    h_process = kernel32.OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        error = ctypes.get_last_error()
        print(f"[ERROR] Cannot open process (error code: {error})")
        print("[HINT] Try running as Administrator")
        return False
    print("[2/8] Process opened successfully")

    try:
        # 分配内存
        dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
        size = len(dll_path_bytes)

        print(f"\n[3/8] Allocating {size} bytes in target process...")
        remote_mem = kernel32.VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote_mem:
            print("[ERROR] Cannot allocate memory")
            return False
        print(f"[3/8] Memory allocated at 0x{remote_mem:X}")

        # 写入 DLL 路径
        print(f"\n[4/8] Writing DLL path to remote memory...")
        bytes_written = ctypes.c_size_t()
        if not kernel32.WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
            print("[ERROR] Cannot write to remote memory")
            return False
        print(f"[4/8] Written {bytes_written.value} bytes")

        # 获取 LoadLibraryA 地址 - 修复版本
        print(f"\n[5/8] Getting LoadLibraryA address...")
        kernel32_handle = kernel32.GetModuleHandleW("kernel32.dll")
        if not kernel32_handle:
            print("[ERROR] Cannot get kernel32.dll handle")
            return False
        print(f"[5/8] kernel32.dll handle: 0x{kernel32_handle:X}")

        # 使用 GetProcAddress 的正确方式
        load_library_addr = kernel32.GetProcAddress(kernel32_handle, b"LoadLibraryA")
        if not load_library_addr:
            print("[ERROR] Cannot get LoadLibraryA address")
            return False
        print(f"[5/8] LoadLibraryA address: 0x{load_library_addr:X}")

        # 创建远程线程
        print(f"\n[6/8] Creating remote thread...")
        thread_id = wintypes.DWORD()
        h_thread = kernel32.CreateRemoteThread(
            h_process,
            None,
            0,
            load_library_addr,
            remote_mem,
            0,
            ctypes.byref(thread_id)
        )

        if not h_thread:
            error = ctypes.get_last_error()
            print(f"[ERROR] Cannot create remote thread (error code: {error})")
            return False
        print(f"[6/8] Remote thread created (TID: {thread_id.value})")

        # 等待线程完成
        print(f"\n[7/8] Waiting for DLL to load...")
        wait_result = kernel32.WaitForSingleObject(h_thread, 10000)  # 10秒超时

        if wait_result == 0:  # WAIT_OBJECT_0
            print("[7/8] Thread completed")

            # 获取线程退出码（LoadLibrary 的返回值，即模块句柄）
            exit_code = wintypes.DWORD()
            kernel32.GetExitCodeThread(h_thread, ctypes.byref(exit_code))

            if exit_code.value == 0:
                print("[ERROR] LoadLibrary returned NULL - DLL load failed!")
                print("[HINT] Check DLL dependencies and compatibility")
                kernel32.CloseHandle(h_thread)
                return False
            else:
                print(f"[7/8] DLL loaded successfully! (Module handle: 0x{exit_code.value:X})")
        elif wait_result == 0x102:  # WAIT_TIMEOUT
            print("[WARNING] Thread timeout - DLL might still be loading")
        else:
            print(f"[WARNING] Unexpected wait result: {wait_result}")

        kernel32.CloseHandle(h_thread)

        # 最终验证
        print(f"\n[8/8] Verification...")
        return True

    finally:
        kernel32.CloseHandle(h_process)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python inject_v2.py <PID>")
        sys.exit(1)

    pid = int(sys.argv[1])
    dll_path = os.path.join(os.path.dirname(__file__), "build", "bin", "dmitri_compat.dll")

    if inject_dll(pid, dll_path):
        print(f"\n{'='*60}")
        print("SUCCESS! DLL injected successfully!")
        print(f"{'='*60}\n")
        print("Next steps:")
        print("1. Play a video in PotPlayer")
        print("2. Check logs: build/bin/logs/dmitri_compat.log")
        print("3. Look for D3D11 API calls\n")
    else:
        print(f"\n{'='*60}")
        print("FAILED! DLL injection failed")
        print(f"{'='*60}\n")
        sys.exit(1)
