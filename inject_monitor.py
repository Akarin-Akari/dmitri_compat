"""
DLL 自动注入工具
监控 PotPlayerMini64.exe 和 drtm.exe 进程，自动注入 libdmitri_late_hook.dll

使用方法:
    python inject_monitor.py

依赖:
    pip install psutil pywin32

作者: DmitriCompat 自动化测试工具
"""

import os
import sys
import time
import ctypes
import struct
from ctypes import wintypes
import psutil

# ============================================================================
# Windows API 定义
# ============================================================================

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

# 常量
PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
PAGE_READWRITE = 0x04

# 函数原型
OpenProcess = kernel32.OpenProcess
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
OpenProcess.restype = wintypes.HANDLE

VirtualAllocEx = kernel32.VirtualAllocEx
VirtualAllocEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]
VirtualAllocEx.restype = wintypes.LPVOID

WriteProcessMemory = kernel32.WriteProcessMemory
WriteProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
WriteProcessMemory.restype = wintypes.BOOL

GetModuleHandleW = kernel32.GetModuleHandleW
GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
GetModuleHandleW.restype = wintypes.HMODULE

GetProcAddress = kernel32.GetProcAddress
GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]
GetProcAddress.restype = ctypes.c_void_p

CreateRemoteThread = kernel32.CreateRemoteThread
CreateRemoteThread.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, wintypes.LPDWORD]
CreateRemoteThread.restype = wintypes.HANDLE

WaitForSingleObject = kernel32.WaitForSingleObject
WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
WaitForSingleObject.restype = wintypes.DWORD

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

VirtualFreeEx = kernel32.VirtualFreeEx
VirtualFreeEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD]
VirtualFreeEx.restype = wintypes.BOOL

MEM_RELEASE = 0x8000

# ============================================================================
# DLL 注入逻辑
# ============================================================================

def inject_dll(pid: int, dll_path: str) -> bool:
    """
    将 DLL 注入到指定进程
    
    Args:
        pid: 目标进程 ID
        dll_path: DLL 的完整路径
        
    Returns:
        是否注入成功
    """
    # 确保路径使用绝对路径
    dll_path = os.path.abspath(dll_path)
    
    if not os.path.exists(dll_path):
        print(f"❌ DLL 文件不存在: {dll_path}")
        return False
    
    # 转换为 bytes (使用 UTF-16 编码，因为 Windows 使用 Wide String)
    dll_path_bytes = (dll_path + '\0').encode('utf-16-le')
    
    print(f"🔧 正在注入 DLL 到 PID {pid}...")
    print(f"   DLL 路径: {dll_path}")
    
    # 打开目标进程
    h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        error = ctypes.get_last_error()
        print(f"❌ 无法打开进程 (错误: {error})")
        print("   💡 提示: 尝试以管理员身份运行此脚本")
        return False
    
    try:
        # 在目标进程中分配内存
        remote_memory = VirtualAllocEx(
            h_process,
            None,
            len(dll_path_bytes),
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        )
        
        if not remote_memory:
            print(f"❌ 无法在目标进程分配内存")
            return False
        
        print(f"   📦 在目标进程分配内存: 0x{remote_memory:X}")
        
        # 将 DLL 路径写入目标进程
        bytes_written = ctypes.c_size_t(0)
        if not WriteProcessMemory(
            h_process,
            remote_memory,
            dll_path_bytes,
            len(dll_path_bytes),
            ctypes.byref(bytes_written)
        ):
            print(f"❌ 无法写入进程内存")
            VirtualFreeEx(h_process, remote_memory, 0, MEM_RELEASE)
            return False
        
        print(f"   ✍️ 写入 {bytes_written.value} 字节")
        
        # 获取 LoadLibraryW 地址
        h_kernel32 = GetModuleHandleW("kernel32.dll")
        if not h_kernel32:
            print(f"❌ 无法获取 kernel32.dll 句柄")
            VirtualFreeEx(h_process, remote_memory, 0, MEM_RELEASE)
            return False
        
        load_library_addr = GetProcAddress(h_kernel32, b"LoadLibraryW")
        if not load_library_addr:
            print(f"❌ 无法获取 LoadLibraryW 地址")
            VirtualFreeEx(h_process, remote_memory, 0, MEM_RELEASE)
            return False
        
        print(f"   📍 LoadLibraryW 地址: 0x{load_library_addr:X}")
        
        # 创建远程线程执行 LoadLibraryW
        thread_id = wintypes.DWORD(0)
        h_thread = CreateRemoteThread(
            h_process,
            None,
            0,
            load_library_addr,
            remote_memory,
            0,
            ctypes.byref(thread_id)
        )
        
        if not h_thread:
            error = ctypes.get_last_error()
            print(f"❌ 无法创建远程线程 (错误: {error})")
            VirtualFreeEx(h_process, remote_memory, 0, MEM_RELEASE)
            return False
        
        print(f"   🧵 创建远程线程: TID {thread_id.value}")
        
        # 等待线程完成
        WaitForSingleObject(h_thread, 5000)  # 最多等待 5 秒
        CloseHandle(h_thread)
        
        # 释放内存
        VirtualFreeEx(h_process, remote_memory, 0, MEM_RELEASE)
        
        print(f"✅ DLL 注入成功!")
        return True
        
    finally:
        CloseHandle(h_process)

# ============================================================================
# 进程监控
# ============================================================================

class ProcessMonitor:
    """进程监控器"""
    
    def __init__(self, dll_path: str, target_processes: list):
        self.dll_path = os.path.abspath(dll_path)
        self.target_processes = [p.lower() for p in target_processes]
        self.injected_pids = set()  # 已注入的 PID
        
    def find_target_processes(self) -> list:
        """查找目标进程"""
        found = []
        for proc in psutil.process_iter(['pid', 'name']):
            try:
                name = proc.info['name'].lower()
                if name in self.target_processes:
                    found.append({
                        'pid': proc.info['pid'],
                        'name': proc.info['name']
                    })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        return found
    
    def run(self, poll_interval: float = 1.0, inject_delay: float = 2.0):
        """
        开始监控
        
        Args:
            poll_interval: 轮询间隔 (秒)
            inject_delay: 发现进程后延迟注入时间 (秒)
        """
        print("=" * 60)
        print("🎯 DmitriCompat DLL 自动注入工具")
        print("=" * 60)
        print(f"📁 DLL 路径: {self.dll_path}")
        print(f"🔍 监控进程: {', '.join(self.target_processes)}")
        print(f"⏱️ 轮询间隔: {poll_interval} 秒")
        print(f"⏳ 注入延迟: {inject_delay} 秒")
        print("=" * 60)
        print()
        
        if not os.path.exists(self.dll_path):
            print(f"❌ 错误: DLL 文件不存在!")
            print(f"   请检查路径: {self.dll_path}")
            return
        
        print("🚀 开始监控... (Ctrl+C 停止)")
        print()
        
        try:
            while True:
                processes = self.find_target_processes()
                
                for proc in processes:
                    pid = proc['pid']
                    name = proc['name']
                    
                    if pid not in self.injected_pids:
                        print(f"\n🎯 发现目标进程: {name} (PID: {pid})")
                        print(f"   ⏳ 等待 {inject_delay} 秒后注入...")
                        time.sleep(inject_delay)
                        
                        # 再次检查进程是否还存在
                        if psutil.pid_exists(pid):
                            if inject_dll(pid, self.dll_path):
                                self.injected_pids.add(pid)
                                print(f"   💾 日志位置: dmitri_compat\\build\\bin\\logs\\dmitri_compat.log")
                            else:
                                print(f"   ⚠️ 注入失败，将在下次轮询重试")
                        else:
                            print(f"   ⚠️ 进程已退出")
                
                # 清理已退出的进程
                active_pids = {p['pid'] for p in processes}
                exited = self.injected_pids - active_pids
                if exited:
                    for pid in exited:
                        print(f"📤 进程 PID {pid} 已退出，从注入列表移除")
                    self.injected_pids -= exited
                
                # 状态显示 (每 10 秒)
                if int(time.time()) % 10 == 0:
                    if self.injected_pids:
                        print(f"📊 [状态] 已注入 {len(self.injected_pids)} 个进程: {self.injected_pids}")
                    else:
                        print(f"⏳ [状态] 等待目标进程启动...")
                
                time.sleep(poll_interval)
                
        except KeyboardInterrupt:
            print("\n\n🛑 监控已停止")
            print(f"📊 本次会话共注入 {len(self.injected_pids)} 个进程")

# ============================================================================
# 主入口
# ============================================================================

def main():
    # 配置
    DLL_PATH = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "dmitri_compat", "build", "bin", "libdmitri_late_hook.dll"
    )
    
    TARGET_PROCESSES = [
        "PotPlayerMini64.exe",
        "drtm.exe"
    ]
    
    # 命令行参数覆盖
    if len(sys.argv) > 1:
        DLL_PATH = sys.argv[1]
    
    # 检查管理员权限
    try:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin()
    except:
        is_admin = False
    
    if not is_admin:
        print("⚠️ 警告: 未以管理员身份运行，注入可能失败！")
        print("   请右键 -> 以管理员身份运行")
        print()
    
    # 启动监控
    monitor = ProcessMonitor(DLL_PATH, TARGET_PROCESSES)
    monitor.run(poll_interval=1.0, inject_delay=3.0)

if __name__ == "__main__":
    main()
