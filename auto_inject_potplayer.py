#!/usr/bin/env python3
"""
Auto-Injector for PotPlayer (64-bit)
Monitors for PotPlayer process and automatically injects dmitri_compat.dll
"""

import subprocess
import time
import os
import sys

DLL_PATH_64 = r'build\bin\dmitri_compat.dll'
TARGET_PROCESS = 'PotPlayerMini64.exe'
CHECK_INTERVAL = 0.3  # Check every 300ms for faster detection

injected_pids = set()

def get_potplayer_processes():
    """Get list of PotPlayer PIDs"""
    try:
        result = subprocess.run(
            ['tasklist', '/FI', f'IMAGENAME eq {TARGET_PROCESS}', '/FO', 'CSV', '/NH'],
            capture_output=True,
            text=True,
            encoding='gbk',
            errors='replace',
            timeout=5
        )

        pids = []
        for line in result.stdout.strip().split('\n'):
            if TARGET_PROCESS in line:
                parts = line.split('","')
                if len(parts) >= 2:
                    pid_str = parts[1].strip('"')
                    try:
                        pids.append(int(pid_str))
                    except ValueError:
                        pass
        return pids
    except Exception as e:
        print(f"[ERROR] Failed to get process list: {e}")
        return []

def inject_dll(pid):
    """Inject DLL into process using Python injector"""
    dll_abs_path = os.path.abspath(DLL_PATH_64)

    print(f"\n{'='*70}")
    print(f"[INJECT] Injecting into PID {pid}...")
    print(f"[INJECT] DLL: {dll_abs_path}")
    print(f"[INJECT] Size: {os.path.getsize(dll_abs_path):,} bytes")

    try:
        result = subprocess.run(
            ['python', 'inject_any.py', str(pid), dll_abs_path],
            capture_output=True,
            text=True,
            timeout=15
        )

        print(result.stdout)

        if 'SUCCESS' in result.stdout or 'Module handle' in result.stdout:
            print(f"[SUCCESS] ✓ DLL injected successfully!")
            print(f"[SUCCESS] Check logs at: build\\bin\\logs\\dmitri_compat.log")
            return True
        else:
            print(f"[FAILED] Injection failed")
            if result.stderr:
                print(f"[STDERR] {result.stderr}")
            return False

    except subprocess.TimeoutExpired:
        print(f"[ERROR] Injection timeout")
        return False
    except Exception as e:
        print(f"[ERROR] Injection error: {e}")
        return False

def main():
    print("="*70)
    print("PotPlayer Auto-Injector for DmitriCompat")
    print("="*70)
    print(f"Target: {TARGET_PROCESS} (64-bit)")
    print(f"DLL: {DLL_PATH_64}")
    print(f"Check Interval: {CHECK_INTERVAL}s")
    print()
    print("Monitoring for PotPlayer processes...")
    print("Press Ctrl+C to stop")
    print("="*70)

    # Check current processes
    current_pids = get_potplayer_processes()
    if current_pids:
        print(f"\n[INFO] Found existing PotPlayer processes: {current_pids}")
        print(f"[INFO] These will be injected now (if not already done)")

        for pid in current_pids:
            if pid not in injected_pids:
                inject_dll(pid)
                injected_pids.add(pid)

    last_status_time = time.time()
    check_count = 0

    try:
        while True:
            time.sleep(CHECK_INTERVAL)
            check_count += 1

            # Get current processes
            current_pids = get_potplayer_processes()

            # Find new processes
            new_pids = [pid for pid in current_pids if pid not in injected_pids]

            if new_pids:
                for pid in new_pids:
                    print(f"\n[DETECTED] New PotPlayer process: PID {pid}")

                    # Small delay to ensure process is initialized
                    time.sleep(0.5)

                    # Inject DLL
                    success = inject_dll(pid)

                    # Mark as injected
                    injected_pids.add(pid)

                    if success:
                        print(f"\n[READY] DmitriCompat is now monitoring D3D11 calls!")
                        print(f"[READY] Video playback will be logged")

            # Status update every 100 checks (~30 seconds)
            if check_count % 100 == 0:
                elapsed = time.time() - last_status_time
                print(f"\n[STATUS] Still monitoring... ({check_count} checks, {elapsed:.1f}s)")
                if current_pids:
                    print(f"[STATUS] Active PotPlayer PIDs: {current_pids}")
                else:
                    print(f"[STATUS] No PotPlayer process detected")
                last_status_time = time.time()

    except KeyboardInterrupt:
        print("\n\n[EXIT] Monitor stopped by user")
        print(f"[EXIT] Injected into {len(injected_pids)} process(es)")
        sys.exit(0)

if __name__ == '__main__':
    # Change to script directory
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    main()
