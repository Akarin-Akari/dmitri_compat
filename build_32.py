import subprocess, os, sys, shutil

GCC = r'C:\mingw32\mingw32\bin\gcc.exe'
GPP = r'C:\mingw32\mingw32\bin\g++.exe'
ROOT = os.path.dirname(os.path.abspath(__file__))

os.makedirs('build_32bit/obj/minhook', exist_ok=True)
os.makedirs('build_32bit/bin/config', exist_ok=True)
os.makedirs('build_32bit/bin/logs', exist_ok=True)

def run_cmd(desc, cmd):
    print(f'{desc}...')
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(f'ERROR: {r.stderr}')
        sys.exit(1)
    if r.stderr: print(f'Warnings: {r.stderr}')
    print('  OK')

inc = f'-Iinclude -Iexternal/minhook/include'

run_cmd('[1/8] MinHook buffer', f'{GCC} -O2 -m32 {inc} -c external/minhook/src/buffer.c -o build_32bit/obj/minhook/buffer.o')
run_cmd('[2/8] MinHook hook', f'{GCC} -O2 -m32 {inc} -c external/minhook/src/hook.c -o build_32bit/obj/minhook/hook.o')
run_cmd('[3/8] MinHook trampoline', f'{GCC} -O2 -m32 {inc} -c external/minhook/src/trampoline.c -o build_32bit/obj/minhook/trampoline.o')
run_cmd('[4/8] MinHook hde32', f'{GCC} -O2 -m32 {inc} -c external/minhook/src/hde/hde32.c -o build_32bit/obj/minhook/hde32.o')
run_cmd('[5/8] logger.cpp', f'{GPP} -std=c++17 -O2 -m32 {inc} -c src/logger.cpp -o build_32bit/obj/logger.o')
run_cmd('[6/8] config.cpp', f'{GPP} -std=c++17 -O2 -m32 {inc} -c src/config.cpp -o build_32bit/obj/config.o')
run_cmd('[7/8] d3d11_hooks.cpp', f'{GPP} -std=c++17 -O2 -m32 {inc} -c src/hooks/d3d11_hooks.cpp -o build_32bit/obj/d3d11_hooks.o')
run_cmd('[8/8] main_debug.cpp', f'{GPP} -std=c++17 -O2 -m32 {inc} -c src/main_debug.cpp -o build_32bit/obj/main_debug.o')

objs = 'build_32bit/obj/minhook/*.o build_32bit/obj/*.o'
run_cmd('[9/9] Linking DLL', f'{GPP} -m32 -shared -o build_32bit/bin/dmitri_compat.dll {objs} -lkernel32 -luser32 -ld3d11 -ldxgi -static-libgcc -static-libstdc++')

shutil.copy('config/config.ini', 'build_32bit/bin/config/config.ini')

dll_path = 'build_32bit/bin/dmitri_compat.dll'
print(f'\nSUCCESS! {os.path.getsize(dll_path):,} bytes')
print(f'DLL: {dll_path}')
