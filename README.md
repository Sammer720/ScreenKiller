# ScreenKiller

> Windows 长截图（滚动截屏）与图像标注工具 · Qt 6 + OpenCV 5 + Windows API

![status](https://img.shields.io/badge/status-WIP-orange)
![platform](https://img.shields.io/badge/platform-Windows11-blue)
![qt](https://img.shields.io/badge/Qt-6.11%2B-green)
![opencv](https://img.shields.io/badge/OpenCV-5.x-green)

---

## 1. 项目简介

ScreenKiller 是一款 Windows 桌面端的「截图 + 标注」工具，具备四种截屏模式
与完整的图形标注画布，整体风格遵循 Windows 11 浅色设计语言。

| 能力         | 说明                                                              |
| ----------- | ---------------------------------------------------------------- |
| 画框截屏     | 拖拽框选任意区域，默认模式                                         |
| 全屏截屏     | 一键抓取主屏全部内容                                              |
| 窗口截屏     | 鼠标悬停高亮目标窗口，点击即抓                                    |
| 滚动截屏     | 自动滚轮 + 多帧抓取 + OpenCV 模板匹配拼接，输出长图               |
| 标注画布     | 矩形 / 椭圆 / 直线 / 箭头 / 画笔 / 荧光笔 / 文字，独立 Item 可编辑 |
| 撤销重做     | 自研轻量 Undo/Redo 栈，Ctrl+Z / Ctrl+Y                           |
| 全局快捷键   | Ctrl+Alt+A 唤起截屏                                               |

---

## 2. 工程结构

```
ScreenKiller/
├── CMakeLists.txt                 # 主构建脚本
├── CMakePresets.json              # 内置预设（msvc-debug / msvc-release）
├── CMakeUserPresets.json.example  # 用户预设模板（拷贝后填本机路径）
├── .clangd                        # clangd 配置（Qt6/OpenCV 头文件路径）
├── .editorconfig
├── .gitignore
├── README.md
│
├── .vscode/                       # VSCode 工作区配置
│   ├── c_cpp_properties.json      # Microsoft C++ Extension 配置
│   ├── settings.json              # clangd + cmake-tools 工作区设置
│   ├── tasks.json                 # CMake 构建/清理任务
│   ├── launch.json                # 调试 ScreenKiller.exe
│   └── extensions.json            # 推荐安装的扩展清单
│
├── cmake/                         # 自定义 Find 模块（备用）
│
├── resources/
│   ├── resources.qrc              # Qt 资源清单
│   ├── styles/
│   │   └── windows11_light.qss    # Windows11 浅色 QSS
│   ├── icons/
│   │   └── app.svg                # 应用图标
│   └── translations/
│
├── src/
│   ├── main.cpp                   # 程序入口
│   │
│   ├── app/                       # 应用框架
│   │   ├── MainWindow.h/.cpp      # 主窗口（截屏按钮 + 隐藏视口）
│   │   └── GlobalHotkey.h/.cpp    # RegisterHotKey 封装
│   │
│   ├── capture/                   # 截屏引擎
│   │   ├── CaptureEngine.h/.cpp   # 模式调度器
│   │   ├── RegionSelector.h/.cpp  # 画框选择器（半透明遮罩）
│   │   ├── WindowSelector.h/.cpp  # 窗口高亮选择器
│   │   └── ScrollCapture.h/.cpp   # 滚动截屏主循环
│   │
│   ├── stitcher/                  # OpenCV 图像拼接
│   │   └── ImageStitcher.h/.cpp   # 模板匹配计算重叠 + 垂直拼接
│   │
│   ├── annotation/                # 标注画布
│   │   ├── AnnotationView.h/.cpp  # QGraphicsView 子类
│   │   ├── AnnotationScene.h/.cpp # QGraphicsScene 子类，工具分发
│   │   ├── UndoStack.h/.cpp       # 撤销重做栈
│   │   └── items/                 # 各类标注图元
│   │       ├── BaseAnnotationItem.h
│   │       ├── RectangleItem.h/.cpp
│   │       ├── EllipseItem.h/.cpp
│   │       ├── ArrowItem.h/.cpp
│   │       ├── PenItem.h/.cpp
│   │       ├── HighlighterItem.h/.cpp
│   │       └── TextItem.h/.cpp
│   │
│   ├── platform/                  # Windows API 封装
│   │   └── WinApi.h/.cpp
│   │
│   ├── ui/                        # UI 组件
│   │   ├── ToolBar.h/.cpp         # 顶部工具栏
│   │   └── Style.h/.cpp           # 样式加载
│   │
│   └── utils/                     # 工具
│       ├── ImageUtils.h/.cpp      # QImage <-> cv::Mat
│       └── Logger.h               # 日志宏
│
├── tests/                         # 单元测试（占位）
└── docs/                          # 文档目录
```

---

## 3. 依赖准备

### 3.1 Qt 6.11+

下载安装 **Qt 6.11+ MSVC 2022 64-bit**：
- 官方下载：<https://www.qt.io/download-open-source>
- 安装时勾选 `Qt 6.11.1 → MSVC 2022 64-bit` + `Developer and Designer Tools → CMake` + `Ninja`

记下安装路径，例如：`F:/ForVS/Qt/6.11.1/msvc2022_64`

### 3.2 OpenCV 5.x

任选其一：

**A. 官方预编译包**（最省事，本项目默认适配）
- 下载：<https://github.com/opencv/opencv/releases>
- 解压到 `F:/ForVS/OpenCV/opencv/build`，包含 `include/`、`x64/vc16/bin/`、`x64/vc16/lib/`
- 注意：官方包采用 **opencv_world 单体库**模式，只有一个 `opencv_world500.lib`

**B. 自行编译**（CMake + MSVC）
```bash
git clone https://github.com/opencv/opencv.git
cd opencv
cmake -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_INSTALL_PREFIX=F:/ForVS/OpenCV/install \
      -DBUILD_SHARED_LIBS=ON \
      -DWITH_OPENGL=OFF -DWITH_D3D=OFF \
      -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF
cmake --build build --config Release --target install
```

记下安装路径，例如：`F:/ForVS/OpenCV/opencv/build` 或 `F:/ForVS/OpenCV/install`

### 3.3 编译工具链

- **Visual Studio 2022**（含 C++ 桌面开发工作负载）—— 本机路径 `F:/ForVS/Product`
- **CMake 3.21+** —— Qt 自带 `F:/ForVS/Qt/Tools/CMake_64/bin`
- **Ninja** —— Qt 自带 `F:/ForVS/Qt/Tools/Ninja`
- **VSCode 扩展**（详见 `.vscode/extensions.json`）：
  - `llvm-vs-code-extensions.vscode-clangd`
  - `ms-vscode.cpptools`
  - `ms-vscode.cmake-tools`
  - `twxs.cmake`

---

## 4. 配置本机路径

### 4.1 方式 A：设置用户环境变量（推荐，一劳永逸）

已在项目初始化时帮你设置好以下用户级环境变量（永久生效）：

```powershell
[Environment]::SetEnvironmentVariable("Qt6_DIR",    "F:\ForVS\Qt\6.11.1\msvc2022_64", "User")
[Environment]::SetEnvironmentVariable("OpenCV_DIR", "F:\ForVS\OpenCV\opencv\build",   "User")
```

设置后**重启 VSCode**（或新开终端），CMake 会自动从环境变量找到 Qt6 和 OpenCV。
`CMakeUserPresets.json` 中无需再重复填写路径，只保留构建类型和生成器。

### 4.2 方式 B：在 CMakeUserPresets.json 中显式填写

如果不想设环境变量，也可以在 `CMakeUserPresets.json` 的 `cacheVariables` 中加上：
```json
"Qt6_DIR":    "F:/ForVS/Qt/6.11.1/msvc2022_64",
"OpenCV_DIR": "F:/ForVS/OpenCV/opencv/build"
```

### 4.3 .clangd 与 c_cpp_properties.json

这两个文件已预填好本机路径，用于 clangd 和 C++ Extension 的索引。
首次执行 `cmake --preset user-debug` 生成 `compile_commands.json` 后，clangd 会自动接管。

---

## 5. 构建与运行

### 5.1 命令行

> **前提**：已在「系统属性 → 环境变量」中设置 `Qt6_DIR` 和 `OpenCV_DIR`（用户级，
> 已在项目初始化时帮你设好）。新开终端后 CMake 会自动读取。

```bash
# 配置
cmake --preset user-debug

# 构建
cmake --build --preset user-debug

# 运行
./build/user-debug/bin/ScreenKiller.exe
```

> **MSVC 编译器说明**：命令行直接跑 cmake 需要 `cl.exe` 在 PATH 中。
> 如果报「cl is not a full path」，请先在「VS x64 Native Tools Command Prompt」
> 里执行上述命令，或直接用 VSCode（见 5.2，cmake-tools 会自动加载 MSVC 环境）。

### 5.2 VSCode

1. 打开项目根目录
2. `Ctrl+Shift+P` → `CMake: Select Configure Preset` → `user-debug`
3. `Ctrl+Shift+P` → `CMake: Select a Kit` → 选择 `Visual Studio ... amd64`（不要选 GCC/Clang）
4. `CMake: Build` 或 `F7`
5. `F5` 调试运行（launch.json 已配置好 MSVC 环境 + Qt/OpenCV DLL 路径）

> **注意**：如果 cmake-tools 自动探测的 kit 导致链接器被 w64devkit 劫持，
> 请在 `.vscode/settings.json` 中已配置 `cmake.useCMakePresets: "always"`，
> 这样会使用 preset 中的 `CMAKE_LINKER=link` / `CMAKE_AR=lib`，绕过 PATH 污染。

### 5.3 VSCode 任务

`Ctrl+Shift+B` 选择任务：
- `CMake: 构建 (Debug)`
- `CMake: 构建 (Release)`
- `CMake: 清理`
- `重新生成 compile_commands.json`

---

## 6. 使用说明

### 6.1 启动

启动后主窗口只显示：
- 截屏按钮（含模式下拉）
- 最小化按钮
- 关闭按钮
- 主体区域为隐藏的标注视口

### 6.2 截屏

- **快捷键** `Ctrl + Alt + A`（与点击截屏按钮等效）
- 点击截屏按钮右侧下拉箭头切换模式：
  - 画框截屏（默认）
  - 全屏截屏
  - 窗口截屏
  - 滚动截屏

### 6.3 标注

截屏完成后，标注视口自动展开，左侧/顶部提供工具切换（占位，
实际工具箱 UI 在后续迭代中接入 `ToolBar` 的二级面板）。

| 操作                  | 效果                          |
| -------------------- | ----------------------------- |
| 左键拖拽              | 用当前工具绘制图元            |
| 点击图元              | 选中                          |
| 拖动选中图元          | 移动                          |
| Delete                | 删除选中                      |
| Ctrl+Z / Ctrl+Y       | 撤销 / 重做                   |
| Ctrl + 滚轮           | 缩放画布                      |
| 双击文字图元          | 编辑文字内容                  |

### 6.4 托盘

关闭主窗口会最小化到系统托盘，右键托盘图标可：
- 显示主窗口
- 截屏
- 退出

---

## 7. 关键实现说明

### 7.1 全局快捷键

- `RegisterHotKey(hwnd, id, MOD_CONTROL|MOD_ALT, 'A')` 注册
- `MainWindow::nativeEvent` 拦截 `WM_HOTKEY`，按 `wParam` 区分 id
- 退出前必须 `UnregisterHotKey`，否则句柄泄漏

### 7.2 滚动截屏算法

```
for i in 0..maxFrames:
    frame = QScreen::grabWindow(0).copy(targetRect)
    if frame == lastFrame:        # 滚动到底
        break
    frames.append(frame)
    SendInput(MOUSEEVENTF_WHEEL, -WHEEL_DELTA * scrollLines / 3)
    sleep(frameIntervalMs)

merged = ImageStitcher.stitchVertical(frames)
```

拼接核心：取上一帧底部 64px 作为模板，在下一帧上半部分做 `cv::matchTemplate`
(`TM_CCOEFF_NORMED`)，最大响应位置即重叠起点，去掉重叠部分垂直堆叠。

### 7.3 标注图元

所有图元继承 `SK::BaseAnnotationItem`（继承 `QGraphicsItem`），
统一管理 `QPen` / `QBrush`，并实现 `type()` 区分种类。
`AnnotationScene` 持有当前工具枚举，在 `mousePressEvent` 中创建对应 Item，
`mouseMoveEvent` 更新几何，`mouseReleaseEvent` 提交到 `UndoStack`。

### 7.4 撤销重做

`UndoStack` 持有两个 `QStack<std::unique_ptr<ICommand>>`，
- `push()` 执行 `redo()` 并入栈
- `undo()` 弹出并执行 `undo()`，压入 redo 栈
- `redo()` 反之

每个 `AddItemCommand` 持有 `QGraphicsItem*` 指针，
`redo` 调用 `scene->addItem`，`undo` 调用 `scene->removeItem`，
**不 delete 指针**，保证撤销时图元仍可恢复。

---

## 8. 常见问题

### 8.1 clangd 报红「找不到 QtWidgets/QWidget」

1. 检查 `.clangd` 中的 Qt6 include 路径是否正确
2. 确认已执行 `cmake --preset user-debug` 生成了 `compile_commands.json`
3. 在 VSCode 中重启 clangd：`Ctrl+Shift+P` → `clangd: Restart language server`

### 8.2 链接错误「无法找到 Qt6Core.lib」

`Qt6_DIR` 未指向带 `lib/cmake/Qt6/` 的目录。
正确示例：`F:/ForVS/Qt/6.11.1/msvc2022_64`（该目录下应有 `lib/cmake/Qt6/Qt6Config.cmake`）

### 8.3 CMake 报「Could NOT find OpenCV (missing: OpenCVConfig.cmake)」

**原因**：`OpenCV_DIR` 未设置，或指向的目录没有 `OpenCVConfig.cmake`。

**Windows 上的坑**：OpenCV 官方预编译包有两个 `OpenCVConfig.cmake`：
- `<build>/OpenCVConfig.cmake` —— **分发器**，需要额外的 `OpenCV_ARCH` + `OpenCV_RUNTIME` 变量
- `<build>/x64/vc16/lib/OpenCVConfig.cmake` —— **完整版**，可直接使用

本项目 CMakeLists.txt 已自动处理：用户只需把 `OpenCV_DIR` 指向 `<build>` 根目录，
脚本会自动细化到 `x64/vc16/lib` 子目录。

正确示例：`OpenCV_DIR = "F:/ForVS/OpenCV/opencv/build"`

### 8.4 CMake 报「Could NOT find OpenCV (missing: features2d)」

**原因**：OpenCV 官方 Windows 预编译包采用 **opencv_world 单体库**模式，
只导出 `opencv_world` 这一个 imported target，没有独立的 `opencv_features2d` 等。

**解决**：本项目 CMakeLists.txt 使用 `find_package(OpenCV REQUIRED)`（**不带 COMPONENTS**），
链接 `${OpenCV_LIBS}` 即可，world 库已包含 core/imgproc/imgcodecs/features2d 等全部模块。

### 8.5 CMake 报「cl is not a full path and was not found in the PATH」

**原因**：未加载 MSVC 开发环境（`cl.exe` 不在 PATH）。

**解决**：
- **VSCode**：用 cmake-tools 扩展，`CMake: Select a Kit` → 选 `Visual Studio ... amd64`，
  扩展会自动加载 MSVC 环境
- **命令行**：在「VS x64 Native Tools Command Prompt」中执行 cmake 命令，
  或先 `call vcvars64.bat`

### 8.6 CMake 报「ld.exe: cannot find /nologo」或链接器异常

**原因**：PATH 中的 `w64devkit/bin` 或 `mingw/bin` 抢占了 MSVC 的 `link.exe`。

**解决**：
- **VSCode**：选对 Kit（Visual Studio amd64）后，cmake-tools 会正确设置 MSVC 环境优先
- **命令行**：在 vcvars64.bat 加载后，确保 MSVC 工具链在 PATH 中靠前
- **兜底**：在 `CMakeUserPresets.json` 的 cacheVariables 中加上：
  ```json
  "CMAKE_LINKER": "link",
  "CMAKE_AR":     "lib"
  ```

### 8.7 运行时崩溃「无法定位程序输入点 opencv_core500.dll」

OpenCV DLL 未在 PATH 中。三种解法：
- 让 CMake 自动拷贝（CMakeLists.txt 中已配置 `copy_directory`，会拷贝 `x64/vc16/bin` 到输出目录）
- 手动把 `F:/ForVS/OpenCV/opencv/build/x64/vc16/bin` 加入系统 PATH
- VSCode 调试时 `launch.json` 的 `environment.PATH` 已配置好

---

## 9. 后续路线图

- [ ] 工具属性面板（颜色 / 线宽 / 字体）接入 ToolBar
- [ ] 图元缩放手柄（Corner handle）
- [ ] ORB 特征点匹配作为模板匹配的回退方案
- [ ] 长图导出为 PNG / JPEG / PDF
- [ ] 国际化（i18n）支持
- [ ] 多显示器分别截屏

---

## 10. 许可证

MIT License · Copyright (c) 2026 Sammer
