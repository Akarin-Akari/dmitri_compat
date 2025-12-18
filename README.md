# DmitriCompat - RTX 50 系列兼容层

DmitriRender RTX 50 系列显卡兼容性修复工具。通过 API Hook 方式解决绿屏问题。

## ⚡ 快速开始

### 1. 构建项目

```bash
# Windows (需要 Visual Studio 2019/2022 和 CMake)
build.bat
```

### 2. 注入到播放器

```bash
# 方式 1: 通过进程 PID
python injector.py 12345

# 方式 2: 通过进程名
python injector.py PotPlayerMini64.exe
```

### 3. 查看日志

打开 `build/bin/logs/dmitri_compat.log` 查看 Hook 日志。

---

## 📋 系统要求

- Windows 10/11 64 位
- Visual Studio 2019/2022
- CMake 3.15+
- Python 3.7+ (用于注入器)

---

## 🛠️ 项目结构

```
dmitri_compat/
├── src/
│   ├── main.cpp              # DLL 入口点
│   ├── logger.cpp            # 日志系统
│   ├── config.cpp            # 配置加载器
│   └── hooks/
│       └── d3d11_hooks.cpp   # D3D11 API Hook 实现
├── include/
│   ├── logger.h
│   ├── config.h
│   └── d3d11_hooks.h
├── external/
│   └── minhook/              # MinHook 库
├── config/
│   └── config.ini            # 配置文件
├── build.bat                 # 构建脚本
├── injector.py               # DLL 注入工具
└── CMakeLists.txt
```

---

## ⚙️ 配置选项

编辑 `build/bin/config/config.ini`:

```ini
[Fixes]
# 纹理格式转换 (推荐开启)
EnableTextureFormatConversion=1

# 颜色空间校正 (实验性)
EnableColorSpaceCorrection=0

# GPU 同步 (实验性)
EnableGPUSync=0

[Debug]
# 日志级别: 0=None, 1=Error, 2=Info, 3=Verbose
LogLevel=2

# 转储纹理 (调试用)
DumpTextures=0
```

---

## 🔍 工作原理

### Hook 的 API

1. **D3D11CreateDevice**
   - 记录设备创建参数
   - 检查特性级别
   - Hook 设备对象的方法

2. **ID3D11Device::CreateTexture2D**
   - 检测视频格式纹理 (NV12, P010, YUY2)
   - 记录所有纹理参数
   - （未来）转换不兼容的格式

3. **IDXGISwapChain::Present**
   - 监控帧呈现
   - （未来）添加颜色空间修复

### 技术细节

- 使用 MinHook 进行运行时 API 拦截
- 通过 VTable Hook 拦截 COM 对象方法
- 详细日志记录便于调试
- 配置文件支持运行时切换修复策略

---

## 📊 当前状态

### ✅ 已实现 (MVP)

- [x] 日志系统
- [x] 配置文件加载
- [x] D3D11CreateDevice Hook
- [x] CreateTexture2D Hook
- [x] Present Hook
- [x] CMake 构建系统
- [x] DLL 注入工具

### 🚧 开发中

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
# [2025-11-08 02:00:00.000] [INFO ] ✓ D3D11CreateDevice hooked at ...
# [2025-11-08 02:00:01.123] [INFO ] === D3D11CreateDevice Called ===
```

### 常见问题

1. **注入失败**
   - 以管理员权限运行
   - 检查目标进程是否是 64 位
   - 确认 dmitri_compat.dll 存在

2. **没有日志输出**
   - 检查 config.ini 的 LogLevel
   - 确认 logs 目录有写入权限
   - 验证 DmitriRender 是否真的使用了 D3D11

3. **仍然绿屏**
   - 收集日志并提交 Issue
   - 尝试启用不同的修复选项
   - 检查 GPU 驱动版本

---

## 🔬 实验性功能

### 启用颜色空间修复

```ini
[Fixes]
EnableColorSpaceCorrection=1
```

### 启用 GPU 同步

```ini
[Fixes]
EnableGPUSync=1
```

**注意**: 实验性功能可能影响性能或稳定性。

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

本项目采用 **MIT 许可证**。

### 重要说明

- ✅ 本项目仅通过外部 API Hook 实现兼容性
- ✅ 不包含任何 DmitriRender 的原始代码
- ✅ 不涉及反编译或逆向工程
- ✅ 完全开源，鼓励社区改进

---

## 🙏 致谢

- **DmitriRender** - 原始补帧滤镜作者
- **MinHook** - 优秀的 Hook 库
- **社区贡献者** - 测试和反馈

---

## 📞 支持

- **Issues**: [GitHub Issues](https://github.com/your-repo/issues)
- **讨论**: [GitHub Discussions](https://github.com/your-repo/discussions)
- **文档**: 查看 `PHASE1_DIAGNOSTIC_REPORT.md` 了解技术细节

---

## 🔄 更新日志

### v0.1.0 (2025-11-08) - MVP

- 实现基础 Hook 框架
- 支持 D3D11CreateDevice 拦截
- 支持 CreateTexture2D 监控
- 支持 Present Hook
- 详细日志记录
- 配置文件支持

---

**祝你成功复活 DmitriRender！** 🚀
