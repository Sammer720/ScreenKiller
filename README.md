# ScreenKiller

> Windows 长截图（滚动截屏）与图像标注工具 · Qt 6 + OpenCV 5 + Windows API

![status](https://img.shields.io/badge/status-WIP-orange)
![platform](https://img.shields.io/badge/platform-Windows11-blue)
![qt](https://img.shields.io/badge/Qt-6.11%2B-green)
![opencv](https://img.shields.io/badge/OpenCV-5.x-green)

---

这是一个集合了常规截屏（全屏截屏、窗口截屏、画框截屏）和滚动长截屏的截屏小工具。你可以根据以下内容构建或编译自己的分支，也可以直接下载发布（release）版本。



## 1. 快速开始

### 1.1 依赖准备

| 依赖                     | 版本    | 说明                                        |
| ---------------------- | ----- | ----------------------------------------- |
| **Visual Studio 2022** | 17+   | 含「C++ 桌面开发」工作负载，提供 MSVC 编译器 `cl.exe`      |
| **Qt**                 | 6.11+ | 安装 `MSVC 2022 64-bit` 组件，附带 CMake + Ninja |
| **OpenCV**             | 5.x   | 官方预编译包或自行编译，采用 **opencv_world 单体库**模式     |
| **CMake**              | 3.21+ | Qt 自带或单独安装                                |

### 1.2 设置本机路径

**推荐：设置用户环境变量（一劳永逸）**

```powershell
[Environment]::SetEnvironmentVariable("Qt6_DIR",    "F:/ForVS/Qt/6.11.1/msvc2022_64", "User")
[Environment]::SetEnvironmentVariable("OpenCV_DIR", "F:/ForVS/OpenCV/opencv/build",   "User")
```

设置后**重启终端/VSCode**，CMake 自动读取。

**备选：在 `CMakeUserPresets.json` 中填写**

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "user-debug",
            "inherits": "msvc-debug",
            "cacheVariables": {
                "Qt6_DIR":    "F:/ForVS/Qt/6.11.1/msvc2022_64",
                "OpenCV_DIR": "F:/ForVS/OpenCV/opencv/build"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "user-debug",
            "configurePreset": "user-debug"
        }
    ]
}
```

> 将 `CMakeUserPresets.json.example` 拷贝为 `CMakeUserPresets.json` 后修改路径即可。

### 1.3 构建与运行

**命令行（需在 VS x64 Native Tools Command Prompt 中执行）：**

```bash
# 配置
cmake --preset user-debug

# 构建
cmake --build --preset user-debug

# 运行
./build/user-debug/bin/ScreenKiller.exe
```

**VSCode（推荐）：**

1. 打开项目根目录
2. `Ctrl+Shift+P` → `CMake: Select Configure Preset` → `user-debug`
3. `Ctrl+Shift+P` → `CMake: Select a Kit` → `Visual Studio ... amd64`
4. `F7` 构建，`F5` 调试运行

> **注意**：如果 PATH 中混入 w64devkit/mingw 劫持了 `link.exe`，CMakePresets 已通过 `CMAKE_LINKER=link` / `CMAKE_AR=lib` 规避。确保 cmake-tools 使用 CMakePresets（已在 `.vscode/settings.json` 中配置 `cmake.useCMakePresets: "always"`）。

**VSCode 任务（`Ctrl+Shift+B`）：**

- `CMake: 构建 (Debug)`
- `CMake: 构建 (Release)`
- `CMake: 清理`
- `重新生成 compile_commands.json`

### 1.4 验证安装

启动后应看到：

- 主窗口显示截屏按钮 + 模式下拉 + 最小化/关闭按钮
- 按下 `Ctrl+Alt+A` 进入截屏模式
- 关闭窗口后缩到系统托盘，托盘右键菜单可退出

---

## 2. 使用方式

### 2.1 截屏模式

| 模式           | 快捷键     | 操作方式                                       |
| ------------ | ------- | ------------------------------------------ |
| **画框截屏**（默认） | 截屏后拖拽框选 | 半透明遮罩 → 拖拽矩形选区 → 释放即截取                     |
| **全屏截屏**     | 一键截取    | 直接抓取主屏全部内容                                 |
| **窗口截屏**     | 鼠标悬停选择  | 遮罩上移动鼠标 → 下方窗口自动高亮 → 点击截取                  |
| **滚动截屏**     | 框选后手动滚动 | 框选区域 → 出现提示浮窗 → 滚动目标窗口 → 按 Enter 或点击「完成」拼接 |

**全局快捷键：** `Ctrl + Alt + A` — 任何时候按下立即进入当前选中的截屏模式。

**模式切换：** 点击截屏按钮右侧的下拉箭头，或按 `Tab` 键循环切换。

### 2.2 标注画布

截屏完成后自动进入标注视图：

| 操作                  | 效果        |
| ------------------- | --------- |
| 左键拖拽                | 用当前工具绘制图元 |
| 点击图元                | 选中        |
| 拖动选中图元              | 移动位置      |
| `Delete`            | 删除选中图元    |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做   |
| `Ctrl + 滚轮`         | 缩放画布      |
| 双击文字图元              | 编辑文字内容    |

**标注工具：** 选择 / 矩形 / 椭圆 / 箭头 / 直线 / 自由画笔 / 荧光笔 / 文字。

### 2.3 系统托盘

- **关闭窗口** → 最小化到系统托盘（不退出）
- **右键托盘图标** → 显示主窗口 / 截屏 / 退出

---

## 3. 项目简介

ScreenKiller 是一款 Windows 桌面端的「截图 + 标注」工具，具备四种截屏模式与完整的图形标注画布，整体风格遵循 Windows 11 浅色设计语言。

| 能力    | 说明                                            |
| ----- | --------------------------------------------- |
| 画框截屏  | 拖拽框选任意区域，默认模式                                 |
| 全屏截屏  | 一键抓取主屏全部内容                                    |
| 窗口截屏  | 鼠标悬停高亮目标窗口，点击即抓                               |
| 滚动截屏  | 手动滚动驱动抓帧 + OpenCV 模板匹配拼接，输出长图                 |
| 标注画布  | 矩形 / 椭圆 / 直线 / 箭头 / 画笔 / 荧光笔 / 文字，独立 Item 可编辑 |
| 撤销重做  | 自研轻量 Undo/Redo 栈，`Ctrl+Z` / `Ctrl+Y`          |
| 全局快捷键 | `Ctrl+Alt+A` 唤起截屏                             |

---

## 4. 工程结构

```
ScreenKiller/
├── CMakeLists.txt                 # 主构建脚本（359 行）
├── CMakePresets.json              # 内置预设（msvc-debug / msvc-release）
├── CMakeUserPresets.json          # 用户预设（gitignored）
├── .clangd                        # clangd 配置（Qt6/OpenCV 头文件路径）
├── .editorconfig                  # 4 空格 / UTF-8 / LF / 去尾空白
├── AGENTS.md                      # OpenCode 会话速通指南
│
├── .vscode/                       # VSCode 工作区配置
│   ├── c_cpp_properties.json
│   ├── settings.json
│   ├── tasks.json
│   ├── launch.json
│   └── extensions.json
│
├── resources/
│   ├── resources.qrc              # Qt 资源清单（17 个文件）
│   ├── styles/windows11_light.qss # Windows 11 浅色 QSS
│   └── icons/                     # 14 个 PNG 图标
│
├── src/
│   ├── main.cpp                   # 程序入口
│   │
│   ├── app/                       # 应用框架
│   │   ├── MainWindow.h/.cpp      # 主窗口（FramelessWindowHint 自绘）
│   │   └── GlobalHotkey.h/.cpp    # RegisterHotKey 封装
│   │
│   ├── capture/                   # 截屏引擎
│   │   ├── CaptureEngine.h/.cpp   # 模式调度器（4 种模式）
│   │   ├── RegionSelector.h/.cpp  # 画框选择器（半透明遮罩 + 拖拽）
│   │   ├── WindowSelector.h/.cpp  # 窗口高亮选择器
│   │   ├── ScrollCapture.h/.cpp   # 手动滚动截屏（滚轮监听 + 抓帧）
│   │   ├── ScrollOverlay.h/.cpp   # 滚动截屏提示浮窗
│   │   └── FrameQueue.h/.cpp      # 线程安全帧队列
│   │
│   ├── stitcher/                  # OpenCV 图像拼接
│   │   ├── ImageStitcher.h/.cpp   # 模板匹配垂直拼接（含 ORB 回退）
│   │   └── StitchWorker.h/.cpp    # 拼接工作线程封装
│   │
│   ├── annotation/                # 标注画布
│   │   ├── AnnotationView.h/.cpp  # QGraphicsView 子类
│   │   ├── AnnotationScene.h/.cpp # QGraphicsScene 子类，工具分发
│   │   ├── UndoStack.h/.cpp       # 自研撤销重做栈
│   │   └── items/                 # 7 种标注图元
│   │       ├── BaseAnnotationItem.h
│   │       ├── RectangleItem.h/.cpp
│   │       ├── EllipseItem.h/.cpp
│   │       ├── ArrowItem.h/.cpp
│   │       ├── PenItem.h/.cpp
│   │       ├── HighlighterItem.h/.cpp
│   │       └── TextItem.h/.cpp
│   │
│   ├── platform/                  # Windows API 封装
│   │   ├── WinApi.h/.cpp          # 窗口/输入/屏幕工具函数
│   │   ├── MouseWheelHook.h/.cpp  # WH_MOUSE_LL 全局滚轮钩子
│   │   └── KeyboardHook.h/.cpp    # WH_KEYBOARD_LL 全局键盘钩子
│   │
│   ├── ui/                        # UI 组件
│   │   ├── ToolBar.h/.cpp         # 顶部工具栏（兼标题栏拖拽）
│   │   └── Style.h/.cpp           # Windows 11 浅色 QSS 加载
│   │
│   └── utils/                     # 工具
│       ├── ImageUtils.h/.cpp      # QImage ↔ cv::Mat 互转
│       ├── Logger.h/.cpp          # QLoggingCategory 分类日志宏
│       └── MessageBox.h           # 中文按钮消息框（header-only）
│
├── tests/                         # 单元测试（待填充）
└── docs/                          # 文档（待填充）
```

---

## 5. 技术架构

### 5.1 调用链路

```
main.cpp
  └→ MainWindow
       ├→ setupUi()          → QStackedWidget[Placeholder | AnnotationView]
       ├→ setupToolBar()     → ToolBar（截屏/模式/保存/最小化/关闭）
       ├→ setupTrayIcon()    → QSystemTrayIcon + 右键菜单
       └→ registerHotkeys()  → GlobalHotkey(Ctrl+Alt+A) → nativeEvent() 分发
            │
            ▼ 用户点击截屏 / Ctrl+Alt+A
       CaptureEngine::start(mode)
            │
            ├→ Region     → RegionSelector → grabRegion
            ├→ FullScreen → QScreen::grabWindow
            ├→ Window     → WindowSelector → grabWindow
            └→ Scrolling  → RegionSelector → MouseWheelHook → FrameQueue
                             → StitchWorker(ImageStitcher) → 拼接
            │
            ▼ captureFinished(QImage)
       MainWindow::onCaptureFinished()
            → AnnotationScene::loadImage()
            → 切换到标注视图
            → 用户标注 → UndoStack 管理撤销/重做
            → 用户保存 → AnnotationScene::exportImage()
```

### 5.2 全局快捷键

- `RegisterHotKey(hwnd, id, MOD_CONTROL|MOD_ALT, 'A')` 注册 `Ctrl+Alt+A`
- `MainWindow::nativeEvent` 拦截 `WM_HOTKEY`，按 `wParam` 区分快捷键 ID
- `GlobalHotkey` 析构时自动 `UnregisterHotKey`，避免句柄泄漏

### 5.3 滚动截屏算法

```
1. 用户框选目标区域，显示 ScrollOverlay 提示浮窗
2. 安装 MouseWheelHook 监听全局滚轮，累积滚动像素
3. 达到阈值（默认 10px）→ 抓取一帧 → FrameQueue.enqueue
4. 工作线程从 FrameQueue.dequeue 处理帧（重复检测 + 转灰度）
5. 用户按 Enter 或点击「完成」→ 进入拼接阶段
6. StitchWorker 调用 ImageStitcher.stitchVertical()：
   a. 取上一帧底部 64px strip 作为模板
   b. 在下一帧上半部 cv::matchTemplate(TM_CCOEFF_NORMED)
   c. 最大响应位置 → 重叠行数 → 去掉重叠垂直堆叠
   d. 置信度 < 0.55 时回退 ORB 特征点匹配
   e. 固定区感知：探测顶/底固定区域（表头/滚动条），排除干扰
7. 发射 captureFinished(mergedImage)
```

### 5.4 标注图元体系

```
BaseAnnotationItem : QGraphicsItem
  ├── RectangleItem    — 矩形
  ├── EllipseItem      — 椭圆
  ├── ArrowItem        — 箭头（直线 + 箭头头）
  ├── PenItem          — 自由画笔（点序列）
  ├── HighlighterItem  — 荧光笔
  └── TextItem         — 文字
```

- `AnnotationScene` 持有 `Tool` 枚举和 `UndoStack`
- `mousePressEvent` → 创建对应 `Item` → `mouseMoveEvent` 更新几何 → `mouseReleaseEvent` 提交 `AddItemCommand` 到 `UndoStack`
- 撤销/重做时**不删除指针**，仅 `addItem/removeItem`，保证图元可恢复

### 5.5 Undo/Redo 栈

- 自研 `ICommand` 抽象接口 + `UndoStack`（上限 100 步）
- 内部使用 `std::vector<std::unique_ptr<ICommand>>`，非 `QStack`（Qt6 要求元素可拷贝）
- `push()` 执行 `redo()` 并入撤销栈，清空重做栈
- `undo()` 弹出执行后压入重做栈，`redo()` 反之

---

## 6. 常见问题

### 6.1 clangd 报红「找不到 QtWidgets/QWidget」

1. 检查 `.clangd` 中的 Qt6 include 路径是否正确
2. 确认已执行 `cmake --preset user-debug` 生成了 `compile_commands.json`
3. VSCode 中 `Ctrl+Shift+P` → `clangd: Restart language server`

### 6.2 链接错误「无法找到 Qt6Core.lib」

`Qt6_DIR` 未指向带 `lib/cmake/Qt6/` 的目录。
正确示例：`F:/ForVS/Qt/6.11.1/msvc2022_64`（该目录下应有 `lib/cmake/Qt6/Qt6Config.cmake`）

### 6.3 CMake 报「Could NOT find OpenCV (missing: OpenCVConfig.cmake)」

`OpenCV_DIR` 未设置，或指向的目录没有 `OpenCVConfig.cmake`。
本项目 CMakeLists.txt 已自动处理 OpenCV 官方包的子目录细化问题，
用户只需将 `OpenCV_DIR` 指向 `<build>` 根目录即可。

### 6.4 CMake 报「Could NOT find OpenCV (missing: features2d)」

OpenCV 官方 Windows 预编译包采用 **opencv_world 单体库**模式，
本项目使用 `find_package(OpenCV REQUIRED)`（**不带 COMPONENTS**），
链接 `${OpenCV_LIBS}` 即可，world 库已包含全部模块。

### 6.5 CMake 报「cl is not a full path and was not found in the PATH」

未加载 MSVC 开发环境。VSCode 中选 `Visual Studio ... amd64` Kit；
命令行需在「VS x64 Native Tools Command Prompt」中执行，或先 `call vcvars64.bat`。

### 6.6 链接器被 w64devkit/mingw 劫持

PATH 中的 w64devkit 抢占了 `link.exe`。CMakePresets 已内置 `CMAKE_LINKER=link` / `CMAKE_AR=lib`，
确保 `cmake.useCMakePresets: "always"` 即可规避。

### 6.7 运行时崩溃「无法定位程序输入点 opencv_core500.dll」

CMakeLists.txt 已配置 `copy_directory` 自动拷贝 OpenCV DLL 到输出目录，
如未生效可手动将 `x64/vc16/bin` 加入 PATH，或使用 VSCode launch.json 中已配好的环境变量。

---

## 7. 打包与分发

### 7.1 前置：安装 NSIS

- 下载 https://nsis.sourceforge.io/Download **NSIS 3.x**（不支持 2.x，CPack 用 NSIS 3 特性），装到默认路径（`makensis` 自动入 PATH）
- 验证：`makensis -version` 输出版本号

### 7.2 打包命令

```bash
cmake --build --preset user-release
cd build/user-release
cpack
```

### 7.3 产物

- `build/dist/ScreenKiller-0.1.0-win64.exe` — NSIS 安装向导（开始菜单快捷方式 + 卸载入口）
- `build/dist/ScreenKiller-0.1.0-win64.zip` — Portable 免安装包（含 `portable.txt` 标记）

### 7.4 Portable 用法

- 解压 zip 到任意目录 → 同目录已有 `portable.txt` → 运行 `ScreenKiller.exe` → 配置落在 **exe 同目录**（删文件夹即净）
- 删除 `portable.txt` 则改回 `%APPDATA%` 配置路径

### 7.5 SmartScreen 警告

- 未数字签名，Windows SmartScreen 首次运行拦截 → 点「更多信息」→「仍要运行」

---

## 8. 后续路线图

- [ ] 工具属性面板（颜色 / 线宽 / 字体）接入 ToolBar
- [ ] 截屏成果预览工具（放大缩小和拖动）

- [ ] 国际化（i18n）支持
- [ ] 多显示器支持

---

## 9. 许可证

MIT License · Copyright (c) 2026 Sammer