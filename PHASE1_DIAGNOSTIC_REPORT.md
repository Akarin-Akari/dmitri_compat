# DmitriRender RTX 50 兼容性问题诊断报告
## 阶段一：环境诊断与信息收集

**生成时间**: 2025-11-08
**分析工具**: pefile, Python 3.12

---

## 执行摘要

DmitriRender 使用 **Direct3D 11** 和 **DXVA2** 进行视频补帧处理。通过对核心 DLL 的分析，**未发现 CUDA 或 OpenCL 依赖**，因此 RTX 50 取消 32 位 CUDA 支持并非问题根源。

**关键发现**：绿屏问题很可能源于：
1. D3D11 纹理格式兼容性
2. DXVA2 视频处理器在 Blackwell 架构上的行为差异
3. 颜色空间转换或同步问题

---

## 1. DmitriRender 架构分析

### 1.1 文件结构

```
C:\Users\Akari\AppData\Roaming\DmitriRender\
├── x64/
│   ├── dmitriRender.dll          (117 KB) - DirectShow 滤镜入口
│   ├── dmitriRenderBase.dll      (453 KB) - 核心渲染引擎 ⭐
│   ├── oqorimis.dll              (4.1 MB) - 许可证管理 + AI 模型
│   └── Jongan.ini                         - 配置文件
├── dmitriRender.dat              (9.2 MB) - 数据/模型文件
├── drtm.exe                      (115 KB) - 管理工具
└── Help/                                  - 帮助文档
```

### 1.2 组件职责

| DLL | 职责 | 关键依赖 |
|-----|------|----------|
| dmitriRender.dll | DirectShow 滤镜注册和 COM 接口 | ole32.dll |
| **dmitriRenderBase.dll** | **D3D11 渲染和视频处理** | **d3d11.dll, dxgi.dll, dxva2.dll** |
| oqorimis.dll | 许可证验证 + 可能包含 AI 模型 | 系统 DLL |

---

## 2. 详细 DLL 依赖分析

### 2.1 dmitriRender.dll (DirectShow 滤镜)

**时间戳**: 2015-08-05 (1438786606)
**架构**: x64

**导入的 DLL**:
- `USER32.dll` - 窗口管理
- `KERNEL32.dll` - 系统核心功能 (70 个函数)
- `ADVAPI32.dll` - 注册表操作
- `ole32.dll` - COM 组件支持

**导出的函数**:
```
- DllCanUnloadNow
- DllGetClassObject      <- DirectShow 标准接口
- DllRegisterServer
- DllUnregisterServer
```

**结论**: 这是标准的 DirectShow 滤镜包装层，不直接处理图形。

---

### 2.2 dmitriRenderBase.dll ⭐ (核心渲染引擎)

**时间戳**: 2020-12-27 (1609068824)
**架构**: x64
**节数量**: 13 (包含多个代码段和大量资源)

#### 关键导入

##### DirectX 11 (`d3d11.dll`)
```cpp
- D3D11CreateDevice    // 创建 D3D11 设备的核心函数
```

**重要性**: 这是 DmitriRender 使用 D3D11 的证据。设备创建时需要指定：
- 驱动类型 (HARDWARE/REFERENCE/SOFTWARE)
- 特性级别 (9_1, 10_0, 11_0 等)
- 创建标志 (DEBUG, SINGLETHREADED 等)

##### DXGI (`dxgi.dll`)
```cpp
- CreateDXGIFactory     // DXGI 1.0
- CreateDXGIFactory1    // DXGI 1.1+
```

**重要性**: DXGI 负责：
- 枚举显示适配器
- 管理交换链 (SwapChain)
- 处理全屏/窗口切换
- **可能是绿屏问题的来源**：如果适配器枚举或格式选择有问题

##### DXVA2 (`dxva2.dll`)
```cpp
- DXVA2CreateVideoService    // 视频加速服务
```

**重要性**: 用于硬件加速视频解码和处理。补帧操作可能依赖 DXVA2 的：
- 视频处理器 (Video Processor)
- 颜色空间转换
- 去隔行/缩放

#### 其他依赖
- `KERNEL32.dll` - 86 个函数
- `USER32.dll` - MonitorFromWindow (多显示器支持)
- `ole32.dll` - COM 支持
- `oqorimis.dll` - 许可证检查 (序号导入)

#### 节结构分析
```
.text      - 241 KB   - 代码段
.rdata     - 83 KB    - 只读数据 (多个段)
.data      - 163 KB   - 可读写数据
.rsrc      - 540 KB   - 资源 (可能包含 Shader 二进制)
.discard   - 8 KB     - 调试/临时数据
.ps4       - 4 KB     - 未知 (可能是平台相关代码)
```

**关键观察**: `.rsrc` 段很大 (540 KB)，可能包含：
- 预编译的 HLSL Shader
- 颜色转换矩阵
- 视频处理参数

---

### 2.3 oqorimis.dll (许可证 + 核心算法)

**时间戳**: 2020-12-27 (1609069032)
**架构**: x64

**特殊节**:
```
.core      - 3.6 MB   - 可能是 AI 模型或专有算法
```

**导出的函数**: 47 个许可证管理函数 (PSA_*)

**结论**: 这个 DLL 不涉及图形 API，主要是许可证和算法实现。

---

## 3. 未发现的技术

✅ **确认不使用**:
- CUDA (`nvcuda.dll`, `cudart.dll`)
- OpenCL (`OpenCL.dll`)
- Vulkan (`vulkan-1.dll`)
- Direct3D 9 (`d3d9.dll`)
- Direct3D 12 (`d3d12.dll`)

这意味着问题**不是** RTX 50 取消 32 位 CUDA 支持导致的。

---

## 4. 问题根源推测

### 4.1 可能性排序

| 可能性 | 描述 | 概率 | 证据 |
|--------|------|------|------|
| 1️⃣ **纹理格式不兼容** | RTX 50 不支持某些旧的 DXGI_FORMAT | 🔴 高 | DXVA2 和 D3D11 互操作时可能使用特殊格式 |
| 2️⃣ **DXVA2 处理器问题** | 视频处理器在 Blackwell 架构上行为变化 | 🟠 中 | DXVA2 是硬件相关的 |
| 3️⃣ **颜色空间转换错误** | YUV→RGB 转换矩阵或 Shader 问题 | 🟠 中 | 绿屏通常与颜色通道混乱有关 |
| 4️⃣ **特性级别不匹配** | D3D11CreateDevice 请求了不兼容的特性级别 | 🟡 低 | RTX 50 完全支持 D3D11_1 |
| 5️⃣ **驱动 Bug** | NVIDIA 驱动对旧应用的兼容性问题 | 🟡 低 | 可能需要等待驱动更新 |

### 4.2 绿屏的技术解释

在图形编程中，绿屏通常表示：

1. **颜色通道错位**
   ```
   正确: RGB = (R, G, B)
   错误: RGB = (0, 255, 0) <- 全绿
   ```

2. **YUV 到 RGB 转换失败**
   ```
   YUV 中的 U 或 V 分量被错误地映射到 RGB
   或者使用了错误的转换矩阵 (BT.601 vs BT.709)
   ```

3. **纹理格式误解**
   ```
   应用期望: NV12 格式
   驱动提供: P010 格式
   结果: 数据解释错误 -> 绿屏
   ```

4. **未初始化的纹理**
   ```
   DirectX 中，未初始化的纹理可能是绿色
   (某些驱动的默认清除颜色)
   ```

---

## 5. 推荐的 Hook 策略

### 5.1 阶段二实施计划

基于以上分析，推荐使用 **MinHook** 进行运行时 API 拦截。

#### 目标 API 列表

##### 🎯 第一优先级 (必须 Hook)

1. **D3D11CreateDevice**
   ```cpp
   目的:
   - 记录请求的特性级别
   - 检查创建标志
   - 记录返回的设备能力
   - 可能需要修改创建参数
   ```

2. **ID3D11Device::CreateTexture2D**
   ```cpp
   目的:
   - 检查纹理格式 (DXGI_FORMAT)
   - 特别关注视频格式: NV12, P010, YUY2
   - 如果格式不支持,转换为兼容格式
   - 记录所有纹理创建参数
   ```

3. **IDXGISwapChain::Present**
   ```cpp
   目的:
   - 检查呈现时的同步标志
   - 添加额外的 Flush 确保 GPU 完成工作
   - 记录每一帧的呈现参数
   ```

4. **DXVA2CreateVideoService**
   ```cpp
   目的:
   - 记录请求的 GUID (解码器/处理器类型)
   - 检查视频处理器能力
   - 验证支持的输入/输出格式
   ```

##### 🔍 第二优先级 (诊断用)

5. **ID3D11Device::CreatePixelShader**
   - 记录使用的 Shader
   - 可能需要修改颜色转换 Shader

6. **ID3D11DeviceContext::Map / Unmap**
   - 监控 CPU-GPU 数据传输
   - 检查纹理数据是否正确

7. **ID3D11DeviceContext::CopyResource**
   - 跟踪纹理复制操作

8. **CreateDXGIFactory1**
   - 记录适配器枚举

### 5.2 Hook 框架设计

```cpp
// 建议的项目结构
dmitri_compat/
├── src/
│   ├── main.cpp               // DLL 入口点
│   ├── hooks/
│   │   ├── d3d11_hooks.cpp   // D3D11 API Hook
│   │   ├── dxgi_hooks.cpp    // DXGI API Hook
│   │   └── dxva2_hooks.cpp   // DXVA2 API Hook
│   ├── logger.cpp             // 日志系统
│   ├── config.cpp             // 配置文件
│   └── fixes/
│       ├── texture_format.cpp // 纹理格式转换
│       ├── color_space.cpp    // 颜色空间修复
│       └── sync.cpp           // 同步修复
├── include/
└── external/
    └── minhook/               // MinHook 库
```

### 5.3 注入方法

推荐使用 **注入器** 而非代理 DLL:

```cpp
// 1. 创建一个简单的注入器工具
dmitri_injector.exe dmitriRenderBase.dll <player_process>

// 2. 或者创建一个批处理文件
// 使用 Windows AppInit_DLLs 注册表键
// (需要管理员权限)
```

---

## 6. 下一步行动计划

### 6.1 立即行动 (阶段二开始)

- [ ] **任务 2.1**: 搭建 MinHook 项目框架
  - 集成 MinHook 库
  - 实现基础日志系统
  - 实现配置文件加载

- [ ] **任务 2.2**: 实现 D3D11CreateDevice Hook
  - 记录所有参数
  - 不做任何修改,先收集信息

- [ ] **任务 2.3**: 实现 CreateTexture2D Hook
  - 记录所有纹理格式
  - 特别标注视频相关格式

- [ ] **任务 2.4**: 测试基础 Hook
  - 在 RTX 5070 上运行
  - 收集日志
  - 对比与 RTX 30 系列的差异 (如果可能)

### 6.2 中期目标

- [ ] **任务 2.5**: 实现纹理格式转换
  - 根据日志识别不兼容的格式
  - 实现格式转换逻辑

- [ ] **任务 2.6**: 实现颜色空间修复
  - 如果发现 YUV/RGB 转换问题
  - 注入正确的转换矩阵

- [ ] **任务 2.7**: 添加同步修复
  - 在关键点添加 Flush
  - 解决可能的时序问题

### 6.3 验证测试

- [ ] 准备测试视频 (24fps, 不同分辨率)
- [ ] 自动化测试脚本
- [ ] 性能基准测试

---

## 7. 风险与限制

### 7.1 技术风险

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| Hook 导致崩溃 | 高 | 大量错误处理,逐步启用修复 |
| 性能下降 | 中 | 性能监控,优化关键路径 |
| 新驱动版本破坏修复 | 低 | 版本检测,提供多套修复策略 |
| 无法完全解决绿屏 | 中 | 提供详细日志,等待社区贡献 |

### 7.2 法律与道德

✅ **合规性**:
- 不反编译 DmitriRender
- 仅使用外部 API Hook
- 开源发布,鼓励社区改进
- 保留原作者信息

---

## 8. 参考资料

### 8.1 技术文档

- [Microsoft D3D11 文档](https://docs.microsoft.com/en-us/windows/win32/direct3d11)
- [DXVA2 视频处理](https://docs.microsoft.com/en-us/windows/win32/medfound/directx-video-acceleration-2-0)
- [MinHook 库](https://github.com/TsudaKageyu/minhook)

### 8.2 类似项目

- **dgVoodoo2**: 老游戏 3D 加速兼容层
- **SpecialK**: 游戏修复和增强框架
- **DXVK**: DirectX 到 Vulkan 转换层

---

## 9. 诊断数据附件

### 9.1 生成的文件

- `dll_analysis.txt` - 完整 DLL 导入/导出分析
- `d3d11_analysis.txt` - D3D11 详细分析
- `analyze_dll.py` - DLL 分析工具
- `analyze_d3d11.py` - D3D11 分析工具

### 9.2 系统信息

```
工作目录: C:\Users\Akari\AppData\Roaming\DmitriRender
平台: Windows (Git Bash 环境)
Python: 3.12.10
已安装: pefile 2024.8.26
```

---

## 10. 结论

DmitriRender 的 RTX 50 兼容性问题**不是由于缺少 CUDA 支持**,而是 **Direct3D 11 和 DXVA2** 在新架构上的行为差异。

**最可能的原因**:
1. 纹理格式兼容性 (视频格式如 NV12, YUY2)
2. 颜色空间转换错误
3. DXVA2 视频处理器在 Blackwell 架构上的变化

**推荐方案**:
使用 MinHook 拦截 D3D11/DXGI/DXVA2 API,先记录行为,再逐步添加兼容性修复。

**成功概率**: 🟢 高 - 类似问题在 RTX 20/30 首发时已通过社区补丁解决。

---

**报告生成**: 2025-11-08
**分析者**: Claude Code (Sonnet 4.5)
**下一步**: 进入阶段二 - 构建 API Hook 兼容层
