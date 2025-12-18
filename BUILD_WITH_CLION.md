# 使用 CLion 构建 DmitriCompat

## 步骤

1. **打开 CLion**

2. **打开项目**
   - File → Open
   - 选择 `C:\Users\Akari\AppData\Roaming\DmitriRender\dmitri_compat`
   - 点击 OK

3. **等待 CMake 配置完成**
   - CLion 会自动检测 CMakeLists.txt
   - 在底部会显示 CMake 配置进度
   - 等待配置完成（可能需要 1-2 分钟）

4. **构建项目**
   - 点击顶部工具栏的 **Build** → **Build Project**
   - 或者按快捷键 **Ctrl+F9**

5. **查找生成的 DLL**
   - 位置：`cmake-build-debug/bin/dmitri_compat.dll`
   - 或者：`cmake-build-release/bin/dmitri_compat.dll`（如果选择了 Release 配置）

6. **复制 DLL 和配置文件**
   ```bash
   # 在项目目录下运行
   mkdir -p build/bin
   cp cmake-build-debug/bin/dmitri_compat.dll build/bin/
   cp -r config build/bin/
   mkdir -p build/bin/logs
   ```

## 完成！

现在可以使用 `python injector.py` 进行注入测试了。
