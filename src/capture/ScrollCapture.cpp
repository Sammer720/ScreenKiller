/**
 * \file ScrollCapture.cpp
 * \brief 手动滚动截屏实现
 */
#include "ScrollCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "RegionSelector.h"
#include "ScrollOverlay.h"
#include "platform/MouseWheelHook.h"
#include "platform/KeyboardHook.h"
#include "stitcher/ImageStitcher.h"
#include "utils/Logger.h"
#include "utils/MessageBox.h"

namespace {

/// \brief grabWindow 的全屏参数（0 表示整个屏幕）
const WId G_FULLSCREEN_WID = 0;
/// \brief 单帧场景标志（仅一帧时无需拼接）
constexpr int G_SINGLE_FRAME = 1;

} // namespace

ScrollCapture::ScrollCapture(QObject* parent)
    : QObject(parent)
{
}

ScrollCapture::~ScrollCapture()
{
    stopListening();

    delete m_overlay;
    m_overlay = nullptr;

    delete m_hook;
    m_hook = nullptr;

    delete m_keyboardHook;
    m_keyboardHook = nullptr;
}

void ScrollCapture::start()
{
    m_state = State::SelectingRegion;

    if (m_selector == nullptr)
    {
        m_selector = new RegionSelector();
        connect(m_selector, &RegionSelector::regionSelected,
                this, &ScrollCapture::onRegionSelected);
        connect(m_selector, &RegionSelector::cancelled,
                this, &ScrollCapture::onRegionCancelled);
    }
    // 滚动截屏模式下：画框后遮罩保持可见，便于用户观察选区
    m_selector->setKeepOpen(true);
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
        if (m_selector != nullptr)
        {
            m_selector->finish();
        }
        Q_EMIT cancelled();
        return;
    }

    // 将焦点设到目标窗口，让用户的滚轮事件能正常投递
    SetForegroundWindow(m_targetHwnd);
#endif

    if (m_stitcher == nullptr)
    {
        m_stitcher = new ImageStitcher();
    }

    m_frames.clear();
    m_frameCount = 0;
    m_state = State::Capturing;

    // 立即抓取第一帧
    captureFrame();

    // 安装滚轮钩子
    if (m_hook == nullptr)
    {
        m_hook = new MouseWheelHook(this);
        connect(m_hook, &MouseWheelHook::wheelScrolled,
                this, &ScrollCapture::onWheelScrolled);
    }
    m_hook->install();

    // 安装键盘钩子，拦截 Esc（取消）和 Enter（完成）
    if (m_keyboardHook == nullptr)
    {
        m_keyboardHook = new KeyboardHook(this);
        connect(m_keyboardHook, &KeyboardHook::cancelTriggered,
                this, &ScrollCapture::onCancelRequested);
        connect(m_keyboardHook, &KeyboardHook::finishTriggered,
                this, &ScrollCapture::onFinishRequested);
    }
    m_keyboardHook->install();

    // 显示提示浮窗
    if (m_overlay == nullptr)
    {
        m_overlay = new ScrollOverlay();
        connect(m_overlay, &ScrollOverlay::finishRequested,
                this, &ScrollCapture::onFinishRequested);
        connect(m_overlay, &ScrollOverlay::cancelRequested,
                this, &ScrollCapture::onCancelRequested);
    }
    m_overlay->setFrameCount(m_frameCount);
    m_overlay->show();
}

void ScrollCapture::onRegionCancelled()
{
    if (m_selector != nullptr)
    {
        m_selector->finish();
    }
    Q_EMIT cancelled();
}

void ScrollCapture::onWheelScrolled(int delta, const QPoint& pos)
{
    // 仅在捕获状态下响应滚轮
    if (m_state != State::Capturing)
    {
        return;
    }

    // 只处理向下滚动
    if (delta >= 0)
    {
        return;
    }

    // 只处理发生在选区内的滚轮事件
    if (!m_targetRect.contains(pos))
    {
        return;
    }

    // 累积滚动像素值，达到阈值时抓帧（不再使用定时器去抖）
    m_accumulatedDelta += qAbs(delta);
    if (m_accumulatedDelta < m_scrollPxThreshold)
    {
        return;
    }
    m_accumulatedDelta = 0;

    captureFrame();

    if (m_frameCount >= m_maxFrames)
    {
        stopListening();
        finishStitching();
    }
}

void ScrollCapture::onFinishRequested()
{
    if (m_state != State::Capturing)
    {
        return;
    }

    // 关闭遮罩
    if (m_selector != nullptr)
    {
        m_selector->finish();
    }

    stopListening();

    // 最后一次截屏，确保捕获当前最终画面
    captureFrame();

    finishStitching();
}

void ScrollCapture::onCancelRequested()
{
    if (m_state != State::Capturing)
    {
        return;
    }

    // 关闭遮罩
    if (m_selector != nullptr)
    {
        m_selector->finish();
    }

    stopListening();
    Q_EMIT cancelled();
}

void ScrollCapture::captureFrame()
{
    auto* screen = QGuiApplication::primaryScreen();
    // Fail-Fast：屏幕无效时取消
    if (screen == nullptr)
    {
        stopListening();
        Q_EMIT cancelled();
        return;
    }

    // 遮罩在选区内部已透明，无需隐藏即可直接抓取
    // 抓取屏幕 -> 裁剪到目标区域（含 DPI 校正）
    QPixmap full = screen->grabWindow(G_FULLSCREEN_WID);
    qreal ratio = full.devicePixelRatio();
    QRectF deviceRect(m_targetRect.x() * ratio, m_targetRect.y() * ratio,
                      m_targetRect.width() * ratio, m_targetRect.height() * ratio);
    QImage frame = full.copy(deviceRect.toAlignedRect()).toImage();

    // Fail-Fast：抓取失败时取消
    if (frame.isNull())
    {
        SK_LOG_WARN() << "抓取帧失败。";
        SK::utils::showWarning(
            qobject_cast<QWidget*>(parent()),
            QStringLiteral("警告"),
            QStringLiteral("抓取屏幕帧失败，滚动截屏已中止。"));
        stopListening();
        Q_EMIT cancelled();
        return;
    }

    // 与上一帧完全相同则跳过，避免到底后重复抓帧
    if (!m_frames.isEmpty() && (m_frames.last() == frame))
    {
        return;
    }

    m_frames.append(frame);
    m_frameCount++;

    if (m_overlay != nullptr)
    {
        m_overlay->setFrameCount(m_frameCount);
    }

    Q_EMIT progress(m_frameCount, m_maxFrames);
}

void ScrollCapture::finishStitching()
{
    m_state = State::Stitching;

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

void ScrollCapture::stopListening()
{
    if (m_hook != nullptr)
    {
        m_hook->uninstall();
    }

    if (m_keyboardHook != nullptr)
    {
        m_keyboardHook->uninstall();
    }

    if (m_overlay != nullptr)
    {
        m_overlay->hide();
    }
}
