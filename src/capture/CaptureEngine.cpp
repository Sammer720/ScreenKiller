/**
 * \file CaptureEngine.cpp
 * \brief 截屏引擎实现
 */
#include "CaptureEngine.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

#include "RegionSelector.h"
#include "WindowSelector.h"
#include "ScrollCapture.h"
#include "utils/Logger.h"
#include "platform/WinApi.h"

namespace {

/// \brief 选区最小有效尺寸（宽度或高度阈值）
constexpr int G_MIN_VALID_SIZE = 1;
/// \brief grabWindow 的全屏参数（0 表示整个屏幕）
const WId G_FULLSCREEN_WID = 0;
/// \brief 置前目标窗口后等待 DWM 刷新的延迟（毫秒）
constexpr int G_DWM_REFRESH_MS = 200;

} // namespace

CaptureEngine::CaptureEngine(QObject* parent)
    : QObject(parent)
{
}

CaptureEngine::~CaptureEngine() = default;

void CaptureEngine::start(Mode mode)
{
    SK_LOG_INFO() << "启动截屏，模式:" << static_cast<int>(mode);

    switch (mode)
    {
    case Mode::Region:
    {
        if (m_regionSelector == nullptr)
        {
            m_regionSelector = new RegionSelector();
            connect(m_regionSelector, &RegionSelector::regionSelected,
                    this, &CaptureEngine::onRegionSelected);
            connect(m_regionSelector, &RegionSelector::cancelled,
                    this, &CaptureEngine::captureCancelled);
        }
        m_regionSelector->start();
        break;
    }

    case Mode::FullScreen:
    {
        QImage img = grabFullScreen();
        if (img.isNull())
        {
            Q_EMIT captureCancelled();
        }
        else
        {
            Q_EMIT captureFinished(img);
        }
        break;
    }

    case Mode::Window:
    {
        if (m_windowSelector == nullptr)
        {
            m_windowSelector = new WindowSelector();
            connect(m_windowSelector, &WindowSelector::windowSelected,
                    this, &CaptureEngine::onWindowSelected);
            connect(m_windowSelector, &WindowSelector::cancelled,
                    this, &CaptureEngine::captureCancelled);
        }
        m_windowSelector->start();
        break;
    }

    case Mode::Scrolling:
    {
        if (m_scrollCapture == nullptr)
        {
            m_scrollCapture = new ScrollCapture();
            connect(m_scrollCapture, &ScrollCapture::captureFinished,
                    this, &CaptureEngine::onScrollingFinished);
            connect(m_scrollCapture, &ScrollCapture::cancelled,
                    this, &CaptureEngine::captureCancelled);
        }
        m_scrollCapture->start();
        break;
    }
    }
}

// -----------------------------------------------------------------------------
// 内部槽
// -----------------------------------------------------------------------------
void CaptureEngine::onRegionSelected(const QRect& rect)
{
    // 选区无效时取消
    if (rect.isNull()
        || (rect.width() <= G_MIN_VALID_SIZE)
        || (rect.height() <= G_MIN_VALID_SIZE))
    {
        Q_EMIT captureCancelled();
        return;
    }
    QImage img = grabRegion(rect);
    if (img.isNull())
    {
        Q_EMIT captureCancelled();
    }
    else
    {
        Q_EMIT captureFinished(img);
    }
}

void CaptureEngine::onWindowSelected(HWND hwnd)
{
#ifdef Q_OS_WIN
    // 窗口句柄无效时取消
    if (hwnd == nullptr)
    {
        Q_EMIT captureCancelled();
        return;
    }

    // 关闭窗口选择遮罩（T5 已在发射信号前关闭，此处做二次保险）
    if (m_windowSelector != nullptr)
    {
        m_windowSelector->close();
    }

    // 记录当前前台窗口，用于截屏完成后恢复焦点
    HWND previousForeground = GetForegroundWindow();

    // 将目标窗口提到前台，使其内容在截屏时可见且不被遮挡
    // 即便置前失败，仍按延迟流程抓取（退化为遮挡状态下的截屏）
    SK::WinApi::setForegroundWindow(hwnd);

    // 延迟等待 DWM 刷新后再抓取，避免捕获到过渡帧或残留遮罩
    QTimer::singleShot(G_DWM_REFRESH_MS, this, [this, hwnd, previousForeground]()
    {
        QImage img = grabWindow(hwnd);

        // 恢复之前的前台窗口；若该窗口已关闭则跳过，避免激活无效句柄
        if ((previousForeground != nullptr)
            && (IsWindow(previousForeground) != 0))
        {
            SetForegroundWindow(previousForeground);
        }

        if (img.isNull())
        {
            Q_EMIT captureCancelled();
        }
        else
        {
            Q_EMIT captureFinished(img);
        }
    });
#else
    Q_UNUSED(hwnd);
    Q_EMIT captureCancelled();
#endif
}

void CaptureEngine::onScrollingFinished(const QImage& image)
{
    if (image.isNull())
    {
        Q_EMIT captureCancelled();
    }
    else
    {
        Q_EMIT captureFinished(image);
    }
}

// -----------------------------------------------------------------------------
// 抓取实现
// -----------------------------------------------------------------------------
QImage CaptureEngine::grabFullScreen()
{
    auto* screen = QGuiApplication::primaryScreen();
    // Fail-Fast：屏幕无效时返回空图像
    if (screen == nullptr)
    {
        return {};
    }
    return screen->grabWindow(G_FULLSCREEN_WID).toImage();
}

QImage CaptureEngine::grabRegion(const QRect& rect)
{
    QImage full = grabFullScreen();
    // Fail-Fast：全屏抓取失败时返回空图像
    if (full.isNull())
    {
        return {};
    }
    // 设备无关像素 -> 设备像素
    qreal ratio = full.devicePixelRatio();
    QRectF deviceRect(rect.x() * ratio, rect.y() * ratio,
                      rect.width() * ratio, rect.height() * ratio);
    return full.copy(deviceRect.toAlignedRect());
}

QImage CaptureEngine::grabWindow(HWND hwnd)
{
#ifdef Q_OS_WIN
    // 窗口句柄无效时取消
    if (hwnd == nullptr)
    {
        return {};
    }

    // 获取窗口矩形（屏幕坐标）
    QRect windowRect = SK::WinApi::getWindowFrameRect(hwnd);
    if (windowRect.isNull())
    {
        return {};
    }

    // 截取全屏后裁剪到窗口区域
    // 注意：不能使用 QScreen::grabWindow(WId(hwnd))，因为 Qt 在 Windows 上
    // 使用 PrintWindow API，对大量窗口类型（DirectX/OpenGL/WPF/UWP 等）返回黑屏
    // 全屏截取+裁剪方式与 RegionSelector 一致，且 WindowSelector 遮罩在
    // 窗口矩形区域是挖空的，裁剪内容是正确的真实窗口像素
    QImage full = grabFullScreen();
    if (full.isNull())
    {
        return {};
    }

    // 设备无关像素 -> 设备像素
    qreal ratio = full.devicePixelRatio();
    QRectF deviceRect(windowRect.x() * ratio, windowRect.y() * ratio,
                      windowRect.width() * ratio, windowRect.height() * ratio);
    return full.copy(deviceRect.toAlignedRect());
#else
    Q_UNUSED(hwnd);
    return {};
#endif
}
