#!/usr/bin/env python3
"""
DmitriCompat DLL 注入器 - 完全修复版
修复了 LoadLibraryA 地址获取问题
"""

import ctypes
from ctypes import wintypes
import sys
import os

# ============================================================================
# Windows API 定义
# ============================================================================

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

# 常量
PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
PAGE_READWRITE = 0x04

# 函数原型 - 关键：正确设置返回类型！
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

GetModuleHandleA = kernel32.GetModuleHandleA
GetModuleHandleA.argtypes = [wintypes.LPCSTR]
GetModuleHandleA.restype = wintypes.HMODULE

# 关键修复：GetProcAddress 返回 FARPROC 而不是 LPVOID
GetProcAddress = kernel32.GetProcAddress
GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]
GetProcAddress.restype = ctypes.c_void_p  # 修复：使用 c_void_p

GetExitCodeThread = kernel32.GetExitCodeThread
GetExitCodeThread.argtypes = [wintypes.HANDLE, wintypes.LPDWORD]
GetExitCodeThread.restype = wintypes.BOOL

# ============================================================================
# 注入函数
# ============================================================================

def inject_dll(pid, dll_path):
    """注入 DLL 到目标进程"""

    print(f"\n{'='*70}")
    print(f"  DmitriCompat DLL Injector v3.0 (Fixed)")
    print(f"{'='*70}\n")

    # ========================================
    # [1/9] 检查 DLL 文件
    # ========================================
    if not os.path.exists(dll_path):
        print(f"[1/9] ✗ DLL not found: {dll_path}")
        return False

    dll_path = os.path.abspath(dll_path)
    dll_size = os.path.getsize(dll_path)

    print(f"[1/9] ✓ DLL File")
    print(f"      Path: {dll_path}")
    print(f"      Size: {dll_size:,} bytes ({dll_size/1024:.1f} KB)")

    # 检查运行时库
    dll_dir = os.path.dirname(dll_path)
    runtime_libs = ['libgcc_s_seh-1.dll', 'libstdc++-6.dll']
    for lib in runtime_libs:
        lib_path = os.path.join(dll_dir, lib)
        status = "✓" if os.path.exists(lib_path) else "✗"
        print(f"      {status} Runtime: {lib}")

    # ========================================
    # [2/9] 获取 LoadLibraryA 地址
    # ========================================
    print(f"\n[2/9] Getting LoadLibraryA address...")

    # 获取 kernel32.dll 模块句柄
    kernel32_module = GetModuleHandleA(b"kernel32.dll")
    if not kernel32_module:
        print(f"[2/9] ✗ Cannot get kernel32.dll handle")
        print(f"      Error code: {ctypes.get_last_error()}")
        return False

    print(f"[2/9] ✓ kernel32.dll handle: 0x{kernel32_module:016X}")

    # 获取 LoadLibraryA 地址 - 关键修复！
    load_library_addr = GetProcAddress(kernel32_module, b"LoadLibraryA")

    if not load_library_addr or load_library_addr == 0:
        print(f"[2/9] ✗ Cannot get LoadLibraryA address")
        print(f"      Returned: {load_library_addr}")
        return False

    print(f"[2/9] ✓ LoadLibraryA address: 0x{load_library_addr:016X}")

    # ========================================
    # [3/9] 打开目标进程
    # ========================================
    print(f"\n[3/9] Opening target process (PID {pid})...")

    h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        error = ctypes.get_last_error()
        print(f"[3/9] ✗ Cannot open process")
        print(f"      Error code: {error}")
        print(f"      Hint: Try running as Administrator")
        return False

    print(f"[3/9] ✓ Process opened (handle: 0x{h_process:X})")

    try:
        # ========================================
        # [4/9] 在目标进程中分配内存
        # ========================================
        dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
        size = len(dll_path_bytes)

        print(f"\n[4/9] Allocating memory in target process...")
        print(f"      Size: {size} bytes")

        remote_mem = VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote_mem:
            print(f"[4/9] ✗ Memory allocation failed")
            print(f"      Error code: {ctypes.get_last_error()}")
            return False

        print(f"[4/9] ✓ Memory allocated at: 0x{remote_mem:016X}")

        # ========================================
        # [5/9] 写入 DLL 路径
        # ========================================
        print(f"\n[5/9] Writing DLL path to remote memory...")

        bytes_written = ctypes.c_size_t()
        if not WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
            print(f"[5/9] ✗ Write failed")
            print(f"      Error code: {ctypes.get_last_error()}")
            return False

        print(f"[5/9] ✓ Written {bytes_written.value}/{size} bytes")

        # ========================================
        # [6/9] 创建远程线程
        # ========================================
        print(f"\n[6/9] Creating remote thread...")
        print(f"      Entry point: 0x{load_library_addr:016X} (LoadLibraryA)")
        print(f"      Parameter: 0x{remote_mem:016X} (DLL path)")

        thread_id = wintypes.DWORD()
        h_thread = CreateRemoteThread(
            h_process,
            None,
            0,
            load_library_addr,  # 现在这个地址应该是正确的！
            remote_mem,
            0,
            ctypes.byref(thread_id)
        )

        if not h_thread:
            error = ctypes.get_last_error()
            print(f"[6/9] ✗ Thread creation failed")
            print(f"      Error code: {error}")
            return False

        print(f"[6/9] ✓ Thread created (TID: {thread_id.value})")

        # ========================================
        # [7/9] 等待线程完成
        # ========================================
        print(f"\n[7/9] Waiting for DLL to load...")

        wait_result = WaitForSingleObject(h_thread, 10000)  # 10秒超时

        if wait_result == 0:  # WAIT_OBJECT_0
            print(f"[7/9] ✓ Thread completed")
        elif wait_result == 0x102:  # WAIT_TIMEOUT
            print(f"[7/9] ⚠ Thread timeout (still loading?)")
        else:
            print(f"[7/9] ⚠ Unexpected wait result: 0x{wait_result:X}")

        # ========================================
        # [8/9] 检查线程退出码
        # ========================================
        print(f"\n[8/9] Checking LoadLibrary result...")

        exit_code = wintypes.DWORD()
        if GetExitCodeThread(h_thread, ctypes.byref(exit_code)):
            if exit_code.value == 0:
                print(f"[8/9] ✗ LoadLibrary failed (returned NULL)")
                print(f"      Possible reasons:")
                print(f"        - DLL dependencies missing")
                print(f"        - DLL architecture mismatch (x86 vs x64)")
                print(f"        - DLL initialization failed")
                CloseHandle(h_thread)
                return False
            else:
                print(f"[8/9] ✓ LoadLibrary succeeded")
                print(f"      Module handle: 0x{exit_code.value:016X}")
        else:
            print(f"[8/9] ⚠ Cannot get exit code")

        CloseHandle(h_thread)

        # ========================================
        # [9/9] 最终验证
        # ========================================
        print(f"\n[9/9] Verification...")
        print(f"[9/9] ✓ Injection process completed")

        return True

    except Exception as e:
        print(f"\n✗ Exception occurred: {e}")
        import traceback
        traceback.print_exc()
        return False

    finally:
        CloseHandle(h_process)

# ============================================================================
# 主函数
# ============================================================================

def main():
    print("\n" + "="*70)
    print("  DmitriCompat - RTX 50 Compatibility Layer")
    print("  DLL Injection Tool v3.0")
    print("="*70)

    if len(sys.argv) < 2:
        print("\nUsage:")
        print("  python inject_fixed.py <PID>")
        print("\nExample:")
        print("  python inject_fixed.py 12345")
        print()
        return

    try:
        pid = int(sys.argv[1])
    except ValueError:
        print(f"\n✗ Invalid PID: {sys.argv[1]}")
        return

    # DLL 路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.join(script_dir, "build", "bin", "dmitri_compat.dll")

    # 注入
    success = inject_dll(pid, dll_path)

    # 结果
    print(f"\n{'='*70}")
    if success:
        print("  ✓✓✓ SUCCESS! DLL INJECTED SUCCESSFULLY ✓✓✓")
        print(f"{'='*70}\n")
        print("Next steps:")
        print("  1. Play a video in PotPlayer")
        print("  2. Seek or pause/resume to trigger D3D11 calls")
        print("  3. Check logs: build/bin/logs/dmitri_compat.log")
        print("  4. Look for:")
        print("     - D3D11CreateDevice Called")
        print("     - CreateTexture2D calls")
        print("     - Video format textures (NV12, YUY2)")
        print()
    else:
        print("  ✗✗✗ FAILED! INJECTION FAILED ✗✗✗")
        print(f"{'='*70}\n")
        print("Troubleshooting:")
        print("  - Make sure you run as Administrator")
        print("  - Check that PID is correct")
        print("  - Ensure target process is 64-bit")
        print("  - Check that runtime DLLs exist")
        print()

if __name__ == "__main__":
    main()
