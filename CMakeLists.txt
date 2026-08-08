# =============================================================================
# ScreenKiller - Windows 长截图与图像标注工具
# 基于 Qt 6.11+ / OpenCV 5.x / Windows API
# =============================================================================
cmake_minimum_required(VERSION 3.21)

# ---------- 项目信息 ----------
project(ScreenKiller
    VERSION 1.1.0
    DESCRIPTION "Windows 长截图（滚动截屏）与图像标注工具"
    LANGUAGES CXX
)

# ---------- 全局选项 ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # 严格使用标准 C++17，不依赖编译器扩展
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 输出目录统一化，便于运行期定位 DLL
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 默认 Release（速度优先）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 平台校验：本工具仅支持 Windows
if(NOT WIN32)
    message(FATAL_ERROR "ScreenKiller 仅支持 Windows 平台。")
endif()

# ---------- 路径提示变量（可在命令行 / CMakePresets / CMakeCache 中覆盖） ----------
# Qt6 安装路径，例如：D:/Qt/6.11.0/msvc2022_64
# 优先级：CMAKE_PREFIX_PATH > Qt6_DIR > 环境变量 Qt6_DIR
if(NOT DEFINED Qt6_DIR AND DEFINED ENV{Qt6_DIR})
    set(Qt6_DIR $ENV{Qt6_DIR})
endif()

# OpenCV 安装路径，例如：D:/opencv/build 或 D:/opencv/install
if(NOT DEFINED OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR $ENV{OpenCV_DIR})
endif()

# 让 find_package 能够找到自定义路径下的 Config 文件
if(DEFINED Qt6_DIR)
    list(APPEND CMAKE_PREFIX_PATH ${Qt6_DIR})
endif()

message(STATUS "===========================================================")
message(STATUS "ScreenKiller ${PROJECT_VERSION} 配置中...")
message(STATUS "CMAKE_BUILD_TYPE       = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_COMPILER     = ${CMAKE_CXX_COMPILER}")
message(STATUS "Qt6_DIR                = ${Qt6_DIR}")
message(STATUS "OpenCV_DIR             = ${OpenCV_DIR}")
message(STATUS "CMAKE_PREFIX_PATH      = ${CMAKE_PREFIX_PATH}")
message(STATUS "===========================================================")

# =============================================================================
# 寻找 Qt6
# =============================================================================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# 编译时需要的 Qt 定义
add_compile_definitions(
    QT_DEPRECATED_WARNINGS
    QT_NO_KEYWORDS          # 不让 signals/slots 等关键字污染第三方头文件
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX                # 避免 windows.h 把 min/max 定义成宏
)

# =============================================================================
# 寻找 OpenCV 5.x
# =============================================================================
# Windows 上的 OpenCV 官方预编译包有两个 OpenCVConfig.cmake：
#   1. <build>/OpenCVConfig.cmake               —— 分发器，需要 OpenCV_ARCH + OpenCV_RUNTIME
#   2. <build>/x64/vc<NN>/lib/OpenCVConfig.cmake —— 完整版，直接可用（推荐）
#
# 优先级（从高到低）：
#   a) 用户显式指定的 OpenCV_DIR（应直接指向含 OpenCVConfig.cmake 的目录）
#   b) <OpenCV_ROOT>/x64/vc17/lib  （VS2022 工具集）
#   c) <OpenCV_ROOT>/x64/vc16/lib  （VS2019 工具集）
#   d) <OpenCV_ROOT>/x64/vc15/lib  （VS2017 工具集）
#   e) <OpenCV_ROOT>                （回退到根分发器，设置 ARCH/RUNTIME）
#
# 用户只需设置 OpenCV_DIR 指向 <build> 根目录即可，下面的逻辑会自动细化。
if(DEFINED OpenCV_DIR AND EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
    # 如果用户指向根目录，尝试细化到 vc 子目录
    if(NOT EXISTS "${OpenCV_DIR}/OpenCVModules.cmake")
        # 这是根分发器，需要找子目录
        set(_OPENCV_ROOT "${OpenCV_DIR}")
        set(_OPENCV_SUBDIR_CANDIDATES
            "${_OPENCV_ROOT}/x64/vc17/lib"
            "${_OPENCV_ROOT}/x64/vc16/lib"
            "${_OPENCV_ROOT}/x64/vc15/lib"
            "${_OPENCV_ROOT}/x64/mingw/lib"
        )
        set(_OPENCV_REFINED FALSE)
        foreach(_sub ${_OPENCV_SUBDIR_CANDIDATES})
            if(EXISTS "${_sub}/OpenCVConfig.cmake")
                set(OpenCV_DIR "${_sub}" CACHE PATH "OpenCV config dir" FORCE)
                message(STATUS "OpenCV_DIR 细化为子目录: ${_sub}")
                set(_OPENCV_REFINED TRUE)
                break()
            endif()
        endforeach()
        if(NOT _OPENCV_REFINED)
            # 回退：使用根分发器，强制设置 ARCH + RUNTIME
            set(OpenCV_ARCH    "x64"    CACHE STRING "OpenCV arch"     FORCE)
            set(OpenCV_RUNTIME "vc16"   CACHE STRING "OpenCV runtime"  FORCE)
            message(STATUS "使用 OpenCV 根分发器，强制 ARCH=${OpenCV_ARCH} RUNTIME=${OpenCV_RUNTIME}")
        endif()
    endif()
endif()

# 注意：不使用 COMPONENTS 参数。
# 原因：OpenCV 官方 Windows 预编译包采用 opencv_world 单体库模式，
# 只导出 opencv_world 这一个 imported target，没有独立的 opencv_features2d 等。
# find_package(OpenCV REQUIRED) 会正确设置 OpenCV_LIBS=opencv_world，
# 该库已包含 core/imgproc/imgcodecs/features2d 等全部模块。
find_package(OpenCV REQUIRED)

# 校验关键模块的可用性（world 模式下这些宏都应为 ON）
message(STATUS "OpenCV version        : ${OpenCV_VERSION}")
message(STATUS "OpenCV libs           : ${OpenCV_LIBS}")
message(STATUS "OpenCV include dirs   : ${OpenCV_INCLUDE_DIRS}")

message(STATUS "Qt6 version     : ${Qt6Core_VERSION}")
message(STATUS "OpenCV version  : ${OpenCV_VERSION}")

# =============================================================================
# 源文件清单
# =============================================================================
set(APP_SOURCES
    src/main.cpp

    # 应用主框架
    src/app/MainWindow.cpp
    src/app/GlobalHotkey.cpp

    # 截屏引擎
    src/capture/CaptureEngine.cpp
    src/capture/RegionSelector.cpp
    src/capture/WindowSelector.cpp
    src/capture/ScrollCapture.cpp
    src/capture/ScrollOverlay.cpp
    src/capture/FrameQueue.cpp

    # 图像拼接
    src/stitcher/ImageStitcher.cpp
    src/stitcher/StitchWorker.cpp

    # 标注画布
    src/annotation/AnnotationView.cpp
    src/annotation/AnnotationScene.cpp
    src/annotation/UndoStack.cpp
    src/annotation/items/BaseAnnotationItem.cpp
    src/annotation/items/RectangleItem.cpp
    src/annotation/items/EllipseItem.cpp
    src/annotation/items/ArrowItem.cpp
    src/annotation/items/PenItem.cpp
    src/annotation/items/HighlighterItem.cpp
    src/annotation/items/TextItem.cpp
    src/annotation/items/MosaicItem.cpp

    # 平台层
    src/platform/MouseWheelHook.cpp
    src/platform/KeyboardHook.cpp
    src/platform/WinApi.cpp

    # UI 组件
    src/ui/GuidePanel.cpp
    src/ui/ToolBar.cpp
    src/ui/AnnotationToolBar.cpp
    src/ui/Style.cpp

    # 子控件
    src/sub_widget/ToolButton.cpp

    # 工具
    src/utils/ImageUtils.cpp
    src/utils/Logger.cpp
)

set(APP_HEADERS
    src/app/MainWindow.h
    src/app/GlobalHotkey.h

    src/capture/CaptureEngine.h
    src/capture/RegionSelector.h
    src/capture/WindowSelector.h
    src/capture/ScrollCapture.h
    src/capture/ScrollOverlay.h
    src/capture/FrameQueue.h

    src/stitcher/ImageStitcher.h
    src/stitcher/StitchWorker.h

    src/annotation/AnnotationView.h
    src/annotation/AnnotationScene.h
    src/annotation/UndoStack.h
    src/annotation/items/BaseAnnotationItem.h
    src/annotation/items/RectangleItem.h
    src/annotation/items/EllipseItem.h
    src/annotation/items/ArrowItem.h
    src/annotation/items/PenItem.h
    src/annotation/items/HighlighterItem.h
    src/annotation/items/TextItem.h
    src/annotation/items/MosaicItem.h
    src/annotation/AnnotationConstants.h

    src/platform/MouseWheelHook.h
    src/platform/KeyboardHook.h
    src/platform/WinApi.h

    src/ui/GuidePanel.h
    src/ui/ToolBar.h
    src/ui/AnnotationToolBar.h
    src/ui/Style.h

    # 子控件
    src/sub_widget/ToolButton.h

    src/utils/ImageUtils.h
    src/utils/Logger.h
)

set(APP_RESOURCES
    resources/resources.qrc
)

# Windows 资源文件（exe 文件图标 + 版本信息）
if(WIN32)
    set(APP_RESOURCES_WIN
        windows/app.rc
    )
endif()

# =============================================================================
# 目标：可执行文件
# 使用 qt_add_executable 而非 add_executable，自动处理：
#   - WIN32 子系统入口（无控制台黑框）
#   - Qt6::EntryPoint 链接
#   - Android 等平台的特殊入口
# =============================================================================
qt_add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${APP_HEADERS}
    ${APP_RESOURCES}
    ${APP_RESOURCES_WIN}
)

# 强制指定 WIN32 GUI 子系统（确保不弹控制台黑框）
# WIN32_EXECUTABLE TRUE 会让链接器使用 /SUBSYSTEM:WINDOWS 入口
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capture
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stitcher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/annotation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/annotation/items
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent

    ${OpenCV_LIBS}

    # Windows 系统库
    user32      # RegisterHotKey, SendInput, GetForegroundWindow
    gdi32       # 屏幕截图辅助
    dwmapi      # DwmGetWindowAttribute（获取真实窗口矩形）
    shell32     # Shell_NotifyIcon（托盘）
    comctl32    # 通用控件
    advapi32    # RegisterHotKey 依赖
)

# =============================================================================
# MSVC 编译选项
# =============================================================================
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4             # 高警告级别
        /permissive-    # 严格标准
        /Zc:__cplusplus # 正确报告 __cplusplus 宏
        /utf-8          # UTF-8 源码与执行字符集
        /MP             # 多核编译
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/Od /Zi /MDd>
    )
endif()

# =============================================================================
# 拷贝 Qt 与 OpenCV 的 DLL 到输出目录（运行期需要）
# 仅在 Windows 上执行
# =============================================================================
# 利用 windeployqt 自动部署 Qt 依赖
if(WIN32)
    find_program(QT_DEPLOY_TOOL windeployqt HINTS "${Qt6_DIR}/bin")
    if(QT_DEPLOY_TOOL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${QT_DEPLOY_TOOL} --no-translations --no-system-d3d-compiler
                    --no-opengl-sw --release
                    "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "运行 windeployqt 部署 Qt 运行时依赖..."
        )
    else()
        message(WARNING "未找到 windeployqt，请手动将 Qt DLL 加入 PATH 或拷贝到 bin 目录。")
    endif()

    # 拷贝 OpenCV DLL（兼容多种安装布局）
    # 注意：find_package 后 OpenCV_DIR 可能已被细化为 vc 子目录的 lib，
    #       因此从 OpenCV_DIR 反推 bin 目录：lib -> .. -> bin，或 lib -> ../../bin
    # 候选清单覆盖了根目录布局与 vc 子目录布局
    set(_OPENCV_BIN_CANDIDATES
        "${OpenCV_DIR}/bin"
        "${OpenCV_DIR}/../bin"
        "${OpenCV_DIR}/../../bin"
        "${OpenCV_DIR}/x64/vc16/bin"
        "${OpenCV_DIR}/x64/vc17/bin"
        "${OpenCV_DIR}/x64/vc15/bin"
        "${OpenCV_DIR}/x64/mingw/bin"
    )
    # 若 OpenCV_DIR 已是 lib 子目录，再加上同级 x64/<runtime>/bin
    if(OpenCV_DIR MATCHES "x64/vc[0-9]+/lib")
        string(REGEX REPLACE "x64/vc([0-9]+)/lib" "x64/vc\\1/bin" _OCV_SIBLING_BIN "${OpenCV_DIR}")
        list(APPEND _OPENCV_BIN_CANDIDATES "${_OCV_SIBLING_BIN}")
    endif()
    set(_OPENCV_BIN_FOUND FALSE)
    foreach(_candidate ${_OPENCV_BIN_CANDIDATES})
        if(EXISTS "${_candidate}" AND NOT _OPENCV_BIN_FOUND)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${_candidate}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "拷贝 OpenCV 运行时 DLL: ${_candidate}"
            )
            set(_OPENCV_BIN_FOUND TRUE)
            message(STATUS "OpenCV bin 目录: ${_candidate}")
        endif()
    endforeach()
    if(NOT _OPENCV_BIN_FOUND)
        message(WARNING "未找到 OpenCV bin 目录，请手动将 OpenCV DLL 加入 PATH。")
    endif()
endif()

# =============================================================================
# 安装规则（供 CPack 打包；复用 windeployqt + copy_directory 已铺好的 bin 目录）
# =============================================================================
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ DESTINATION .
        FILES_MATCHING
        PATTERN "*.dll"
        PATTERN "*.exe"
        PATTERN "*.qm"
        PATTERN "*.pdb"   EXCLUDE
        PATTERN "*.ilk"   EXCLUDE
        PATTERN "*.obj"   EXCLUDE
        PATTERN "*.lib"   EXCLUDE
        PATTERN "*.exp"   EXCLUDE)

# 许可证文件入包（CPACK_RESOURCE_FILE_LICENSE 只供 NSIS license page 显示，不自动入安装目录/ZIP——CMake 官方明确）
# 注意：此 install(FILES) 需 Todo 2 已创建 LICENSE/THIRD_PARTY_LICENSES.txt，否则 cmake --install 报错（Wave 1 已调序 Todo 2 先）
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
             "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.txt"
         DESTINATION .)

# =============================================================================
# CPack 打包配置（NSIS 安装器 + ZIP portable 双产物）
# =============================================================================
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_SYSTEM_NAME "win64")
set(CPACK_PACKAGE_NAME "ScreenKiller")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Sammer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Windows 长截图与图像标注工具")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ScreenKiller")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/build/dist")
set(CPACK_INSTALL_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/portable_marker.cmake")

# 图标（程序 exe 图标通过 .rc 嵌入 app.ico，此处设置安装器/卸载器 exe 图标 + 向导品牌图）
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icons/app_installer.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icons/app_installer.ico")
# CPACK_PACKAGE_ICON 需要 BMP 在构建目录可访问（NSIS File 命令使用原生路径格式）
configure_file(
    "${CMAKE_SOURCE_DIR}/resources/icons/app_installer.bmp"
    "${CMAKE_BINARY_DIR}/app_installer.bmp"
    COPYONLY
)
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/app_installer.bmp" _nsis_bmp_path)
set(CPACK_PACKAGE_ICON "${_nsis_bmp_path}")

# NSIS 特定
set(CPACK_NSIS_DISPLAY_NAME "ScreenKiller")
set(CPACK_NSIS_PACKAGE_NAME "ScreenKiller ${PROJECT_VERSION}")
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
set(CPACK_NSIS_CREATE_ICONS_EXTRA
     "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\ScreenKiller.lnk' '$INSTDIR\\\\ScreenKiller.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
     "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\ScreenKiller.lnk'")
include(CPack)

# =============================================================================
# 配置完成提示
# =============================================================================
message(STATUS "-----------------------------------------------------------")
message(STATUS "ScreenKiller 配置完成。")
message(STATUS "可执行文件路径: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.exe")
message(STATUS "-----------------------------------------------------------")
