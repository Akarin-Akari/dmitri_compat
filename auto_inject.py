#!/usr/bin/env python3
"""
Auto-Injector for DmitriRender
Monitors for drtm.exe process and automatically injects dmitri_compat.dll
"""

import subprocess
import time
import os
import sys

INJECTOR_PATH = r'injector32.exe'
DLL_PATH = r'build_32bit\bin\dmitri_compat.dll'
TARGET_PROCESS = 'drtm.exe'
CHECK_INTERVAL = 0.5  # Check every 500ms

injected_pids = set()

def get_drtm_processes():
    """Get list of drtm.exe PIDs"""
    try:
        result = subprocess.run(
            ['tasklist', '/FI', f'IMAGENAME eq {TARGET_PROCESS}', '/FO', 'CSV', '/NH'],
            capture_output=True,
            text=True,
            encoding='gbk',
            errors='replace'
        )

        pids = []
        for line in result.stdout.strip().split('\n'):
            if TARGET_PROCESS in line:
                # Parse CSV: "drtm.exe","PID","Console","1","10,084 K"
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
    """Inject DLL into process"""
    dll_abs_path = os.path.abspath(DLL_PATH)

    print(f"\n{'='*70}")
    print(f"[INJECT] Injecting into PID {pid}...")
    print(f"[INJECT] DLL: {dll_abs_path}")

    try:
        result = subprocess.run(
            [INJECTOR_PATH, str(pid), dll_abs_path],
            capture_output=True,
            text=True,
            timeout=10
        )

        if 'SUCCESS' in result.stdout:
            print(f"[SUCCESS] ✓ DLL injected successfully!")
            print(f"[SUCCESS] Check logs at: build_32bit\\bin\\logs\\dmitri_compat.log")
            return True
        else:
            print(f"[FAILED] Injection failed:")
            print(result.stdout)
            if result.stderr:
                print(result.stderr)
            return False

    except subprocess.TimeoutExpired:
        print(f"[ERROR] Injection timeout")
        return False
    except Exception as e:
        print(f"[ERROR] Injection error: {e}")
        return False

def main():
    print("="*70)
    print("DmitriRender Auto-Injector")
    print("="*70)
    print(f"Target: {TARGET_PROCESS}")
    print(f"DLL: {DLL_PATH}")
    print(f"Check Interval: {CHECK_INTERVAL}s")
    print()
    print("Monitoring for drtm.exe processes...")
    print("Press Ctrl+C to stop")
    print("="*70)

    # Check current processes
    current_pids = get_drtm_processes()
    if current_pids:
        print(f"\n[INFO] Found existing processes: {current_pids}")
        for pid in current_pids:
            injected_pids.add(pid)
            print(f"[INFO] Skipping PID {pid} (already running before monitor started)")

    last_check_time = time.time()
    check_count = 0

    try:
        while True:
            time.sleep(CHECK_INTERVAL)
            check_count += 1

            # Get current processes
            current_pids = get_drtm_processes()

            # Find new processes
            new_pids = [pid for pid in current_pids if pid not in injected_pids]

            if new_pids:
                for pid in new_pids:
                    print(f"\n[DETECTED] New drtm.exe process: PID {pid}")

                    # Small delay to ensure process is fully initialized
                    time.sleep(0.2)

                    # Inject DLL
                    success = inject_dll(pid)

                    # Mark as injected (even if failed, to avoid retry spam)
                    injected_pids.add(pid)

                    if success:
                        print(f"\n[READY] DmitriCompat is now monitoring D3D11 calls!")
                        print(f"[READY] Perform video operations (play/pause/seek) to trigger hooks")

            # Status update every 60 checks (~30 seconds)
            if check_count % 60 == 0:
                elapsed = time.time() - last_check_time
                print(f"\n[STATUS] Still monitoring... ({check_count} checks, {elapsed:.1f}s)")
                if current_pids:
                    print(f"[STATUS] Active drtm.exe PIDs: {current_pids}")
                last_check_time = time.time()

    except KeyboardInterrupt:
        print("\n\n[EXIT] Monitor stopped by user")
        print(f"[EXIT] Injected into {len(injected_pids)} process(es)")
        sys.exit(0)

if __name__ == '__main__':
    # Change to script directory
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    main()
