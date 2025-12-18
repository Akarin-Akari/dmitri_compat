#!/usr/bin/env python3
import subprocess
import os
import sys
import shutil

print('='*70)
print('Building 32-bit DmitriCompat DLL')
print('='*70)

MINGW32_BIN = os.path.join('C:', os.sep, 'mingw32', 'mingw32', 'bin')
GCC = os.path.join(MINGW32_BIN, 'gcc.exe')
GPP = os.path.join(MINGW32_BIN, 'g++.exe')

# Verify compiler
if not os.path.exists(GCC):
    print(f'ERROR: GCC not found at {GCC}')
    sys.exit(1)

print(f'Using GCC: {GCC}')
print()

# Setup paths
ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT, 'build_32bit')
OBJ_DIR = os.path.join(BUILD_DIR, 'obj')
BIN_DIR = os.path.join(BUILD_DIR, 'bin')

# Create dirs
os.makedirs(os.path.join(OBJ_DIR, 'minhook'), exist_ok=True)
os.makedirs(os.path.join(BIN_DIR, 'config'), exist_ok=True)
os.makedirs(os.path.join(BIN_DIR, 'logs'), exist_ok=True)

# Common flags
CFLAGS = ['-O2', '-Wall', '-m32']
CXXFLAGS = ['-std=c++17', '-O2', '-Wall', '-m32']
INCLUDES = [
    f'-I{os.path.join(ROOT, "include")}',
    f'-I{os.path.join(ROOT, "external", "minhook", "include")}'
]

# Files to compile
sources = [
    (GCC, CFLAGS, os.path.join(ROOT, 'external', 'minhook', 'src', 'buffer.c'), os.path.join(OBJ_DIR, 'minhook', 'buffer.o'), 'MinHook buffer.c'),
    (GCC, CFLAGS, os.path.join(ROOT, 'external', 'minhook', 'src', 'hook.c'), os.path.join(OBJ_DIR, 'minhook', 'hook.o'), 'MinHook hook.c'),
    (GCC, CFLAGS, os.path.join(ROOT, 'external', 'minhook', 'src', 'trampoline.c'), os.path.join(OBJ_DIR, 'minhook', 'trampoline.o'), 'MinHook trampoline.c'),
    (GCC, CFLAGS, os.path.join(ROOT, 'external', 'minhook', 'src', 'hde', 'hde32.c'), os.path.join(OBJ_DIR, 'minhook', 'hde32.o'), 'MinHook hde32.c'),
    (GPP, CXXFLAGS, os.path.join(ROOT, 'src', 'logger.cpp'), os.path.join(OBJ_DIR, 'logger.o'), 'logger.cpp'),
    (GPP, CXXFLAGS, os.path.join(ROOT, 'src', 'config.cpp'), os.path.join(OBJ_DIR, 'config.o'), 'config.cpp'),
    (GPP, CXXFLAGS, os.path.join(ROOT, 'src', 'hooks', 'd3d11_hooks.cpp'), os.path.join(OBJ_DIR, 'd3d11_hooks.o'), 'd3d11_hooks.cpp'),
    (GPP, CXXFLAGS, os.path.join(ROOT, 'src', 'main_debug.cpp'), os.path.join(OBJ_DIR, 'main_debug.o'), 'main_debug.cpp'),
]

obj_files = []
for idx, (compiler, flags, src, obj, name) in enumerate(sources, 1):
    print(f'[{idx}/{len(sources)}] Compiling {name}...')

    cmd = [compiler] + flags + INCLUDES + ['-c', src, '-o', obj]

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()

    if proc.returncode != 0:
        print(f'  ERROR! Compilation failed')
        print(f'  Command: {" ".join(cmd)}')
        if stdout: print(f'  STDOUT: {stdout}')
        if stderr: print(f'  STDERR: {stderr}')
        sys.exit(1)

    if stderr:
        print(f'  Warnings: {stderr}')

    obj_files.append(obj)
    print(f'  OK')

# Link
print(f'\n[{len(sources)+1}/{len(sources)+1}] Linking DLL...')
dll_path = os.path.join(BIN_DIR, 'dmitri_compat.dll')
cmd = [GPP, '-m32', '-shared', '-o', dll_path] + obj_files + [
    '-lkernel32', '-luser32', '-ld3d11', '-ldxgi',
    '-static-libgcc', '-static-libstdc++'
]

proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
stdout, stderr = proc.communicate()

if proc.returncode != 0:
    print('  ERROR! Linking failed')
    if stdout: print(f'  STDOUT: {stdout}')
    if stderr: print(f'  STDERR: {stderr}')
    sys.exit(1)

print('  OK')

# Copy config
shutil.copy(os.path.join(ROOT, 'config', 'config.ini'), os.path.join(BIN_DIR, 'config', 'config.ini'))

print()
print('='*70)
print('SUCCESS! 32-bit DLL built')
print('='*70)
print(f'DLL: {dll_path}')
print(f'Size: {os.path.getsize(dll_path):,} bytes')
