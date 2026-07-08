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

namespace {

/// \brief 选区最小有效尺寸（宽度或高度阈值）
constexpr int G_MIN_VALID_SIZE = 1;
/// \brief grabWindow 的全屏参数（0 表示整个屏幕）
const WId G_FULLSCREEN_WID = 0;

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
    QImage img = grabWindow(hwnd);
    if (img.isNull())
    {
        Q_EMIT captureCancelled();
    }
    else
    {
        Q_EMIT captureFinished(img);
    }
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
    auto* screen = QGuiApplication::primaryScreen();
    // Fail-Fast：屏幕无效时返回空图像
    if (screen == nullptr)
    {
        return {};
    }
    // QScreen::grabWindow 接收 WId
    return screen->grabWindow(reinterpret_cast<WId>(hwnd)).toImage();
#else
    Q_UNUSED(hwnd);
    return {};
#endif
}
