#!/usr/bin/env python3
"""
Universal DLL injector - can inject any DLL
"""

import ctypes
from ctypes import wintypes
import sys
import os

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

GetModuleHandleA = kernel32.GetModuleHandleA
GetModuleHandleA.argtypes = [wintypes.LPCSTR]
GetModuleHandleA.restype = wintypes.HMODULE

GetProcAddress = kernel32.GetProcAddress
GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]
GetProcAddress.restype = ctypes.c_void_p

GetExitCodeThread = kernel32.GetExitCodeThread
GetExitCodeThread.argtypes = [wintypes.HANDLE, wintypes.LPDWORD]
GetExitCodeThread.restype = wintypes.BOOL

def inject_dll(pid, dll_path):
    print(f"\n{'='*60}")
    print(f"Universal DLL Injector")
    print(f"{'='*60}\n")

    if not os.path.exists(dll_path):
        print(f"[ERROR] DLL not found: {dll_path}")
        return False

    dll_path = os.path.abspath(dll_path)
    dll_name = os.path.basename(dll_path)
    print(f"DLL: {dll_name}")
    print(f"Path: {dll_path}")
    print(f"Size: {os.path.getsize(dll_path):,} bytes\n")

    # Get LoadLibraryA address
    kernel32_module = GetModuleHandleA(b"kernel32.dll")
    if not kernel32_module:
        print("[ERROR] Cannot get kernel32.dll handle")
        return False

    load_library_addr = GetProcAddress(kernel32_module, b"LoadLibraryA")
    if not load_library_addr:
        print("[ERROR] Cannot get LoadLibraryA address")
        return False

    # Open process
    h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        print(f"[ERROR] Cannot open process (PID {pid})")
        return False

    print(f"Injecting into PID {pid}...\n")

    try:
        # Allocate memory
        dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
        size = len(dll_path_bytes)

        remote_mem = VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote_mem:
            print("[ERROR] Memory allocation failed")
            return False

        # Write DLL path
        bytes_written = ctypes.c_size_t()
        if not WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
            print("[ERROR] Write failed")
            return False

        # Create remote thread
        thread_id = wintypes.DWORD()
        h_thread = CreateRemoteThread(
            h_process,
            None,
            0,
            load_library_addr,
            remote_mem,
            0,
            ctypes.byref(thread_id)
        )

        if not h_thread:
            print("[ERROR] Thread creation failed")
            return False

        print(f"Thread created (TID: {thread_id.value})")

        # Wait for thread
        wait_result = WaitForSingleObject(h_thread, 10000)

        if wait_result == 0:
            print("Thread completed")
            exit_code = wintypes.DWORD()
            if GetExitCodeThread(h_thread, ctypes.byref(exit_code)):
                if exit_code.value == 0:
                    print("[ERROR] LoadLibrary returned NULL")
                    CloseHandle(h_thread)
                    return False
                else:
                    print(f"Module handle: 0x{exit_code.value:016X}")
        elif wait_result == 0x102:
            print("[INFO] Thread timeout (MessageBox waiting?)")
        else:
            print(f"[WARNING] Unexpected wait result: 0x{wait_result:X}")

        CloseHandle(h_thread)
        return True

    finally:
        CloseHandle(h_process)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("\nUsage: python inject_any.py <PID> <DLL_NAME>")
        print("\nExample:")
        print("  python inject_any.py 12345 dmitri_debug2.dll")
        print()
        sys.exit(1)

    pid = int(sys.argv[1])
    dll_name = sys.argv[2]

    # Try build_32bit first, then fallback to build
    dll_path_32 = os.path.join(os.path.dirname(__file__), "build_32bit", "bin", dll_name)
    dll_path_64 = os.path.join(os.path.dirname(__file__), "build", "bin", dll_name)

    if os.path.exists(dll_path_32):
        dll_path = dll_path_32
    elif os.path.exists(dll_path_64):
        dll_path = dll_path_64
    else:
        print(f"\n[ERROR] DLL not found in build_32bit/bin or build/bin")
        sys.exit(1)

    success = inject_dll(pid, dll_path)

    print(f"\n{'='*60}")
    if success:
        print("SUCCESS! Check for MessageBox and logs.")
    else:
        print("FAILED! Check error messages.")
    print(f"{'='*60}\n")
