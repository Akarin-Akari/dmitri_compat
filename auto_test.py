#!/usr/bin/env python3
"""
DmitriCompat 自动测试脚本 v2.0
=====================================
1. 轮询检测 PotPlayer 进程
2. 等待进程稳定后再注入
3. 自动处理崩溃和重试
4. 详细的日志记录

使用方法:
    以管理员权限运行:
    python auto_test.py

    [可选参数]
    --delay N       注入前等待N秒 (默认3秒)
    --dll PATH      指定DLL路径
    --no-wait       检测到进程立即注入(不推荐)
"""

import ctypes
from ctypes import wintypes
import subprocess
import time
import os
import sys
import argparse
from datetime import datetime

# ============================================================================
# 配置
# ============================================================================
# 注意: drtm.exe 是 DmitriRender 管理工具，不需要注入
# 我们只注入到使用 DmitriRender 滤镜的播放器进程
TARGET_PROCESSES = ['PotPlayerMini64.exe']
DEFAULT_INJECTION_DELAY = 3.0  # 等待进程稳定的时间
CHECK_INTERVAL = 0.5  # 检测间隔
MAX_CRASH_COUNT = 3   # 最大崩溃次数后停止

# ============================================================================
# Windows API 定义
# ============================================================================
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

# ============================================================================
# 日志
# ============================================================================
def log(msg, level="INFO"):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    colors = {
        "INFO": "\033[92m",      # 绿色
        "WARN": "\033[93m",      # 黄色
        "ERROR": "\033[91m",     # 红色
        "DEBUG": "\033[94m",     # 蓝色
        "SUCCESS": "\033[96m",   # 青色
    }
    reset = "\033[0m"
    color = colors.get(level, "")
    print(f"{color}[{timestamp}] [{level}] {msg}{reset}")

# ============================================================================
# 进程管理
# ============================================================================
def get_target_processes():
    """获取所有目标播放器进程"""
    processes = []
    try:
        result = subprocess.run(
            ['tasklist', '/FO', 'CSV', '/NH'],
            capture_output=True,
            text=True,
            encoding='gbk',
            errors='replace',
            timeout=5
        )
        
        for line in result.stdout.strip().split('\n'):
            for target in TARGET_PROCESSES:
                if target.lower() in line.lower():
                    parts = line.split('","')
                    if len(parts) >= 2:
                        name = parts[0].strip('"')
                        pid_str = parts[1].strip('"')
                        try:
                            processes.append({
                                'name': name,
                                'pid': int(pid_str)
                            })
                        except ValueError:
                            pass
    except Exception as e:
        log(f"获取进程列表失败: {e}", "ERROR")
    
    return processes

def is_process_running(pid):
    """检查进程是否还在运行"""
    try:
        result = subprocess.run(
            ['tasklist', '/FI', f'PID eq {pid}', '/FO', 'CSV', '/NH'],
            capture_output=True,
            text=True,
            encoding='gbk',
            errors='replace',
            timeout=5
        )
        return str(pid) in result.stdout
    except:
        return False

def wait_for_process_stable(pid, timeout=5.0):
    """等待进程稳定(不再频繁变化)"""
    log(f"等待进程 {pid} 稳定...", "DEBUG")
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if not is_process_running(pid):
            log(f"进程 {pid} 已退出", "WARN")
            return False
        time.sleep(0.5)
    
    return True

# ============================================================================
# DLL 注入
# ============================================================================
def inject_dll(pid, dll_path):
    """注入 DLL 到目标进程"""
    log(f"开始注入到 PID {pid}...", "INFO")
    log(f"DLL: {dll_path}", "DEBUG")
    
    if not os.path.exists(dll_path):
        log(f"DLL 文件不存在: {dll_path}", "ERROR")
        return False
    
    dll_path = os.path.abspath(dll_path)
    
    # 获取 LoadLibraryA 地址
    kernel32_module = GetModuleHandleA(b"kernel32.dll")
    if not kernel32_module:
        log("无法获取 kernel32.dll 句柄", "ERROR")
        return False
    
    load_library_addr = GetProcAddress(kernel32_module, b"LoadLibraryA")
    if not load_library_addr:
        log("无法获取 LoadLibraryA 地址", "ERROR")
        return False
    
    log(f"LoadLibraryA @ 0x{load_library_addr:016X}", "DEBUG")
    
    # 打开目标进程
    h_process = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        error = ctypes.get_last_error()
        log(f"无法打开进程 (错误代码: {error})", "ERROR")
        log("提示: 请以管理员权限运行此脚本", "WARN")
        return False
    
    try:
        # 分配内存
        dll_path_bytes = dll_path.encode('utf-8') + b'\x00'
        size = len(dll_path_bytes)
        
        remote_mem = VirtualAllocEx(h_process, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote_mem:
            log("无法分配远程内存", "ERROR")
            return False
        
        log(f"已分配远程内存 @ 0x{remote_mem:016X}", "DEBUG")
        
        # 写入 DLL 路径
        bytes_written = ctypes.c_size_t()
        if not WriteProcessMemory(h_process, remote_mem, dll_path_bytes, size, ctypes.byref(bytes_written)):
            log("无法写入远程内存", "ERROR")
            return False
        
        # 创建远程线程
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
            log("无法创建远程线程", "ERROR")
            return False
        
        log(f"远程线程已创建 (TID: {thread_id.value})", "DEBUG")
        
        # 等待线程完成
        wait_result = WaitForSingleObject(h_thread, 10000)  # 10秒超时
        
        if wait_result == 0:  # WAIT_OBJECT_0
            # 检查退出码
            exit_code = wintypes.DWORD()
            if GetExitCodeThread(h_thread, ctypes.byref(exit_code)):
                if exit_code.value == 0:
                    log("LoadLibrary 返回 NULL - DLL 加载失败", "ERROR")
                    log("可能原因: 缺少依赖、架构不匹配、DLL初始化失败", "WARN")
                    CloseHandle(h_thread)
                    return False
                else:
                    log(f"DLL 已加载! 模块句柄: 0x{exit_code.value:08X}", "SUCCESS")
        elif wait_result == 0x102:  # WAIT_TIMEOUT
            log("等待超时 - DLL 可能仍在加载", "WARN")
        else:
            log(f"等待失败: 0x{wait_result:X}", "ERROR")
            CloseHandle(h_thread)
            return False
        
        CloseHandle(h_thread)
        return True
        
    except Exception as e:
        log(f"注入异常: {e}", "ERROR")
        import traceback
        traceback.print_exc()
        return False
    finally:
        CloseHandle(h_process)

# ============================================================================
# 主循环
# ============================================================================
def main():
    parser = argparse.ArgumentParser(description='DmitriCompat 自动测试脚本')
    parser.add_argument('--delay', type=float, default=DEFAULT_INJECTION_DELAY, 
                        help=f'注入前等待秒数 (默认: {DEFAULT_INJECTION_DELAY})')
    parser.add_argument('--dll', type=str, default=None, help='指定 DLL 路径')
    parser.add_argument('--no-wait', action='store_true', help='检测到进程立即注入')
    parser.add_argument('--once', action='store_true', help='只注入一次后退出')
    args = parser.parse_args()
    
    # 确定 DLL 路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    if args.dll:
        dll_path = args.dll
    else:
        # 按优先级尝试不同的 DLL (Late Hook 版本优先！)
        dll_candidates = [
            os.path.join(script_dir, "build", "bin", "libdmitri_late_hook.dll"),  # 新的 Late Hook 版本
            os.path.join(script_dir, "build", "bin", "dmitri_late_hook.dll"),
            os.path.join(script_dir, "build", "bin", "dmitri_compat.dll"),
            os.path.join(script_dir, "build", "bin", "dmitri_compat_working.dll"),
        ]
        dll_path = None
        for candidate in dll_candidates:
            if os.path.exists(candidate):
                dll_path = candidate
                break
        
        if not dll_path:
            log("找不到 DLL 文件!", "ERROR")
            log("请先运行 build.bat 构建项目", "ERROR")
            sys.exit(1)
    
    # 检查管理员权限
    try:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin()
    except:
        is_admin = False
    
    print("\n" + "=" * 70)
    print("  🎮 DmitriCompat 自动测试工具 v2.0")
    print("=" * 70)
    print(f"  目标进程: {', '.join(TARGET_PROCESSES)}")
    print(f"  DLL 文件: {dll_path}")
    print(f"  注入延迟: {args.delay} 秒")
    print(f"  管理员模式: {'✓ 是' if is_admin else '✗ 否 (可能注入失败!)'}")
    print("=" * 70)
    print("\n操作说明:")
    print("  1. 打开 PotPlayer 并加载视频")
    print("  2. 启用 DmitriRender 滤镜")
    print("  3. 等待自动注入...")
    print("  4. 检查日志: build/bin/logs/dmitri_compat.log")
    print("\n按 Ctrl+C 停止监控")
    print("=" * 70 + "\n")
    
    if not is_admin:
        log("警告: 不是管理员权限，注入可能失败!", "WARN")
        log("请右键选择'以管理员身份运行'", "WARN")
    
    injected_pids = set()
    crash_count = {}  # 记录每个进程名的崩溃次数
    last_injection_time = {}  # 记录上次注入时间
    
    # 检查现有进程
    existing = get_target_processes()
    if existing:
        log(f"发现已运行的播放器进程: {existing}", "INFO")
        log("这些进程将在延迟后被注入", "INFO")
    
    try:
        while True:
            time.sleep(CHECK_INTERVAL)
            
            # 获取当前进程
            current = get_target_processes()
            
            for proc in current:
                pid = proc['pid']
                name = proc['name']
                
                # 跳过已注入的进程
                if pid in injected_pids:
                    continue
                
                # 检查崩溃次数
                if crash_count.get(name, 0) >= MAX_CRASH_COUNT:
                    if crash_count.get(name) == MAX_CRASH_COUNT:
                        log(f"{name} 已崩溃 {MAX_CRASH_COUNT} 次，停止自动注入", "ERROR")
                        crash_count[name] += 1  # 防止重复输出
                    continue
                
                # 新进程检测
                log(f"检测到新进程: {name} (PID: {pid})", "SUCCESS")
                
                # 等待进程稳定
                if not args.no_wait:
                    log(f"等待 {args.delay} 秒让进程稳定...", "INFO")
                    time.sleep(args.delay)
                    
                    # 再次检查进程是否还在
                    if not is_process_running(pid):
                        log(f"进程 {pid} 已在等待期间退出", "WARN")
                        continue
                
                # 注入
                log("=" * 50, "INFO")
                success = inject_dll(pid, dll_path)
                log("=" * 50, "INFO")
                
                if success:
                    injected_pids.add(pid)
                    last_injection_time[pid] = time.time()
                    log(f"✓ 注入成功! 请查看日志文件", "SUCCESS")
                    log(f"  日志路径: {os.path.dirname(dll_path)}\\logs\\dmitri_compat.log", "INFO")
                    
                    if args.once:
                        log("--once 模式，退出监控", "INFO")
                        return
                else:
                    log(f"✗ 注入失败", "ERROR")
                
                # 短暂等待后检查进程是否崩溃
                time.sleep(1.0)
                if not is_process_running(pid):
                    log(f"⚠ 进程 {pid} 在注入后崩溃了!", "ERROR")
                    crash_count[name] = crash_count.get(name, 0) + 1
                    log(f"  {name} 崩溃计数: {crash_count[name]}/{MAX_CRASH_COUNT}", "WARN")
                    
                    if crash_count[name] >= MAX_CRASH_COUNT:
                        log("建议检查:", "WARN")
                        log("  1. DLL 是否与目标进程架构匹配 (64位)", "WARN")
                        log("  2. 运行时库是否齐全 (libgcc, libstdc++)", "WARN")
                        log("  3. Hook 代码是否有bug", "WARN")
            
            # 清理已退出的进程
            for pid in list(injected_pids):
                if not is_process_running(pid):
                    injected_pids.discard(pid)
                    log(f"进程 {pid} 已退出", "DEBUG")
    
    except KeyboardInterrupt:
        print("\n")
        log("监控已停止", "INFO")
        log(f"共注入 {len(injected_pids)} 个进程", "INFO")
        sys.exit(0)

if __name__ == "__main__":
    main()
