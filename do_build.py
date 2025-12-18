#!/usr/bin/env python3
import subprocess
import os
import sys

os.chdir(r'C:\Users\Akari\AppData\Roaming\DmitriRender\dmitri_compat')

# Add MinGW to PATH
mingw_bin = r'C:\mingw32\mingw32\bin'
os.environ['PATH'] = mingw_bin + os.pathsep + os.environ['PATH']

def run(desc, cmd):
    print(desc)
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f'  ERROR!')
        if result.stdout: print(f'  OUT: {result.stdout}')
        if result.stderr: print(f'  ERR: {result.stderr}')
        sys.exit(1)
    if result.stderr and 'warning' in result.stderr.lower():
        print(f'  Warnings: {result.stderr[:200]}')
    print('  OK')
    return result

print('='*70)
print('Building 32-bit DmitriCompat DLL')
print('='*70)
print()

# Create directories
os.makedirs('build_32bit/obj/minhook', exist_ok=True)
os.makedirs('build_32bit/bin/config', exist_ok=True)
os.makedirs('build_32bit/bin/logs', exist_ok=True)

# Compile MinHook
run('[1/9] MinHook buffer.c',
    'gcc -O2 -m32 -Iinclude -Iexternal/minhook/include -c external/minhook/src/buffer.c -o build_32bit/obj/minhook/buffer.o')

run('[2/9] MinHook hook.c',
    'gcc -O2 -m32 -Iinclude -Iexternal/minhook/include -c external/minhook/src/hook.c -o build_32bit/obj/minhook/hook.o')

run('[3/9] MinHook trampoline.c',
    'gcc -O2 -m32 -Iinclude -Iexternal/minhook/include -c external/minhook/src/trampoline.c -o build_32bit/obj/minhook/trampoline.o')

run('[4/9] MinHook hde32.c',
    'gcc -O2 -m32 -Iinclude -Iexternal/minhook/include -c external/minhook/src/hde/hde32.c -o build_32bit/obj/minhook/hde32.o')

# Compile dmitri_compat sources
run('[5/9] logger.cpp',
    'g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal/minhook/include -c src/logger.cpp -o build_32bit/obj/logger.o')

run('[6/9] config.cpp',
    'g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal/minhook/include -c src/config.cpp -o build_32bit/obj/config.o')

run('[7/9] d3d11_hooks.cpp',
    'g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal/minhook/include -c src/hooks/d3d11_hooks.cpp -o build_32bit/obj/d3d11_hooks.o')

run('[8/9] main_debug.cpp',
    'g++ -std=c++17 -O2 -m32 -Iinclude -Iexternal/minhook/include -c src/main_debug.cpp -o build_32bit/obj/main_debug.o')

# Link
run('[9/9] Linking dmitri_compat.dll',
    'g++ -m32 -shared -o build_32bit/bin/dmitri_compat.dll ' +
    'build_32bit/obj/minhook/buffer.o build_32bit/obj/minhook/hook.o build_32bit/obj/minhook/trampoline.o build_32bit/obj/minhook/hde32.o ' +
    'build_32bit/obj/logger.o build_32bit/obj/config.o build_32bit/obj/d3d11_hooks.o build_32bit/obj/main_debug.o ' +
    '-lkernel32 -luser32 -ld3d11 -ldxgi -static-libgcc -static-libstdc++')

# Copy config
import shutil
shutil.copy('config/config.ini', 'build_32bit/bin/config/config.ini')

# Verify
dll_path = 'build_32bit/bin/dmitri_compat.dll'
size = os.path.getsize(dll_path)

print()
print('='*70)
print('SUCCESS!')
print('='*70)
print(f'DLL: {os.path.abspath(dll_path)}')
print(f'Size: {size:,} bytes ({size/1024/1024:.2f} MB)')
print()
print('Next step: Inject into drtm.exe')
print('  python inject_any.py [PID] dmitri_compat.dll')
