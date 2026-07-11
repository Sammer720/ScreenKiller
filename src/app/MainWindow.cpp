/**
 * \file MainWindow.cpp
 * \brief MainWindow 实现
 *
 * 集成 ToolBar / AnnotationScene / CaptureEngine / GlobalHotkey / 系统托盘，
 * 协调各模块完成截屏→标注→保存流程。
 *
 * 退出流程：关闭按钮 → onQuitRequested → Q_EMIT requestQuit
 *           → main.cpp 中连接到 QApplication::quit → Qt 对象树自动析构
 */
#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMenu>
#include <QShortcut>
#include <QStackedWidget>
#include <QLabel>
#include <QBoxLayout>
#include <QSystemTrayIcon>
#include <QClipboard>
#include <QStyle>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QTimer>
#include <QMetaObject>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QSettings>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <windowsx.h>
#endif

#include "annotation/AnnotationView.h"
#include "annotation/AnnotationScene.h"
#include "GlobalHotkey.h"
#include "ui/ToolBar.h"
#include "utils/Logger.h"
#include "utils/MessageBox.h"

namespace SK {

namespace {
/// \brief 主窗口最小宽度
constexpr int G_WINDOW_MIN_WIDTH   = 380;
/// \brief 主窗口最小高度
constexpr int G_WINDOW_MIN_HEIGHT  = 60;
/// \brief 主窗口默认宽度
constexpr int G_WINDOW_DEFAULT_WIDTH  = 620;
/// \brief 主窗口默认高度
constexpr int G_WINDOW_DEFAULT_HEIGHT = 600;
/// \brief 截屏启动延迟（毫秒，等主窗口隐藏动画完成）
constexpr int G_CAPTURE_DELAY_MS   = 120;
/// \brief 系统托盘气泡提示显示时长（毫秒）
constexpr int G_TRAY_NOTICE_DURATION_MS = 3000;
/// \brief 默认截图保存子目录名
const QString G_SCREENSHOT_SUBDIR = QStringLiteral("Screenshots");
/// \brief 截图文件名前缀
const QString G_SCREENSHOT_PREFIX  = QStringLiteral("screenshot_");
/// \brief 默认截图时间戳格式
const QString G_TIMESTAMP_FORMAT   = QStringLiteral("yyyyMMdd_HHmmss");
/// \brief 中央栈页面索引：占位页
constexpr int G_PAGE_PLACEHOLDER   = 0;
/// \brief 中央栈页面索引：标注视口
constexpr int G_PAGE_ANNOTATION    = 1;
/// \brief 无边框窗口边缘可拖拽缩放区域的宽度（像素）
///
/// 鼠标落在此宽度的边缘带时，WM_NCHITTEST 返回对应方向的命中码
/// （HTLEFT/HTRIGHT/HTTOP/HTBOTTOM 等），由 Windows 自动处理缩放
/// 与 Aero Snap（拖到顶部最大化、拖到左右边缘半屏）。
constexpr int G_RESIZE_BORDER_WIDTH = 6;
/// \brief 窗口几何状态在配置文件中的键名
const QString G_CONFIG_KEY_GEOMETRY = QStringLiteral("mainWindow/geometry");
/// \brief 截屏模式在配置文件中的键名
const QString G_CONFIG_KEY_CAPTURE_MODE = QStringLiteral("mainWindow/captureMode");
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // 取消系统标题栏：使用 FramelessWindowHint
    // 保留 Qt::Window 使窗口仍出现在任务栏
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    setWindowTitle(tr("ScreenKiller"));
    setWindowIcon(QIcon(":/icons/app_alph.png"));
    setMinimumSize(G_WINDOW_MIN_WIDTH, G_WINDOW_MIN_HEIGHT);

    // 窗口几何状态恢复：能从配置读到历史就用历史，读不到就用默认大小
    // 默认构造 QSettings()：自动使用 main.cpp 中设置的全局 org/app name 和 defaultFormat
    QSettings settings;
    QByteArray savedGeometry = settings.value(G_CONFIG_KEY_GEOMETRY).toByteArray();
    if (savedGeometry.isEmpty())
    {
        resize(G_WINDOW_DEFAULT_WIDTH, G_WINDOW_DEFAULT_HEIGHT);
    }
    else
    {
        restoreGeometry(savedGeometry);
    }

    setAttribute(Qt::WA_TranslucentBackground, false);

    setupUi();
    setupToolBar();
    setupTrayIcon();
    registerHotkeys();

    // 从配置恢复上次关闭时选用的截屏模式，读不到则默认画框
    QSettings initSettings;
    int savedMode = initSettings.value(G_CONFIG_KEY_CAPTURE_MODE,
                                       static_cast<int>(CaptureMode::Region)).toInt();
    m_mode = static_cast<CaptureMode>(savedMode);
    m_toolBar->setCaptureMode(savedMode);
}

// -----------------------------------------------------------------------------
// UI 构建
// -----------------------------------------------------------------------------
void MainWindow::setupUi()
{
    m_centralStack = new QStackedWidget(this);
    setCentralWidget(m_centralStack);

    // 页面 0：占位（初始）
    m_placeholder = new QLabel(this);
    m_placeholder->setObjectName("placeholderLabel");
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setTextFormat(Qt::RichText);
    m_placeholder->setText(tr(
        "<div style='text-align: center;'>"
        "<div style='font-size: 20px; font-weight: 700; margin-bottom: 24px; "
        "color: #4A3F6E; letter-spacing: 1px;'>ScreenKiller</div>"
        "<table style='margin: 0 auto; font-size: 14px; line-height: 2.4; "
        "border-collapse: collapse;'>"
        "<tr>"
        "<td style='text-align: right; padding-right: 40px; "
        "color: #6B5B95; font-weight: 600; white-space: nowrap;'>"
        "Ctrl + Alt + A</td>"
        "<td style='text-align: left; color: #3A3357;'>开始截屏</td>"
        "</tr>"
        "<tr>"
        "<td style='text-align: right; padding-right: 40px; "
        "color: #6B5B95; font-weight: 600; white-space: nowrap;'>Tab</td>"
        "<td style='text-align: left; color: #3A3357;'>切换截屏模式</td>"
        "</tr>"
        "<tr>"
        "<td style='text-align: right; padding-right: 40px; "
        "color: #6B5B95; font-weight: 600; white-space: nowrap;'>Esc</td>"
        "<td style='text-align: left; color: #3A3357;'>退出或取消截屏</td>"
        "</tr>"
        "<tr>"
        "<td style='text-align: right; padding-right: 40px; "
        "color: #6B5B95; font-weight: 600; white-space: nowrap;'>Enter</td>"
        "<td style='text-align: left; color: #3A3357;'>完成滚动截屏</td>"
        "</tr>"
        "</table>"
        "</div>"
    ));
    m_centralStack->addWidget(m_placeholder);

    // Tab 快捷键：循环切换截屏模式（0->1->2->3->0->...）
    auto* tabShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    tabShortcut->setContext(Qt::ApplicationShortcut);
    connect(tabShortcut, &QShortcut::activated, this, [this]()
    {
        int nextMode = (static_cast<int>(m_mode) + 1) % 4;
        m_mode = static_cast<CaptureMode>(nextMode);
        m_toolBar->setCaptureMode(nextMode);
        Q_EMIT m_toolBar->captureModeChanged(nextMode);
        SK_LOG_INFO() << "Tab 切换截屏模式为:" << nextMode;
    });

    // 页面 1：标注视口（初始隐藏，截屏完成后显示）
    m_scene = new AnnotationScene(this);
    m_view  = new AnnotationView(m_scene, this);
    m_view->setObjectName("annotationView");
    m_centralStack->addWidget(m_view);

    m_centralStack->setCurrentIndex(G_PAGE_PLACEHOLDER);
}

void MainWindow::setupToolBar()
{
    m_toolBar = new ToolBar(this);
    addToolBar(Qt::TopToolBarArea, m_toolBar);

    connect(m_toolBar, &ToolBar::captureClicked, this, &MainWindow::onCaptureButtonClicked);
    connect(m_toolBar, &ToolBar::captureModeChanged, this, &MainWindow::onCaptureModeChanged);
    // 最小化按钮：隐藏主窗口，缩到系统托盘（任务栏保持清爽）
    connect(m_toolBar, &ToolBar::minimizeRequested, this, [this]()
    {
        hide();
    });
    // 关闭按钮：发射 requestQuit 信号，由 main.cpp 连接 QApplication::quit
    connect(m_toolBar, &ToolBar::closeRequested, this, &MainWindow::onQuitRequested);
    connect(m_toolBar, &ToolBar::saveRequested,  this, &MainWindow::onSaveRequested);
}

void MainWindow::setupTrayIcon()
{
    m_tray = new QSystemTrayIcon(QIcon(":/icons/app_alph.png"), this);
    m_tray->setToolTip(tr("ScreenKiller"));
    m_tray->show();

    auto* menu = new QMenu(this);
    auto* actShow = menu->addAction(tr("显示主窗口"));
    QAction* actCap  = menu->addAction(tr("截屏  Ctrl+Alt+A"));
    menu->addSeparator();
    QAction* actQuit = menu->addAction(tr("退出"));

    connect(actShow, &QAction::triggered, this, &QWidget::showNormal);
    connect(actCap,  &QAction::triggered, this, &MainWindow::onCaptureButtonClicked);
    connect(actQuit, &QAction::triggered, this, &MainWindow::onQuitRequested);
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
}

void MainWindow::registerHotkeys()
{
    m_hotkey = new GlobalHotkey(this);
    // Ctrl + Alt + A
    bool isRegistered = m_hotkey->registerShortcut(kHotKeyId, MOD_CONTROL | MOD_ALT, 'A');
    if (!isRegistered)
    {
        SK_LOG_WARN() << "全局快捷键 Ctrl+Alt+A 注册失败，可能已被占用。";
        SK::utils::showWarning(
            this,
            tr("警告"),
            tr("全局快捷键  Ctrl + Alt + A  已占用。\n"
               "将无法使用快捷键截图，请关闭冲突程序后重启 ScreenKiller。"));
    }
}

void MainWindow::unregisterHotkeys()
{
    if (m_hotkey != nullptr)
    {
        m_hotkey->unregisterAll();
    }
}

// -----------------------------------------------------------------------------
// 事件
// -----------------------------------------------------------------------------
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if ((eventType == "windows_generic_MSG") || (eventType == "windows_dispatcher_MSG"))
    {
        auto* msg = reinterpret_cast<MSG*>(message);

        // 全局快捷键：Ctrl+Alt+A
        if (msg->message == WM_HOTKEY)
        {
            if (msg->wParam == kHotKeyId)
            {
                onCaptureButtonClicked();
                *result = 0;
                return true;
            }
        }

        // 无边框窗口边缘缩放：拦截命中测试，模拟窗口边框
        if (msg->message == WM_NCHITTEST)
        {
            return handleNcHitTest(msg, result);
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

#ifdef Q_OS_WIN
/// @brief 处理 WM_NCHITTEST：在无边框窗口边缘返回对应命中码
///
/// Windows 通过 WM_NCHITTEST 询问鼠标命中了窗口的哪个区域。
/// 默认情况下 FramelessWindowHint 窗口全部返回 HTCLIENT，
/// 导致用户无法拖拽边缘缩放。
///
/// 本方法在窗口外缘 G_RESIZE_BORDER_WIDTH 像素的边缘带内，
/// 返回 HTLEFT / HTTOP / HTRIGHT / HTBOTTOM 等命中码，
/// 让 Windows 自动接管缩放处理（含 Aero Snap）。
///
/// @param msg       Windows 消息指针
/// @param result    [out] 命中码（HTLEFT 等）
/// @return true 表示已处理；false 表示交给 Qt 默认处理
bool MainWindow::handleNcHitTest(MSG* msg, qintptr* result)
{
    // 鼠标屏幕坐标
    long mouseX = GET_X_LPARAM(msg->lParam);
    long mouseY = GET_Y_LPARAM(msg->lParam);

    // 窗口屏幕矩形
    RECT winRect;
    if (GetWindowRect(msg->hwnd, &winRect) == 0)
    {
        return false;
    }

    // 判断鼠标是否落在各方向边缘带
    bool isLeft   = (mouseX >= winRect.left)
                    && (mouseX <  winRect.left + G_RESIZE_BORDER_WIDTH);
    bool isRight  = (mouseX >= winRect.right - G_RESIZE_BORDER_WIDTH)
                    && (mouseX <  winRect.right);
    bool isTop    = (mouseY >= winRect.top)
                    && (mouseY <  winRect.top + G_RESIZE_BORDER_WIDTH);
    bool isBottom = (mouseY >= winRect.bottom - G_RESIZE_BORDER_WIDTH)
                    && (mouseY <  winRect.bottom);

    // 四个角落优先（双向缩放）
    if (isTop && isLeft)
    {
        *result = HTTOPLEFT;
        return true;
    }
    if (isTop && isRight)
    {
        *result = HTTOPRIGHT;
        return true;
    }
    if (isBottom && isLeft)
    {
        *result = HTBOTTOMLEFT;
        return true;
    }
    if (isBottom && isRight)
    {
        *result = HTBOTTOMRIGHT;
        return true;
    }
    // 四条边（单向缩放）
    if (isLeft)
    {
        *result = HTLEFT;
        return true;
    }
    if (isRight)
    {
        *result = HTRIGHT;
        return true;
    }
    if (isTop)
    {
        *result = HTTOP;
        return true;
    }
    if (isBottom)
    {
        *result = HTBOTTOM;
        return true;
    }

    // 鼠标在客户区，交给 Qt 默认处理（用于工具栏拖拽等）
    return false;
}
#endif // Q_OS_WIN

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 保存窗口几何状态和截屏模式到系统目录下的 INI 文件
    QSettings settings;
    settings.setValue(G_CONFIG_KEY_GEOMETRY, saveGeometry());
    settings.setValue(G_CONFIG_KEY_CAPTURE_MODE, static_cast<int>(m_mode));

    unregisterHotkeys();
    QMainWindow::closeEvent(event);
}

// -----------------------------------------------------------------------------
// 槽
// -----------------------------------------------------------------------------
void MainWindow::onCaptureButtonClicked()
{
    startCapture(m_mode);
}

void MainWindow::onCaptureModeChanged(int mode)
{
    m_mode = static_cast<CaptureMode>(mode);
    SK_LOG_INFO() << "截屏模式切换为:" << mode;
}

void MainWindow::onCaptureFinished(const QImage& image)
{
    if (image.isNull())
    {
        SK_LOG_WARN() << "截屏结果为空。";
        SK::utils::showWarning(
            this,
            tr("警告"),
            tr("未能获取到截图，请重试。"));
        showNormal();
        return;
    }

    // 将图片加载到标注场景
    m_scene->loadImage(image);

    // 切换到标注视口
    m_centralStack->setCurrentIndex(G_PAGE_ANNOTATION);

    // 显示保存按钮
    m_toolBar->setShowSaveButton(true);

    // 截图成功后自动复制到剪贴板
    QGuiApplication::clipboard()->setImage(image, QClipboard::Clipboard);

    // 用系统托盘气泡提示用户
    if (m_tray != nullptr)
    {
        m_tray->showMessage(
            tr("截图完成"),
            tr("图片已复制到剪贴板！"),
            QSystemTrayIcon::Information,
            G_TRAY_NOTICE_DURATION_MS);
    }

    // 显示并激活主窗口
    showNormal();
    raise();
    activateWindow();

    // 等待视口布局完成后自适应显示
    QTimer::singleShot(0, m_view, &AnnotationView::fitToView);
}

void MainWindow::onCaptureCancelled()
{
    SK_LOG_INFO() << "用户取消截屏。";
    // 取消截屏后恢复主窗口
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ((reason == QSystemTrayIcon::Trigger) || (reason == QSystemTrayIcon::DoubleClick))
    {
        showNormal();
        raise();
        activateWindow();
    }
}

void MainWindow::onQuitRequested()
{
    // 发射退出信号，由 main.cpp 连接到 QApplication::quit
    // 资源清理依赖 Qt 对象树：MainWindow 析构时 children 自动 delete
    // GlobalHotkey 析构会自动调用 UnregisterHotKey
    Q_EMIT requestQuit();
}

void MainWindow::onSaveRequested()
{
    if (m_scene == nullptr)
    {
        return;
    }

    QImage img = m_scene->exportImage();
    if (img.isNull())
    {
        SK_LOG_WARN() << "导出图像为空，无法保存。";
        SK::utils::showWarning(this, tr("警告"), tr("没有可保存的截图。"));
        return;
    }

    // 默认保存路径：C:\Users\<user>\Pictures\Screenshots
    QString defaultDir =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/") + G_SCREENSHOT_SUBDIR;
    QDir().mkpath(defaultDir);

    QString timestamp = QDateTime::currentDateTime().toString(G_TIMESTAMP_FORMAT);
    QString defaultPath = defaultDir
                          + QStringLiteral("/")
                          + G_SCREENSHOT_PREFIX
                          + timestamp
                          + QStringLiteral(".png");

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("保存截图"), defaultPath,
        tr("PNG 图像 (*.png);;JPEG 图像 (*.jpg);;BMP 图像 (*.bmp)"));
    if (filePath.isEmpty())
    {
        return;
    }

    // 根据扩展名选择格式
    QFileInfo fileInfo(filePath);
    QByteArray format = chooseImageFormat(fileInfo.suffix().toLower());

    bool isSaved = img.save(filePath, format.constData());
    if (!isSaved)
    {
        SK::utils::showCritical(this, tr("异常"),
                                tr("无法写入文件：\n%1").arg(filePath));
        return;
    }
    SK_LOG_INFO() << "截图已保存至:" << filePath;
}

QByteArray MainWindow::chooseImageFormat(const QString& suffix) const
{
    if ((suffix == QStringLiteral("jpg")) || (suffix == QStringLiteral("jpeg")))
    {
        return "JPG";
    }
    if (suffix == QStringLiteral("bmp"))
    {
        return "BMP";
    }
    return "PNG";
}

// -----------------------------------------------------------------------------
// 启动截屏
// -----------------------------------------------------------------------------
void MainWindow::startCapture(CaptureMode mode)
{
    // 隐藏主窗口避免被截入
    hide();
    QGuiApplication::processEvents();
    QTimer::singleShot(G_CAPTURE_DELAY_MS, this, [this, mode]()
    {
        if (m_capture == nullptr)
        {
            m_capture = new CaptureEngine(this);
            connect(m_capture, &CaptureEngine::captureFinished,
                    this, &MainWindow::onCaptureFinished);
            connect(m_capture, &CaptureEngine::captureCancelled,
                    this, &MainWindow::onCaptureCancelled);
        }
        m_capture->start(static_cast<CaptureEngine::Mode>(mode));
    });
}

} // namespace SK
