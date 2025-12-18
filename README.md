# DmitriCompat - RTX 50 系列兼容层

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)

> 🎯 通过 API Hook 技术解决 DmitriRender 补帧滤镜在 NVIDIA RTX 50 (Blackwell) 系列显卡上的绿屏兼容性问题。

## 📖 项目背景

[DmitriRender](http://www.dmitrirender.ru/) 是一款广受好评的视频插帧滤镜，可将低帧率视频实时补帧至 60fps 或更高。然而，由于该滤镜使用了针对旧版 GPU 架构编译的 CUDA Kernel，在最新的 RTX 50 系列 (Blackwell 架构) 显卡上会出现**绿屏**问题。

本项目通过 **运行时 API Hook** 技术，拦截并修复有问题的 CUDA 调用，使 DmitriRender 能在新显卡上正常工作。

---

## ⚡ 技术方案

```
┌─────────────────┐     Hook      ┌──────────────────────┐
│  DmitriRender   │ ──────────▶  │   DmitriCompat.dll   │
│  (CUDA Kernel)  │              │  - CUDA API Hook     │
└─────────────────┘              │  - JIT Fallback      │
                                 │  - Compute Shader    │
                                 └──────────────────────┘
```

### 核心修复策略

1. **CUDA Module JIT Fallback** - 当 `cuModuleLoadData` 失败时，自动切换到 PTX JIT 重编译模式
2. **NULL Kernel Bypass** - 当 CUDA Kernel 函数指针为空时，返回成功避免程序崩溃
3. **Compute Shader 替代** - 使用 D3D11 Compute Shader 替代失败的 CUDA 色彩转换 Kernel

---

## 🚀 快速开始

### 1. 构建项目

```bash
# 使用 Visual Studio
build.bat

# 或使用 MinGW
build_smart.bat
```

### 2. 注入到播放器

```bash
# 启动 PotPlayer 并加载视频后
python injector.py PotPlayerMini64.exe

# 或通过进程 PID
python injector.py 12345

# 自动监控并注入
python auto_inject_potplayer.py
```

### 3. 查看日志

```bash
# 日志位置
%APPDATA%\DmitriRender\dmitri_compat\logs\dmitri_compat.log

# 或在构建目录
build\bin\logs\dmitri_compat.log
```

---

## 📋 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows 10/11 64-bit |
| 编译器 | Visual Studio 2019/2022 或 MinGW-w64 |
| CMake | 3.15+ |
| Python | 3.7+ (用于注入工具) |
| 目标显卡 | NVIDIA RTX 50 系列 (Blackwell) |

---

## 🛠️ 项目结构

```
dmitri_compat/
├── src/
│   ├── main.cpp                  # DLL 入口点 (基础版)
│   ├── main_late_hook.cpp        # DLL 入口点 (RTX 50 模式)
│   ├── logger.cpp                # 日志系统
│   ├── config.cpp                # 配置加载器
│   └── hooks/
│       ├── cuda_hook.cpp         # CUDA Driver API Hook (核心)
│       ├── d3d11_hooks.cpp       # D3D11 API Hook
│       ├── late_hook.cpp         # 后期设备 Hook
│       ├── video_processor_hook.cpp  # 视频处理器 Hook
│       ├── keyed_mutex_hook.cpp  # KeyedMutex Hook
│       └── compute_shader_replacement.cpp  # Compute Shader 替代
├── include/
│   ├── logger.h
│   ├── config.h
│   └── d3d11_hooks.h
├── external/
│   └── minhook/                  # MinHook Hook 库
├── shaders/
│   └── nv12_to_bgra.hlsl         # NV12 转 BGRA Compute Shader
├── config/
│   └── config.ini                # 配置文件
├── CMakeLists.txt                # CMake 构建配置
├── build.bat                     # Windows 构建脚本
├── injector.py                   # DLL 注入工具
└── auto_inject_potplayer.py      # PotPlayer 自动注入
```

---

## ⚙️ 配置选项

编辑 `config/config.ini`:

```ini
[Fixes]
# CUDA JIT Fallback (RTX 50 核心修复)
EnableCudaJitFallback=1

# Compute Shader 替代色彩转换
EnableComputeShaderReplacement=1

# 纹理格式转换 (实验性)
EnableTextureFormatConversion=0

# 颜色空间校正 (实验性)
EnableColorSpaceCorrection=0

# GPU 同步 (实验性)
EnableGPUSync=0

[Debug]
# 日志级别: 0=Off, 1=Error, 2=Info, 3=Verbose
LogLevel=2

# 转储纹理 (调试用)
DumpTextures=0
```

---

## 🔍 Hook 的 API

### CUDA Driver API (RTX 50 核心)

| API | 功能 |
|-----|------|
| `cuModuleLoadData` | 添加 JIT PTX Fallback |
| `cuModuleLoadDataEx` | 扩展 JIT 选项 |
| `cuLaunchKernel` | 绕过 NULL 函数指针 |
| `cuGraphicsD3D11RegisterResource` | 追踪 D3D11 纹理绑定 |

### D3D11 API

| API | 功能 |
|-----|------|
| `D3D11CreateDevice` | 设备创建监控 |
| `ID3D11Device::CreateTexture2D` | 视频纹理格式检测 (NV12, P010, YUY2) |
| `IDXGISwapChain::Present` | 帧呈现监控 |

### 技术细节

- 使用 MinHook 进行运行时 API 拦截
- 通过 VTable Hook 拦截 COM 对象方法
- 详细日志记录便于调试
- 配置文件支持运行时切换修复策略

---

## 📊 当前状态

### ✅ 已实现

- [x] 日志系统
- [x] 配置文件加载
- [x] D3D11CreateDevice Hook
- [x] CreateTexture2D Hook
- [x] Present Hook
- [x] CUDA Driver API Hook
- [x] JIT Fallback 机制
- [x] NULL Kernel Bypass
- [x] CMake 构建系统
- [x] DLL 注入工具

### 🚧 开发中

- [ ] Compute Shader 色彩转换
- [ ] 纹理格式自动转换
- [ ] 颜色空间修复
- [ ] DXVA2 Hook
- [ ] GPU 同步优化

### 📅 计划中

- [ ] GUI 配置工具
- [ ] 自动更新检查
- [ ] 性能监控面板
- [ ] 多播放器兼容性测试

---

## 🐛 调试指南

### 检查 Hook 是否生效

```bash
# 查看日志文件
tail -f build/bin/logs/dmitri_compat.log

# 应该看到类似输出:
# [INFO ] ✓ cuModuleLoadData hooked at 0x...
# [INFO ] 🔥 cuInit #1: flags=0x0
# [INFO ] ✓ cuInit SUCCESS
```

### 常见问题

1. **注入失败**
   - 以管理员权限运行
   - 检查目标进程是否是 64 位
   - 确认 dmitri_compat.dll 存在

2. **没有日志输出**
   - 检查 config.ini 的 LogLevel
   - 确认 logs 目录有写入权限
   - 验证 DmitriRender 是否真的使用了 D3D11/CUDA

3. **仍然绿屏**
   - 收集日志并提交 Issue
   - 尝试启用不同的修复选项
   - 检查 GPU 驱动版本

---

## 📖 使用场景

### 场景 1: PotPlayer + DmitriRender

```bash
# 1. 打开 PotPlayer
# 2. 加载视频
# 3. 启用 DmitriRender 滤镜
# 4. 获取 PotPlayer 进程 PID
tasklist | findstr PotPlayer

# 5. 注入 DLL
python injector.py PotPlayerMini64.exe

# 6. 查看日志
notepad build\bin\logs\dmitri_compat.log
```

### 场景 2: MPC-HC + DmitriRender

```bash
# 类似流程
python injector.py mpc-hc64.exe
```

---

## 📚 版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| v0.4.1 | 2025-12-12 | RTX 50 专用模式，禁用 D3D11 VTable Hook 防崩溃 |
| v0.4.0 | 2025-12-08 | 添加 Compute Shader 替代方案 |
| v0.3.0 | 2025-11-28 | CUDA Hook + JIT Fallback |
| v0.2.0 | 2025-11-15 | 后期 Hook (Late Hook) 技术 |
| v0.1.0 | 2025-11-08 | MVP - 基础 Hook 框架 |

---

## 📝 技术文档

- [PHASE1_DIAGNOSTIC_REPORT.md](./PHASE1_DIAGNOSTIC_REPORT.md) - DmitriRender DLL 依赖分析报告
- [PHASE2_SUMMARY.md](./PHASE2_SUMMARY.md) - API Hook 兼容层开发总结
- [BUILD_SOLUTIONS.md](./BUILD_SOLUTIONS.md) - 构建问题解决方案

---

## ⚠️ 注意事项

1. **RTX 50 专用模式**: 当前版本针对 Blackwell 架构优化，避免使用 D3D11 VTable Hook
2. **管理员权限**: DLL 注入需要以管理员权限运行
3. **杀毒软件**: 可能需要将注入工具和 DLL 添加到白名单
4. **实验性功能**: Compute Shader 替代方案仍在测试中

---

## 🤝 贡献指南

欢迎提交 Pull Request！

### 开发流程

1. Fork 本仓库
2. 创建特性分支: `git checkout -b feature/xxx`
3. 提交更改: `git commit -m "Add xxx"`
4. 推送到分支: `git push origin feature/xxx`
5. 提交 Pull Request

### 代码规范

- 使用 C++17 标准
- 遵循现有代码风格
- 添加详细注释
- 更新文档

---

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。

### 声明

- ✅ 本项目仅通过外部 API Hook 实现兼容性
- ✅ 不包含任何 DmitriRender 的原始代码
- ✅ 不涉及反编译或逆向工程
- ✅ 完全开源，欢迎社区改进

---

## 🙏 致谢

- **DmitriRender** - 原始补帧滤镜作者 Dmitri
- **[MinHook](https://github.com/TsudaKageworthy/minhook)** - 优秀的 Windows Hook 库
- **社区贡献者** - 测试和反馈

---

## 📞 支持

- **Issues**: [GitHub Issues](https://github.com/Akarin-Akari/dmitri_compat/issues)
- **讨论**: [GitHub Discussions](https://github.com/Akarin-Akari/dmitri_compat/discussions)
- **文档**: 查看 `PHASE1_DIAGNOSTIC_REPORT.md` 了解技术细节

---

**Made with ❤️ for the video enthusiast community** 🚀
