/**
 * \file MainWindow.h
 * \brief 主窗口
 *
 * 初始状态：
 *   - 顶部一个工具条（仅含：截屏按钮 / 最小化 / 关闭）
 *   - 主体区域为隐藏的标注视口（AnnotationView），首次截屏后展开
 *   - 监听全局快捷键 Ctrl+Alt+A
 *
 * 截屏模式：
 *   - 画框截屏（默认）
 *   - 全屏截屏
 *   - 窗口截屏
 *   - 滚动截屏
 *
 * 窗口行为约定：
 *   - 最小化按钮 = 隐藏主窗口，缩到系统托盘（任务栏保持清爽）
 *   - 关闭按钮 = 发射 requestQuit 信号，由 main.cpp 连接 QApplication::quit
 *   - 不使用系统标题栏（FramelessWindowHint），由 ToolBar 承担拖拽
 *
 * 内存管理：
 *   - 遵循 Qt 对象树机制，所有成员对象均以 this 为 parent
 *   - 不重写析构函数，依赖 QObject 自动释放 children
 *   - GlobalHotkey 析构会自动调用 UnregisterHotKey
 */
#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "capture/CaptureEngine.h"
#include "update/UpdateInfo.h"

class QStackedWidget;
class QLabel;
class QResizeEvent;
class AnnotationView;
class GlobalHotkey;

namespace SK {

class ToolBar;
class AnnotationScene;
class GuidePanel;
class AnnotationToolBar;

namespace update {
class UpdateChecker;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口；通常为 nullptr
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /// @brief 截屏模式枚举
    enum class CaptureMode
    {
        Region,      ///< 画框截屏（默认）
        FullScreen,  ///< 全屏截屏
        Window,      ///< 窗口截屏
        Scrolling    ///< 滚动截屏
    };
    Q_ENUM(CaptureMode)

protected:
    /// @brief 拦截 Windows 原生消息，用于捕获 WM_HOTKEY 和 WM_NCHITTEST
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    /// @brief 窗口关闭时注销全局快捷键，并调用父类 closeEvent
    void closeEvent(QCloseEvent* event) override;
    /// @brief 窗口尺寸变化时联动标注工具栏位置（右侧贴边悬浮）
    void resizeEvent(QResizeEvent* event) override;

Q_SIGNALS:
    /// @brief 请求退出应用（由 main.cpp 连接到 QApplication::quit）
    void requestQuit();

private Q_SLOTS:
    /// @brief 截屏按钮点击
    void onCaptureButtonClicked();
    /// @brief 截屏模式切换
    void onCaptureModeChanged(int mode);
    /// @brief 截屏完成，将图片加载到标注场景
    void onCaptureFinished(const QImage& image);
    /// @brief 用户取消截屏
    void onCaptureCancelled();
    /// @brief 托盘图标被点击/双击
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    /// @brief 关闭按钮触发：发射 requestQuit 信号
    void onQuitRequested();
    /// @brief 保存截屏到文件
    void onSaveRequested();
    /// @brief 标注视图右键复制图片完成，触发托盘通知
    void onViewImageCopied();
    /// @brief Delete 三连击：复位页面到初始状态（清空场景并切回占位页）
    void onResetToInitialRequested();
    /// @brief 托盘菜单「检查更新」手动触发
    void onManualUpdateCheck();
    /// @brief 检测到可用更新
    void onUpdateAvailable(const update::ReleaseInfo& releaseInfo);
    /// @brief 已是最新版本
    void onUpdateUpToDate();
    /// @brief 检查失败
    void onUpdateCheckFailed(const QString& reason);

private:
    /// @brief 构造 UI 控件与布局
    void setupUi();
    /// @brief 构造工具栏并连接信号
    void setupToolBar();
    /// @brief 构造系统托盘图标及右键菜单
    void setupTrayIcon();
    /// @brief 注册全局快捷键 Ctrl+Alt+A
    void registerHotkeys();
    /// @brief 注销所有全局快捷键
    void unregisterHotkeys();
    /// @brief 启动截屏流程（隐藏主窗口、延迟触发 CaptureEngine）
    void startCapture(CaptureMode mode);
#ifdef Q_OS_WIN
    /// @brief 处理 WM_NCHITTEST：无边框窗口边缘缩放命中测试
    /// @param msg     Windows 消息指针
    /// @param result  [out] 命中码（HTLEFT 等）
    /// @return true 表示已处理；false 表示交给 Qt 默认处理
    bool handleNcHitTest(MSG* msg, qintptr* result);
#endif
    /// @brief 根据扩展名选择图像保存格式
    /// @param suffix 文件后缀（小写）
    /// @return Qt 支持的格式字符串，如 "PNG" / "JPG" / "BMP"
    QByteArray chooseImageFormat(const QString& suffix) const;

    /// @brief 显示「图片已复制到剪贴板」托盘气泡通知（截屏完成与标注页右键复制共用）
    /// @param title 通知标题
    void showImageCopiedNotice(const QString& title);
    /// @brief 计算标注工具栏右侧贴边悬浮的几何位置并应用
    ///
    /// 工具栏是中央页栈的直接子控件，叠加显示在标注页之上；
    /// 窗口尺寸变化时由 resizeEvent 统一调用，保证位置跟随窗口缩放。
    void updateAnnotationToolBarGeometry();
    /// @brief 启动自动更新检查（带冷却间隔判断）
    void startAutoUpdateCheck();
    /// @brief 发起一次更新检查
    /// @param isManual 是否为用户手动触发（true 手动 / false 自动）
    void startUpdateCheck(bool isManual);
    /// @brief 模态展示更新对话框，并按用户选择执行对应动作
    /// @param releaseInfo 待展示的发布信息
    void showUpdateDialog(const update::ReleaseInfo& releaseInfo);
    /// @brief 打开与发行版匹配的下载地址（仅打开浏览器，不执行下载安装）
    /// @param releaseInfo 发布信息
    void openDownloadUrl(const update::ReleaseInfo& releaseInfo);
    /// @brief 记录用户选择跳过的版本号
    /// @param versionString 被跳过的版本号
    void rememberIgnoredVersion(const QString& versionString);

    ToolBar*            m_toolBar        = nullptr;  ///< 顶部工具栏
    QStackedWidget*     m_centralStack   = nullptr;  ///< 中心页栈（占位/标注视口切换）
    QLabel*             m_placeholder    = nullptr;  ///< 初始占位提示标签
    AnnotationView*     m_view           = nullptr;  ///< 标注视图
    AnnotationScene*    m_scene          = nullptr;  ///< 标注场景
    GlobalHotkey*       m_hotkey         = nullptr;  ///< 全局快捷键管理
    QSystemTrayIcon*    m_tray           = nullptr;  ///< 系统托盘图标
    CaptureMode         m_mode           = CaptureMode::Region;  ///< 当前截屏模式
    CaptureEngine*      m_capture        = nullptr;  ///< 截屏引擎
    GuidePanel*         m_guidePanel     = nullptr;  ///< 标注页悬浮引导面板
    AnnotationToolBar*  m_annToolBar     = nullptr;  ///< 标注工具栏（标注页右侧悬浮）
    update::UpdateChecker* m_updateChecker   = nullptr;  ///< 更新检测器
    update::ReleaseInfo    m_latestRelease;              ///< 最近检测到的新版信息（气泡点击回调用）
    bool                   m_isManualCheck     = false;  ///< 当前检查是否为手动触发
    bool                   m_updateDialogShown = false;  ///< 更新对话框是否正在展示（防重入）

    static constexpr int kHotKeyId = 0x0001;   ///< Ctrl+Alt+A 的快捷键 ID
};

} // namespace SK
