#!/usr/bin/env python3
"""
Automated 32-bit MinGW-w64 deployment script
Downloads and installs i686-posix-dwarf MinGW for 32-bit compilation
"""

import os
import sys
import urllib.request
import zipfile
import shutil
from pathlib import Path

MINGW32_URL = "https://github.com/niXman/mingw-builds-binaries/releases/download/14.2.0-rt_v12-rev0/i686-14.2.0-release-posix-dwarf-msvcrt-rt_v12-rev0.7z"
MINGW32_DIR = r"C:\mingw32"
TEMP_DIR = r"C:\Users\Akari\AppData\Local\Temp\mingw32_install"

def download_file(url, dest_path):
    """Download file with progress bar"""
    print(f"\nDownloading from:\n  {url}")
    print(f"To:\n  {dest_path}\n")

    def progress_hook(block_num, block_size, total_size):
        downloaded = block_num * block_size
        percent = min(100, downloaded * 100 / total_size) if total_size > 0 else 0
        bar_length = 50
        filled = int(bar_length * percent / 100)
        bar = '#' * filled + '.' * (bar_length - filled)
        print(f"\r[{bar}] {percent:.1f}% ({downloaded/1024/1024:.1f}MB / {total_size/1024/1024:.1f}MB)", end='', flush=True)

    urllib.request.urlretrieve(url, dest_path, progress_hook)
    print("\n\nDownload complete!")

def extract_7z(archive_path, extract_to):
    """Extract 7z archive using Windows built-in tar (available in Win10+)"""
    print(f"\nExtracting to: {extract_to}")

    # First try using 7z if available
    try:
        import subprocess
        result = subprocess.run(['7z', 'x', archive_path, f'-o{extract_to}', '-y'],
                              capture_output=True, text=True)
        if result.returncode == 0:
            print("Extracted successfully using 7z!")
            return True
    except:
        pass

    # Fallback: Try using Python's zipfile (won't work for .7z, but worth trying)
    print("\n7z not found. Trying alternative extraction method...")
    print("\nNOTE: Windows 10+ includes tar.exe which can handle .7z files.")
    print("Attempting to use tar...")

    import subprocess
    try:
        result = subprocess.run(['tar', '-xf', archive_path, '-C', extract_to],
                              capture_output=True, text=True, timeout=300)
        if result.returncode == 0:
            print("Extracted successfully using tar!")
            return True
        else:
            print(f"tar failed: {result.stderr}")
            return False
    except Exception as e:
        print(f"Extraction failed: {e}")
        return False

def main():
    print("="*70)
    print("32-bit MinGW-w64 Automated Deployment")
    print("="*70)

    # Check if already installed
    if os.path.exists(MINGW32_DIR):
        print(f"\n[WARNING] {MINGW32_DIR} already exists!")
        response = input("Remove and reinstall? (y/N): ").strip().lower()
        if response == 'y':
            print(f"Removing {MINGW32_DIR}...")
            shutil.rmtree(MINGW32_DIR)
        else:
            print("Installation cancelled.")
            sys.exit(0)

    # Create temp directory
    os.makedirs(TEMP_DIR, exist_ok=True)

    try:
        # Download MinGW
        archive_path = os.path.join(TEMP_DIR, "mingw32.7z")
        download_file(MINGW32_URL, archive_path)

        # Extract
        print(f"\nExtracting archive...")
        os.makedirs(MINGW32_DIR, exist_ok=True)

        if not extract_7z(archive_path, MINGW32_DIR):
            print("\n[ERROR] Extraction failed!")
            print("\nManual steps:")
            print(f"1. Download: {MINGW32_URL}")
            print(f"2. Extract to: {MINGW32_DIR}")
            print(f"3. Ensure {MINGW32_DIR}\\bin\\gcc.exe exists")
            sys.exit(1)

        # Verify installation
        print("\nVerifying installation...")
        gcc_path = os.path.join(MINGW32_DIR, "mingw32", "bin", "gcc.exe")

        # Check if files are in subdirectory
        if not os.path.exists(gcc_path):
            # Try to find gcc.exe
            for root, dirs, files in os.walk(MINGW32_DIR):
                if "gcc.exe" in files:
                    actual_mingw_dir = root.replace("\\bin", "")
                    print(f"\nFound MinGW at: {actual_mingw_dir}")
                    # Move to correct location if needed
                    if actual_mingw_dir != MINGW32_DIR:
                        print("Reorganizing directory structure...")
                        temp_move = MINGW32_DIR + "_temp"
                        shutil.move(actual_mingw_dir, temp_move)
                        shutil.rmtree(MINGW32_DIR)
                        shutil.move(temp_move, MINGW32_DIR)
                    gcc_path = os.path.join(MINGW32_DIR, "bin", "gcc.exe")
                    break

        if os.path.exists(gcc_path):
            print(f"[OK] GCC found at: {gcc_path}")

            # Test gcc
            import subprocess
            result = subprocess.run([gcc_path, '--version'],
                                  capture_output=True, text=True)
            print("\nGCC Version:")
            print(result.stdout)

            # Test 32-bit compilation
            test_c = os.path.join(TEMP_DIR, "test32.c")
            with open(test_c, 'w') as f:
                f.write('int main() { return 0; }')

            test_exe = os.path.join(TEMP_DIR, "test32.exe")
            result = subprocess.run([gcc_path, test_c, '-o', test_exe],
                                  capture_output=True, text=True)

            if os.path.exists(test_exe):
                print("[OK] 32-bit compilation test PASSED!")

                # Check architecture
                result = subprocess.run(['file', test_exe],
                                      capture_output=True, text=True)
                if 'PE32' in result.stdout and '80386' in result.stdout:
                    print("[OK] Confirmed 32-bit executable (PE32 Intel 80386)")

                os.remove(test_exe)
            else:
                print("[WARNING] 32-bit compilation test failed")
                print(result.stderr)

            print("\n" + "="*70)
            print("SUCCESS! 32-bit MinGW-w64 installed successfully!")
            print("="*70)
            print(f"\nInstallation path: {MINGW32_DIR}")
            print(f"GCC path: {gcc_path}")
            print("\nNext steps:")
            print("1. Run build_32bit.bat to compile 32-bit DLL")
            print("2. Inject into drtm.exe (DmitriRender)")

        else:
            print(f"\n[ERROR] gcc.exe not found at expected location!")
            print(f"Expected: {gcc_path}")
            print(f"\nPlease check {MINGW32_DIR} and locate gcc.exe manually")
            sys.exit(1)

    except Exception as e:
        print(f"\n[ERROR] Installation failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

    finally:
        # Cleanup temp directory
        if os.path.exists(TEMP_DIR):
            print(f"\nCleaning up temporary files...")
            try:
                shutil.rmtree(TEMP_DIR)
            except:
                pass

if __name__ == "__main__":
    main()
