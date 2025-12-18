# DmitriRender RTX 50系兼容性修复项目

## 项目背景

DmitriRender是一个优秀的视频补帧滤镜，可以将24fps视频补帧到165fps且伪影极少。但在RTX 50系显卡（Blackwell架构）上运行时出现绿屏问题，怀疑是缺少对新架构的支持。

## 核心问题

- **症状**：在RTX 5070显卡上使用DmitriRender播放视频时，画面全绿
- **原因推测**：RTX 50系取消了32位OpenCL/CUDA支持，且可能存在DirectX API兼容性问题
- **历史先例**：RTX 20/30系列首发时也有类似问题，通过社区补丁解决
- **现状**：DmitriRender已停止开发，官网无法访问

## 修复策略

**采用API Hook方式**（而非反编译DLL），在外部构建兼容层拦截并修复DirectX/CUDA调用。

---

## 阶段一：环境诊断与信息收集

### 任务1.1：分析DmitriRender的依赖和API使用

```bash
# 请执行以下诊断步骤：

1. 使用 Dependency Walker 或 dumpbin 分析 DmitriRender.dll
   - 列出所有导入的DLL
   - 识别使用的DirectX版本（D3D9/D3D11/D3D12）
   - 检查是否有CUDA/OpenCL依赖

2. 使用 Process Monitor 监控运行时行为
   - 记录DLL加载顺序
   - 捕获注册表访问
   - 监控文件系统操作
   - 查找可能的日志文件或配置文件

3. 检查安装目录结构
   - 列出所有相关文件
   - 查找配置文件、日志文件
   - 检查是否有调试模式开关
```

**期望输出：**
- DmitriRender使用的图形API清单
- 依赖的CUDA/OpenCL版本
- 可能的调试入口点

### 任务1.2：使用RenderDoc捕获绿屏帧

```bash
# 如果RenderDoc可用，请：

1. 启动RenderDoc
2. 附加到使用DmitriRender的播放器进程
3. 在出现绿屏时捕获一帧
4. 分析捕获的调用栈：
   - 查看所有Draw Call
   - 检查纹理格式
   - 查看Shader输入输出
   - 识别异常的API调用

5. 特别关注：
   - 颜色空间转换（YUV/RGB）
   - 纹理格式是否被支持
   - Render Target状态
   - Present链的正确性
```

**期望输出：**
- 绿屏发生时的完整API调用序列
- 异常的纹理格式或参数
- 可疑的API调用点

---

## 阶段二：构建API Hook兼容层

### 任务2.1：创建DirectX Hook框架

基于诊断结果，选择合适的Hook方案：

#### 方案A：D3D9 Wrapper（如果使用D3D9）

```cpp
// 创建文件：d3d9_hook/d3d9.cpp
// 目标：创建一个代理DLL，拦截所有D3D9调用

/*
需求：
1. 加载真正的系统d3d9.dll
2. 导出所有d3d9标准接口
3. 转发调用到真实DLL
4. 在关键API上添加兼容性修复

关键拦截点：
- Direct3DCreate9
- IDirect3DDevice9::Present
- IDirect3DDevice9::CreateTexture
- IDirect3DDevice9::SetRenderTarget
- IDirect3DDevice9::StretchRect
- IDirect3DDevice9::SetPixelShader
*/

// 请实现完整的D3D9代理DLL框架
// 包含日志记录功能，方便调试
```

#### 方案B：使用MinHook进行运行时Hook（通用方案）

```cpp
// 创建文件：dmitri_compat/main.cpp
// 使用MinHook库进行运行时API拦截

/*
需求：
1. 集成MinHook库
2. Hook关键DirectX API
3. Hook可能的CUDA/OpenCL调用
4. 实现兼容性修复逻辑
5. 提供详细的调试日志

请实现：
- Hook框架初始化
- 目标API的拦截函数
- 日志系统
- 配置文件支持（可以开关不同的修复）
*/
```

### 任务2.2：实现常见绿屏问题的修复

```cpp
// 请实现以下修复策略：

// 修复1：纹理格式兼容性
/*
问题：旧的纹理格式可能不被RTX 50支持
解决：拦截CreateTexture，转换格式
*/
HRESULT WINAPI Hook_CreateTexture(
    IDirect3DDevice9* device,
    UINT Width, UINT Height, UINT Levels,
    DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle
) {
    // 检测并转换不兼容的格式
    // 例如：YUY2 -> X8R8G8B8
    // 记录转换日志
}

// 修复2：颜色空间转换
/*
问题：YUV到RGB的转换可能有问题
解决：在Present前后添加颜色空间校正
*/
HRESULT WINAPI Hook_Present(
    IDirect3DDevice9* device,
    CONST RECT* pSourceRect,
    CONST RECT* pDestRect,
    HWND hDestWindowOverride,
    CONST RGNDATA* pDirtyRegion
) {
    // 添加颜色矩阵校正
    // 强制刷新GPU
    // 调用原始Present
}

// 修复3：GPU同步问题
/*
问题：异步操作可能导致帧数据错乱
解决：添加适当的同步点
*/

// 修复4：Shader常量寄存器
/*
问题：可能访问了RTX 50不支持的寄存器
解决：重映射寄存器索引
*/
```

### 任务2.3：CUDA/OpenCL兼容层（如需要）

```cpp
// 如果诊断发现使用了CUDA/OpenCL：

/*
问题：RTX 50取消了32位CUDA支持
可能的解决方案：
1. 拦截CUDA初始化，强制使用64位上下文
2. Hook cuLaunchKernel，检查参数合法性
3. 实现CUDA到OpenCL的转换层
*/

// 请基于诊断结果实现必要的CUDA Hook
```

---

## 阶段三：测试与迭代优化

### 任务3.1：构建测试框架

```bash
# 创建自动化测试流程：

1. 准备测试视频样本
   - 24fps标准视频
   - 不同编码格式（H.264, H.265）
   - 不同分辨率（1080p, 4K）

2. 编写测试脚本
   - 自动加载Hook DLL
   - 启动播放器和DmitriRender
   - 截图并验证是否为绿屏
   - 记录性能指标（CPU/GPU使用率、帧时间）

3. 对比测试
   - Hook前后的行为差异
   - 不同修复策略的效果
```

### 任务3.2：逐步启用修复并验证

```cpp
// 实现配置文件系统：

/*
config.ini示例：
[Fixes]
EnableTextureFormatConversion=1
EnableColorSpaceCorrection=1
EnableGPUSync=1
EnableShaderRegisterRemap=0

[Debug]
LogLevel=2  ; 0=None, 1=Error, 2=Info, 3=Verbose
DumpTextures=0
DumpShaders=0
*/

// 请实现：
// 1. 配置文件解析
// 2. 运行时可切换的修复开关
// 3. 详细的诊断日志
```

### 任务3.3：性能优化

```cpp
// 优化目标：
// - Hook开销 < 1ms/frame
// - 不影响DmitriRender的补帧性能
// - 内存占用增加 < 50MB

// 请实现：
// 1. 缓存机制（避免重复转换）
// 2. 多线程优化（如果需要）
// 3. 性能监控点
```

---

## 阶段四：打包与发布

### 任务4.1：创建安装器

```bash
# 创建用户友好的安装包：

1. 文件结构：
   dmitri_compat/
   ├── README.md
   ├── config.ini
   ├── dmitri_hook.dll
   ├── install.bat
   └── uninstall.bat

2. install.bat功能：
   - 检测DmitriRender安装位置
   - 备份原始文件
   - 复制Hook DLL到正确位置
   - 配置环境变量或注册表

3. 提供详细文档：
   - 安装步骤
   - 配置选项说明
   - 故障排除指南
   - 已知问题列表
```

### 任务4.2：编写文档

```markdown
# 请创建以下文档：

## README.md
- 项目介绍
- 技术原理简述
- 安装使用指南
- 兼容性列表（测试过的显卡/播放器）

## TROUBLESHOOTING.md
- 常见问题FAQ
- 调试方法
- 日志分析指南

## TECHNICAL.md
- 详细的技术实现
- Hook点说明
- 修复原理解释
- 开发者文档
```

---

## 重要约束与注意事项

### 法律与道德

1. **不反编译DmitriRender.dll** - 仅使用外部Hook
2. **保留原作者信息** - 在文档中注明这是兼容性补丁
3. **开源发布** - 建议使用MIT或Apache 2.0许可证

### 技术约束

1. **向后兼容** - 确保在旧显卡上也能正常工作
2. **性能优先** - Hook不应显著影响补帧性能
3. **稳定性** - 宁可功能受限也不能崩溃
4. **可调试** - 提供详细日志，方便社区改进

### 开发原则

1. **迭代开发** - 先解决绿屏，再优化性能
2. **充分测试** - 每个修复都要验证副作用
3. **文档完整** - 记录每个设计决策
4. **社区友好** - 代码清晰，易于其他人贡献

---

## 预期交付物

### 最小可行产品（MVP）

- [ ] 能够拦截DmitriRender的DirectX调用
- [ ] 解决RTX 50系列的绿屏问题
- [ ] 提供基础的日志和配置功能
- [ ] 包含安装脚本和简单文档

### 完整版本

- [ ] 支持多种修复策略切换
- [ ] 详细的性能监控
- [ ] 完善的文档和FAQ
- [ ] 在多个配置上测试通过
- [ ] 可选的GUI配置工具

---

## 调试建议

### 如果Hook后仍然绿屏

```bash
1. 检查Hook是否真的生效
   - 在拦截函数中添加MessageBox或日志
   - 确认DLL被正确加载

2. 逐个禁用修复
   - 找出哪个修复引入了新问题

3. 使用GPU调试工具
   - NVIDIA Nsight Graphics
   - 捕获完整的GPU状态

4. 对比正常工作的显卡
   - 如果有RTX 30系列，对比API调用差异
```

### 如果性能下降明显

```bash
1. Profile Hook函数
   - 找出耗时的操作
   - 添加缓存机制

2. 减少不必要的转换
   - 只在确实需要时才修复

3. 考虑异步处理
   - 将非关键操作移到后台线程
```

---

## 开始行动

请按以下顺序执行：

1. **立即开始阶段一的诊断工作**
2. **分析结果后决定具体的Hook方案**
3. **实现最小修复，验证能否解决绿屏**
4. **迭代优化，逐步完善功能**

在每个阶段完成后，报告进展和发现的问题，我会提供进一步的指导。

---

## 额外资源

### 推荐工具

- **MinHook**: https://github.com/TsudaKageyu/minhook
- **Microsoft Detours**: https://github.com/microsoft/Detours
- **RenderDoc**: https://renderdoc.org/
- **Process Monitor**: Sysinternals套件
- **Dependency Walker**: dependencywalker.com

### 参考项目

- **DXVK**: DirectX到Vulkan的转换层
- **dgVoodoo2**: 老游戏的3D加速兼容层
- **SpecialK**: 通用游戏修复框架

### 社区资源

- **Doom9论坛**: 视频处理社区
- **NVIDIA开发者论坛**: GPU相关问题

---

**祝你成功复活DmitriRender！这将是对视频处理社区的重要贡献。**