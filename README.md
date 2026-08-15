# 屏幕杀手 (ScreenKiller)

> Windows 长截屏（滚动截屏）与图像标注工具 · Qt 6 + OpenCV 5 + Windows API

![status](https://img.shields.io/badge/status-WIP-orange)
![platform](https://img.shields.io/badge/platform-Windows11-blue)
![qt](https://img.shields.io/badge/Qt-6.11%2B-green)
![opencv](https://img.shields.io/badge/OpenCV-5.x-green)

> **💾 直接下载发布版**：可从发行页面下载 **v1.2.2** 安装包 (`ScreenKiller-1.2.2-win64.exe`) 一键安装，或下载绿色免安装版压缩包 (`ScreenKiller-1.2.2-win64-portable.zip`) 解压即用，无需配置编译环境。

---

## 1. 快速开始

### 1.1 依赖准备

| 依赖 | 版本 | 说明 |
|------|------|------|
| **Visual Studio 2022** | 17+ | 含「C++ 桌面开发」工作负载 |
| **Qt** | 6.11+ | 安装 `MSVC 2022 64-bit` 组件，附带 CMake + Ninja |
| **OpenCV** | 5.x | 官方预编译包（opencv_world 单体库）或自行编译 |
| **CMake** | 3.21+ | Qt 自带或单独安装 |

### 1.2 配置本机路径

**推荐：设置用户环境变量**

```powershell
[Environment]::SetEnvironmentVariable("Qt6_DIR",    "你Qt的根目录/6.11.1/msvc2022_64", "User")
[Environment]::SetEnvironmentVariable("OpenCV_DIR", "你OpenCV的根目录/opencv/build",   "User")
```

设置后重启终端。也可在 `CMakeUserPresets.json` 的 `cacheVariables` 中填写（模板见 `CMakeUserPresets.json.example`）。

### 1.3 构建

**命令行（需在 VS x64 Native Tools Command Prompt 中执行）：**

```bash
# Debug 构建
cmake --preset user-debug
cmake --build --preset user-debug
./build/user-debug/bin/ScreenKiller.exe

# Release 构建
cmake --preset user-release
cmake --build --preset user-release
```

**发布打包（指定版本号 → 配置 → 构建 → 打包，输出至 `build/dist/`）：**

```bash
# 1. 设置发布版本号（Windows cmd 语法；加引号可防止把行尾空格收进变量）
set "SCREENKILLER_VERSION=1.2.2"

# 2. 配置 → 构建 → 打包
cmake --preset user-release
cmake --build --preset user-release
cpack --preset package_release

# 3. 便携包（ZIP）追加 -portable 后缀
cmake -P cmake/rename_portable_zip.cmake
```

产物：`ScreenKiller-1.2.2-win64.exe`（NSIS 安装器）与 `ScreenKiller-1.2.2-win64-portable.zip`（免安装版），版本号三处同源（exe 属性 / 包文件名 / 注册表 DisplayVersion）。

> **注意**：PATH 混入 w64devkit/mingw 会劫持 `link.exe`，CMakePresets 已内置 `CMAKE_LINKER=link` / `CMAKE_AR=lib` 规避。

### 1.4 验证安装

启动后应看到主窗口显示截屏按钮 + 模式下拉 + 最小化/关闭按钮。按下 `Ctrl+Alt+A` 进入截屏模式，关闭窗口缩到托盘，托盘右键可退出。

---

## 2. 使用方式

### 2.1 截屏模式

| 模式 | 快捷键 | 操作 |
|------|--------|------|
| **画框截屏**（默认） | 截屏后拖拽框选 | 半透明遮罩 → 拖拽矩形选区 → 释放即截取 |
| **全屏截屏** | 一键截取 | 直接抓取主屏全部内容 |
| **窗口截屏** | 鼠标悬停选择 | 遮罩上移动鼠标 → 下方窗口自动高亮 → 点击截取 |
| **滚动截屏** | 框选后手动滚动 | 框选区域 → 提示浮窗 → 手动滚动目标窗口 → 按 Enter 或点「完成」→ 自动拼接长图 |

- **全局快捷键**：`Ctrl + Alt + A`
- **模式切换**：点击截屏按钮右侧模式选择按钮，或按 `Tab` 键循环

### 2.2 标注画布

截屏完成后自动进入标注视图：

| 操作 | 效果 |
|------|------|
| 左键 | 用当前工具在截屏上绘制 |
| 右键 | 复制当前已编辑的截屏到粘贴板 |
| 点击中键 | 恢复视图到100%缩放大小，并居中显示 |
| 滚动中键 | 缩放画布 |
| 拖动中键 | 平移视图，查看画布 |
| `Delete + Delete` | 清除所有编辑内容 |
| `Delete + Delete + Delete` | 复位页面到初始状态（标题与快捷键引导页） |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |


**标注工具**：水笔、荧光笔、马赛克、文字和几何图案（直线、箭头、方框和圆）。

### 2.3 系统托盘

- **关闭窗口** → 退出程序，结束进程
- **最小化窗口** → 缩到系统托盘，托盘右键显示主窗口
- **右键托盘图标** → 显示主窗口 / 截屏 / 退出

---

## 3. 项目简介

屏幕杀手 (ScreenKiller) 是一款 Windows 桌面端的「截屏 + 标注」工具，具备四种截屏模式与完整的图形标注画布，整体风格遵循 Windows 11 浅色设计语言。

| 能力 | 说明 |
|------|------|
| 画框截屏 | 拖拽框选任意区域，默认模式 |
| 全屏截屏 | 一键抓取主屏全部内容 |
| 窗口截屏 | 鼠标悬停高亮目标窗口，点击即抓 |
| 滚动截屏 | 手动滚动驱动抓帧 + OpenCV 模板匹配拼接，输出长图 |
| 标注画布 | 矩形 / 椭圆 / 直线 / 箭头 / 水笔 / 荧光笔 / 马赛克 / 文字 |
| 标注工具栏 | 悬浮手风琴工具栏，集中工具选择 + 颜色（标注/荧光笔双色板）/ 线宽 / 透明度 / 字体滑块，`QSettings` 持久化 |
| 撤销重做 | 自研轻量 Undo/Redo 栈，`Ctrl+Z` / `Ctrl+Y` |
| 全局快捷键 | `Ctrl+Alt+A` 唤起截屏 |
| 自动更新 | GitHub / Gitee 双源检测正式版更新，托盘气泡 + 对话框引导下载 |

**v1.2.2 主要更新（自 v1.2.1）：**
- 新增 **版本自动更新** 机制：启动后约 3 秒后台检测 GitHub Releases 正式版（24 小时冷却），托盘气泡 + 更新对话框（Markdown 变更日志），支持「前往下载 / 稍后 / 跳过此版本」
- 更新检查改为 **GitHub / Gitee 双源竞速**，国内网络下自动落到可达源
- **新手引导提示面板**显示方案大改版，交互与样式重构
- 新增 **三击 `Delete` 清空全部图像**，复位到初始状态
- 修复 **长截屏右侧黑边**：框选瞬间十字光标的暗色描边残影被截入首帧
- 修正 **首次打开的默认标注参数**（工具 / 颜色 / 线宽等初始值）
- 打包范式升级：exe 版本号模板化注入（`app.rc.in`），便携包自动追加 `-portable` 后缀
- 应用图标去除多余边缘，新增「设置」图标（含悬停态）

**v1.2.1 主要更新（自 v1.2.0）：**
- 修复画框截屏左 / 上边缘残留蓝色边框
- 修复窗口截屏预览左上角标签文字超出
- 保存对话框默认打开上一次保存的位置
- 统一菜单 / 按钮等操作名称

**v1.2.0 主要更新（自 v1.1.0）：**
- 新增 **马赛克** 标注工具，涂抹模糊遮盖
- 新增 **标注工具栏**（悬浮手风琴 UI）与 `ToolButton` 子控件，集中管理工具与参数
- 标注 / 荧光笔 **双色板** 拆分；颜色 / 线宽 / 透明度 / 字体滑块参数，`QSettings` 持久化
- 文字标注改为 **点击即输入**（虚线框编辑覆盖层）
- 图元属性边界 clamp 约束（`AnnotationConstants`）
- 新增多个涉及截屏绘制的快捷键
- 新手引导提示面板，可收缩

**v1.2.1 主要更新（自 v1.2.0，Bug 修复）：**
- 修复 **框选截图** 时选区左边与上边残留蓝色边框的问题
- 修复 **窗体截图** 窗体预览左上角标签文字超出边界的问题
- 修复 **保存** 时每次都会重新打开上一次保存位置的问题（现默认打开当前会话起始目录）

---

## 4. 工程结构

```
ScreenKiller/
├── CMakeLists.txt                 # 主构建脚本（419 行）
├── CMakePresets.json              # 内置预设（msvc-debug / msvc-release）
├── CMakeUserPresets.json          # 用户预设（gitignored）
├── .clangd / .editorconfig / .gitignore
├── LICENSE                        # MIT 许可证
├── THIRD_PARTY_LICENSES.txt       # 第三方依赖许可证
│
├── cmake/
│   ├── portable_marker.cmake      # CPack ZIP 便携包标记钩子
│   └── rename_portable_zip.cmake  # 便携包重命名（追加 -portable 后缀）
├── windows/
│   └── app.rc.in                  # Windows 资源脚本模板（exe 图标 + 版本信息，版本号由 CMake 代入）
│
├── resources/
│   ├── resources.qrc              # Qt 资源清单（1 QSS + 14 个图标）
│   ├── styles/windows11_light.qss # Windows 11 浅色 QSS
│   ├── icons/                     # 18 个图标文件（含安装器图标）
│   └── translations/              # 国际化（待填充）
│
├── src/                           # ≈ 66 个源文件
│   ├── main.cpp                   # 程序入口
│   │
│   ├── app/                       # 应用框架
│   │   ├── MainWindow.h/.cpp      # 主窗口（FramelessWindowHint 自绘）
│   │   └── GlobalHotkey.h/.cpp    # RegisterHotKey 封装
│   │
│   ├── capture/                   # 截屏引擎（6 组）
│   │   ├── CaptureEngine.h/.cpp   # 4 模式调度器
│   │   ├── RegionSelector.h/.cpp  # 画框选择器
│   │   ├── WindowSelector.h/.cpp  # 窗口高亮选择器
│   │   ├── ScrollCapture.h/.cpp   # 手动滚动截屏（滚轮监听 + 抓帧）
│   │   ├── ScrollOverlay.h/.cpp   # 滚动截屏提示浮窗
│   │   └── FrameQueue.h/.cpp      # 线程安全帧队列
│   │
│   ├── stitcher/                  # 图像拼接（2 组）
│   │   ├── ImageStitcher.h/.cpp   # OpenCV 模板匹配垂直拼接
│   │   └── StitchWorker.h/.cpp    # 拼接工作线程封装
│   │
│   ├── annotation/                # 标注画布（4 组 + 8 种图元）
│   │   ├── AnnotationView.h/.cpp  # QGraphicsView 子类
│   │   ├── AnnotationScene.h/.cpp # QGraphicsScene 子类，工具分发
│   │   ├── UndoStack.h/.cpp       # 自研撤销重做栈
│   │   ├── AnnotationConstants.h  # 标注属性边界常量（图元 clamp）
│   │   └── items/                 # 8 种标注图元
│   │       ├── BaseAnnotationItem.h/.cpp     # 抽象基类
│   │       ├── {Rectangle|Ellipse|Arrow|Pen| # 矩形/椭圆/箭头/水笔/
│   │       │    Highlighter|Mosaic|Text}Item # 荧光笔/马赛克/文字
│   │       │    .h/.cpp
│   │
│   ├── platform/                  # Windows API 封装（3 组）
│   │   ├── WinApi.h/.cpp          # 窗口/输入/屏幕工具函数集
│   │   ├── MouseWheelHook.h/.cpp  # WH_MOUSE_LL 全局滚轮钩子
│   │   └── KeyboardHook.h/.cpp    # WH_KEYBOARD_LL 全局键盘钩子
│   │
│   ├── ui/                        # UI 组件（4 组）
│   │   ├── ToolBar.h/.cpp         # 顶部工具栏（兼标题栏拖拽）
│   │   ├── AnnotationToolBar.h/.cpp  # 标注工具栏（悬浮手风琴）
│   │   ├── GuidePanel.h/.cpp      # 悬浮引导面板
│   │   └── Style.h/.cpp           # Windows 11 浅色 QSS 加载
│   │
│   ├── sub_widget/                # 子控件（1 组）
│   │   └── ToolButton.h/.cpp      # 手风琴工具按钮（tooltip / 启闭循环）
│   │
│   ├── utils/                     # 工具（3 组）
│   │   ├── ImageUtils.h/.cpp      # QImage ↔ cv::Mat 互转
│   │   ├── Logger.h/.cpp          # QLoggingCategory 分类日志宏
│   │   └── MessageBox.h/.cpp      # 中文按钮消息框
│   │
│   └── update/                    # 自动更新（4 组）
│       ├── UpdateChecker.h/.cpp   # GitHub Releases 检测器
│       ├── UpdateDialog.h/.cpp    # 更新提示对话框
│       ├── UpdateInfo.h           # 发布信息结构体
│       └── VersionUtils.h/.cpp    # 语义版本比较工具
│
├── build/                         # 构建输出（含 dist/ 打包产物）
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

### 5.2 滚动截屏流程

1. 用户框选目标区域 → 显示 `ScrollOverlay` 提示浮窗
2. 安装 `MouseWheelHook`（`WH_MOUSE_LL`）监听全局滚轮事件
3. 累积滚动像素达到阈值（默认 10px）→ 抓取一帧 → `FrameQueue.enqueue`
4. 帧处理工作线程从 `FrameQueue.dequeue` 处理帧（重复检测 + 灰度转换）
5. 用户按 Enter 或点击「完成」→ 进入拼接阶段
6. `StitchWorker`（`QtConcurrent` 异步线程）调用 `ImageStitcher.stitchVertical()`：
   - 取上一帧底部 64px strip 作为模板
   - `cv::matchTemplate(TM_CCOEFF_NORMED)` 计算重叠区域
   - 去掉重叠部分垂直堆叠
   - 置信度 < 0.55 时回退 ORB 特征点匹配
   - 固定区感知：探测顶/底固定区域（表头/滚动条），排除干扰
7. 发射 `captureFinished(mergedImage)`

### 5.3 标注图元体系

```
BaseAnnotationItem : QGraphicsItem   ← 统一管理 QPen / QBrush
  ├── RectangleItem    — 矩形
  ├── EllipseItem      — 椭圆
  ├── ArrowItem        — 箭头（直线 + 箭头头）
  ├── PenItem          — 水笔（点序列）
  │   └── HighlighterItem — 荧光笔（继承 PenItem 复用路径）
  ├── MosaicItem       — 马赛克（涂抹模糊遮盖）
  └── TextItem         — 文字
```

`AnnotationScene` 持有 `Tool` 枚举和 `UndoStack`。鼠标事件创建对应 Item 后提交 `AddItemCommand` 到撤销栈。

### 5.4 Undo/Redo 栈

- 自研 `ICommand` 接口 + `UndoStack`（上限 100 步），`std::vector<std::unique_ptr<ICommand>>`
- 撤销/重做时**不删除图元指针**，仅 `addItem/removeItem`，保证可恢复

### 5.5 打包分发

- Release 构建后通过 `cpack --preset package_release` 打包，**跨平台**按平台生成对应产物（`CMakeLists.txt` 按 `WIN32 / APPLE / UNIX` 自动选择生成器）：
  - **Windows**：NSIS 安装器（`.exe`）+ ZIP 便携包（`.zip`）
  - **macOS**：DragNDrop（`.dmg`）+ ZIP 便携包
  - **Linux**：DEB + RPM 安装包 + ZIP 便携包
- ZIP 便携包内含 `portable.txt` 标记文件，配置写入同级目录而非注册表
- 便携包（ZIP）在打包后自动重命名为 `ScreenKiller-<版本>-win64-portable.zip`（安装器为 `ScreenKiller-<版本>-win64.exe`），文件名即可区分安装版与免安装版
- 打包时通过环境变量 `SCREENKILLER_VERSION` 在配置期传入发布版本号，exe 属性版本、安装包/便携包文件名、注册表 `DisplayVersion` 三者同源一致；日常编译版本默认 Debug=0.0.0 / Release=0.0.1（由 `CMakeLists.txt` 控制）
- `windows/app.rc.in` 由 CMake 配置期代入版本号生成 `app.rc`，嵌入 exe 图标（`.ico`）和版本信息（`VS_VERSIONINFO`）；NSIS 安装向导标题不再附带版本号
- `windeployqt` + `copy_directory` 自动部署 Qt/OpenCV DLL 到输出目录

### 5.6 版本号控制规则

- **版本号唯一来源**：`CMakeLists.txt` 顶部的 `project(VERSION ...)`。所有渠道的版本号最终都汇入这里，再派生到 exe 属性版本、安装包/便携包文件名、注册表 `DisplayVersion`。
- **日常编译默认值**（IDE / 命令行直接配置构建时生效）：
  - `Debug` → `0.0.0`（开发构建）
  - `Release` → `0.0.1`（预发布）
  - 默认值写在 `CMakeLists.txt` 版本策略的 `if/elseif/else` 分支中，需要调整时只改这一处。
- **打包发布**：配置前设置环境变量 `SCREENKILLER_VERSION`（如 `1.2.2`），在**配置期**读入 `project(VERSION)`；配置输出会打印 `ScreenKiller 1.2.2 配置中...` 供确认。
- **同源一致**（一次输入，处处生效）：
  - exe 属性版本（`app.rc.in` → `VS_VERSIONINFO`，含安装包与便携包内的 exe）
  - 安装包/便携包文件名（`ScreenKiller-1.2.2-win64.exe` / `ScreenKiller-1.2.2-win64-portable.zip`）
  - 安装后注册表 `DisplayVersion`
- **版本在 configure 时固化**：改版本号后必须重新配置（重新执行 `cmake --preset ...`）才会重编译生效；只执行构建不会换版本号。
- **发布流程**：设置 `SCREENKILLER_VERSION` → `cmake --preset user-release` → `cmake --build --preset user-release` → `cpack --preset package_release` → `cmake -P cmake/rename_portable_zip.cmake`（便携包自动追加 `-portable` 后缀）。
- **运行期同源**：程序运行期版本号由 CMake 配置期经 `configure_file` 生成 `generated/update/AppInfo.h` 的 `SK_APP_VERSION` 宏注入，`main.cpp` 用它设置 `QApplication::applicationVersion()`，供自动更新模块比对——与 exe 属性 / 包文件名 / 注册表 DisplayVersion 完全同源。

### 5.7 自动更新机制

程序内置「检测更新」能力，更新源为 GitHub 仓库 `Sammer720/ScreenKiller` 的 Releases 发布页，全部基于 Qt 原生模块（`QNetworkAccessManager` + `QJsonDocument` + `QVersionNumber` + `QDesktopServices` + 托盘气泡 + 自定义对话框），不引入任何第三方依赖。

- **检测时机**：启动后约 3 秒在后台自动检测一次（24 小时冷却）；托盘右键菜单「检查更新」可随时手动触发。
- **双源竞速**：同时请求 GitHub 与 Gitee 的 `releases/latest`（`api.github.com` / `gitee.com/api/v5`），**哪个源先成功回包就用哪个源**的版本号与下载地址；单源失败不判失败，等另一源；双源均失败才提示。国内网络下 GitHub 不可达时自动落到 Gitee，下载地址指向可访问的源。
- **检测目标**：只认正式版（跳过 draft / prerelease），比对 `tag_name` 的 `X.Y.Z` 段。
- **版本比对**：`QVersionNumber` 逐段数值比较（正确处理 1.2.9 < 1.2.10），当前版本号来自运行期同源的 `SK_APP_VERSION`。
- **按发行版匹配资产**：安装版跳转 `ScreenKiller-<版本>-win64.exe` 安装器；便携版（exe 旁存在 `portable.txt`）跳转 `ScreenKiller-<版本>-win64-portable.zip` 压缩包；找不到对应资产则回退到发布页。
- **提示与交互**：有新版 → 托盘气泡「发现新版本」，点击弹出更新对话框（Markdown 渲染变更日志），提供「前往下载 / 稍后 / 跳过此版本」三个动作；「前往下载」仅打开浏览器到对应资产地址，**不自动下载安装**。
- **便携版更新**：完全由用户手动完成（自行下载 ZIP 解压覆盖，保留 `portable.txt` 与 INI 配置）。
- **容错**：离线 / API 限流（403/429）/ 无 Release 等异常不崩溃、不刷屏，自动检查仅记日志，手动检查给一次性提示。
- **持久化**：跳过版本（`update/ignoredVersion`）与最近自动检查时间（`update/lastAutoCheck`）写入 INI。

---

## 6. 后续路线图

- [x] 工具属性面板（颜色 / 线宽 / 字体）接入标注工具栏
- [ ] ORB 特征点匹配作为模板匹配的回退方案（接口已预留）
- [ ] 国际化（i18n）支持（translations/ 目录已就绪）
- [ ] 多平台兼容 （macOS / Linux / Windows）

---

## 7. 许可证

MIT License · Copyright (c) 2026 Sammer