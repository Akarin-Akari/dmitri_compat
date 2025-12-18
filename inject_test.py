#!/usr/bin/env python3
"""
Quick test injector for minimal DLL
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

# Function prototypes
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
    print(f"Minimal DLL Test Injector")
    print(f"{'='*60}\n")

    if not os.path.exists(dll_path):
        print(f"[ERROR] DLL not found: {dll_path}")
        return False

    dll_path = os.path.abspath(dll_path)
    print(f"DLL: {dll_path}")
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

    print(f"LoadLibraryA: 0x{load_library_addr:016X}\n")

    # Open process
    h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        print(f"[ERROR] Cannot open process (PID {pid})")
        return False

    print(f"Process opened (PID {pid})\n")

    try:
        # Allocate memory
        dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
        size = len(dll_path_bytes)

        remote_mem = VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote_mem:
            print("[ERROR] Memory allocation failed")
            return False

        print(f"Memory allocated at: 0x{remote_mem:016X}")

        # Write DLL path
        bytes_written = ctypes.c_size_t()
        if not WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
            print("[ERROR] Write failed")
            return False

        print(f"Written {bytes_written.value} bytes\n")

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
        wait_result = WaitForSingleObject(h_thread, 5000)

        if wait_result == 0:
            print("Thread completed\n")

            # Get exit code
            exit_code = wintypes.DWORD()
            if GetExitCodeThread(h_thread, ctypes.byref(exit_code)):
                if exit_code.value == 0:
                    print("[ERROR] LoadLibrary returned NULL")
                    CloseHandle(h_thread)
                    return False
                else:
                    print(f"SUCCESS! Module handle: 0x{exit_code.value:016X}")
            else:
                print("[WARNING] Cannot get exit code")
        elif wait_result == 0x102:
            print("[WARNING] Thread timeout")
        else:
            print(f"[WARNING] Unexpected wait result: 0x{wait_result:X}")

        CloseHandle(h_thread)
        return True

    finally:
        CloseHandle(h_process)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python inject_test.py <PID>")
        sys.exit(1)

    pid = int(sys.argv[1])
    dll_path = os.path.join(os.path.dirname(__file__), "build", "bin", "test_minimal.dll")

    success = inject_dll(pid, dll_path)

    print(f"\n{'='*60}")
    if success:
        print("If you see a MessageBox, the minimal DLL loaded!")
        print("This means the injection works and the problem is")
        print("in the dmitri_compat.dll initialization code.")
    else:
        print("Injection failed - check error messages above")
    print(f"{'='*60}\n")
