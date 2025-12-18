import ctypes
from ctypes import wintypes
import sys
import os

PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
PAGE_READWRITE = 0x04

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

OpenProcess = kernel32.OpenProcess
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
OpenProcess.restype = wintypes.HANDLE

VirtualAllocEx = kernel32.VirtualAllocEx
VirtualAllocEx.restype = ctypes.c_void_p

WriteProcessMemory = kernel32.WriteProcessMemory
WriteProcessMemory.restype = wintypes.BOOL

CreateRemoteThread = kernel32.CreateRemoteThread
CreateRemoteThread.restype = wintypes.HANDLE

WaitForSingleObject = kernel32.WaitForSingleObject
GetExitCodeThread = kernel32.GetExitCodeThread
CloseHandle = kernel32.CloseHandle
VirtualFreeEx = kernel32.VirtualFreeEx

GetModuleHandleA = kernel32.GetModuleHandleA
GetModuleHandleA.argtypes = [ctypes.c_char_p]
GetModuleHandleA.restype = ctypes.c_void_p

GetProcAddress = kernel32.GetProcAddress
GetProcAddress.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
GetProcAddress.restype = ctypes.c_void_p

pid = int(sys.argv[1])
dll_name = sys.argv[2]

dll_path = os.path.abspath(os.path.join('build_32bit', 'bin', dll_name))
print(f'DLL: {dll_path}')
print(f'Exists: {os.path.exists(dll_path)}')
print(f'Size: {os.path.getsize(dll_path):,} bytes')
print()

kernel32_module = GetModuleHandleA(b'kernel32.dll')
load_library_addr = GetProcAddress(kernel32_module, b'LoadLibraryA')
print(f'LoadLibraryA: 0x{load_library_addr:X}')

h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
if not h_process:
    print(f'ERROR: Cannot open process (error: {ctypes.get_last_error()})')
    sys.exit(1)

print(f'Opened PID {pid}')

dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
size = len(dll_path_bytes)

remote_mem = VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
if not remote_mem:
    print(f'ERROR: VirtualAllocEx failed (error: {ctypes.get_last_error()})')
    sys.exit(1)

print(f'Allocated at 0x{remote_mem:X}')

bytes_written = ctypes.c_size_t()
if not WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
    print(f'ERROR: WriteProcessMemory failed (error: {ctypes.get_last_error()})')
    sys.exit(1)

print(f'Wrote {bytes_written.value} bytes')

thread_id = wintypes.DWORD()
h_thread = CreateRemoteThread(h_process, None, 0, load_library_addr, remote_mem, 0, ctypes.byref(thread_id))

if not h_thread:
    print(f'ERROR: CreateRemoteThread failed (error: {ctypes.get_last_error()})')
    sys.exit(1)

print(f'Thread created (TID: {thread_id.value})')

WaitForSingleObject(h_thread, 0xFFFFFFFF)

exit_code = wintypes.DWORD()
if GetExitCodeThread(h_thread, ctypes.byref(exit_code)):
    hmodule = exit_code.value
    print(f'\nLoadLibraryA returned: 0x{hmodule:X}')
    if hmodule == 0:
        print('FAILED: LoadLibrary returned NULL')
        print('This usually means DLL dependencies are missing or DllMain failed')
    else:
        print(f'SUCCESS: DLL loaded at 0x{hmodule:X}')

CloseHandle(h_thread)
VirtualFreeEx(h_process, remote_mem, 0, 0x8000)
CloseHandle(h_process)
