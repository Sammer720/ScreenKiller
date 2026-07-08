/**
 * \file ScrollCapture.cpp
 * \brief 滚动截屏实现
 */
#include "ScrollCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QPixmap>
#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <windowsx.h>
#endif

#include "RegionSelector.h"
#include "stitcher/ImageStitcher.h"
#include "utils/Logger.h"
#include "utils/MessageBox.h"

namespace {

/// \brief 鼠标滚轮 WHEEL_DELTA 常量（Windows 定义为 120）
constexpr int G_WHEEL_DELTA = 120;
/// \brief 滚动步长基数（每行对应的 WHEEL_DELTA 除数）
constexpr int G_SCROLL_DIVISOR = 3;
/// \brief grabWindow 的全屏参数（0 表示整个屏幕）
const WId G_FULLSCREEN_WID = 0;
/// \brief 单帧场景标志（仅一帧时无需拼接）
constexpr int G_SINGLE_FRAME = 1;

} // namespace

ScrollCapture::ScrollCapture(QObject* parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ScrollCapture::captureNextFrame);
}

ScrollCapture::~ScrollCapture() = default;

void ScrollCapture::start()
{
    // 第一步：让用户框选要截屏的区域
    if (m_selector == nullptr)
    {
        m_selector = new RegionSelector();
        connect(m_selector, &RegionSelector::regionSelected,
                this, &ScrollCapture::onRegionSelected);
        connect(m_selector, &RegionSelector::cancelled,
                this, &ScrollCapture::onRegionCancelled);
    }
    m_selector->start();
}

void ScrollCapture::onRegionSelected(const QRect& rect)
{
    m_targetRect = rect;

#ifdef Q_OS_WIN
    // 找到区域中心的窗口作为滚动目标
    POINT pt{ rect.center().x(), rect.center().y() };
    m_targetHwnd = WindowFromPoint(pt);
    // Fail-Fast：无法定位目标窗口时取消
    if (m_targetHwnd == nullptr)
    {
        SK_LOG_WARN() << "无法定位滚动目标窗口。";
        SK::utils::showWarning(
            qobject_cast<QWidget*>(parent()),
            QStringLiteral("警告"),
            QStringLiteral("无法定位滚动目标窗口，请将鼠标悬停在可滚动窗口上再启动。"));
        Q_EMIT cancelled();
        return;
    }
    // 将焦点设到目标窗口
    SetForegroundWindow(m_targetHwnd);
#endif

    if (m_stitcher == nullptr)
    {
        m_stitcher = new ImageStitcher();
    }

    m_frames.clear();
    m_frameCount = 0;

    // 立即抓取第一帧
    captureNextFrame();
}

void ScrollCapture::onRegionCancelled()
{
    Q_EMIT cancelled();
}

// -----------------------------------------------------------------------------
// 帧抓取循环
// -----------------------------------------------------------------------------
void ScrollCapture::captureNextFrame()
{
    // 达到最大帧数限制时停止
    if (m_frameCount >= m_maxFrames)
    {
        SK_LOG_INFO() << "达到最大帧数限制，停止滚动截屏。";
        finishStitching();
        return;
    }

    auto* screen = QGuiApplication::primaryScreen();
    // Fail-Fast：屏幕无效时取消
    if (screen == nullptr)
    {
        Q_EMIT cancelled();
        return;
    }

    // 抓取屏幕 -> 裁剪到目标区域
    QPixmap full = screen->grabWindow(G_FULLSCREEN_WID);
    QImage frame = full.copy(m_targetRect).toImage();

    // Fail-Fast：抓取失败时取消
    if (frame.isNull())
    {
        SK_LOG_WARN() << "抓取帧失败。";
        SK::utils::showWarning(
            qobject_cast<QWidget*>(parent()),
            QStringLiteral("警告"),
            QStringLiteral("抓取屏幕帧失败，滚动截屏已中止。"));
        Q_EMIT cancelled();
        return;
    }

    // 检测是否已经滚动到底（与上一帧完全相同）
    if (!m_lastFrame.isNull() && (m_lastFrame == frame))
    {
        SK_LOG_INFO() << "检测到滚动停滞，结束抓取。";
        finishStitching();
        return;
    }

    m_frames.append(frame);
    m_lastFrame = frame;
    m_frameCount++;
    Q_EMIT progress(m_frameCount, m_maxFrames);

    // 触发滚轮
    scrollTarget();

    // 等待滚动稳定后抓取下一帧
    m_timer.start(m_frameIntervalMs);
}

void ScrollCapture::scrollTarget()
{
#ifdef Q_OS_WIN
    // Fail-Fast：目标窗口无效时不滚动
    if (m_targetHwnd == nullptr)
    {
        return;
    }

    // 将光标移到目标窗口中心，确保滚轮事件投递到正确窗口
    QPoint center = m_targetRect.center();
    SetCursorPos(center.x(), center.y());

    INPUT input{};
    input.type       = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    // WHEEL_DELTA = 120，每行 = 120/3
    input.mi.mouseData = static_cast<DWORD>(G_WHEEL_DELTA * m_scrollLines / G_SCROLL_DIVISOR);
    SendInput(1, &input, sizeof(INPUT));
#endif
}

bool ScrollCapture::isScrollStuck()
{
    // 已在 captureNextFrame 中通过帧比对实现
    return false;
}

void ScrollCapture::finishStitching()
{
    // Fail-Fast：没有帧时取消
    if (m_frames.isEmpty())
    {
        Q_EMIT cancelled();
        return;
    }
    // 仅一帧时无需拼接
    if (m_frames.size() == G_SINGLE_FRAME)
    {
        Q_EMIT captureFinished(m_frames.first());
        return;
    }

    SK_LOG_INFO() << "开始拼接" << m_frames.size() << "帧...";
    QImage merged = m_stitcher->stitchVertical(m_frames);
    Q_EMIT captureFinished(merged);
}
