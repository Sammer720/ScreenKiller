/**
 * \file ScrollCapture.cpp
 * \brief 手动滚动截屏实现（线程化版本）
 *
 * 架构：
 *   主线程：grabWindow + copy → 入队（仅 ~20ms/2K，远低于 300ms 钩子超时）
 *   帧处理线程：dequeue → isBlackFrame → 重复检查 → append → emit进度
 *   拼接线程（QtConcurrent）：stitchVertical（每帧对检查取消标志）
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
#include "stitcher/StitchWorker.h"
#include "capture/FrameQueue.h"
#include "utils/Logger.h"
#include "utils/MessageBox.h"

namespace {

/// \brief grabWindow 的全屏参数（0 表示整个屏幕）
const WId G_FULLSCREEN_WID = 0;
/// \brief 单帧场景标志（仅一帧时无需拼接）
constexpr int G_SINGLE_FRAME = 1;
/// \brief 判定"接近纯黑"的灰度上限
constexpr int G_BLACK_PIXEL_THRESHOLD = 16;
/// \brief 纯黑像素占比超过此值判为黑帧
constexpr double G_BLACK_FRAME_RATIO = 0.70;

/**
 * @brief 判断帧是否为黑帧（≥70% 像素灰度 ≤16）
 *
 * 用"纯黑占比"而非"低方差"——暗色主题应用背景是深灰但不会
 * 70% 像素接近纯黑，因此不会误杀。
 *
 * @param frame 待检测帧
 * @param threshold 灰度判定阈值（≤此值视为接近纯黑）
 * @param ratio 纯黑像素占比阈值（超过此值判为黑帧）
 * @return true 表示是黑帧，应丢弃
 */
static bool isBlackFrame(const QImage& frame,
                         int threshold = G_BLACK_PIXEL_THRESHOLD,
                         double ratio = G_BLACK_FRAME_RATIO)
{
    if (frame.isNull())
    {
        return true;
    }

    QImage gray = frame.convertToFormat(QImage::Format_Grayscale8);
    int totalPixels = gray.width() * gray.height();
    if (totalPixels <= 0)
    {
        return true;
    }

    int blackCount = 0;
    for (int y = 0; y < gray.height(); ++y)
    {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
        {
            if (line[x] <= threshold)
            {
                ++blackCount;
            }
        }
    }

    return (static_cast<double>(blackCount) / totalPixels) >= ratio;
}

} // namespace

// =============================================================================
// 构造 / 析构
// =============================================================================
ScrollCapture::ScrollCapture(QObject* parent)
    : QObject(parent)
{
}

ScrollCapture::~ScrollCapture()
{
    // 安全退出线程
    stopFrameThread();
    stopListening();

    delete m_stitchWorker;
    m_stitchWorker = nullptr;

    delete m_frameQueue;
    m_frameQueue = nullptr;

    delete m_overlay;
    m_overlay = nullptr;

    delete m_hook;
    m_hook = nullptr;

    delete m_keyboardHook;
    m_keyboardHook = nullptr;

    delete m_stitcher;
    m_stitcher = nullptr;
}

// =============================================================================
// 启动
// =============================================================================
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

    m_frames.clear();
    m_frameCount = 0;
    m_processedFrameCount.store(0);
    m_state = State::Capturing;

    // 创建帧队列和帧处理线程
    if (m_frameQueue == nullptr)
    {
        m_frameQueue = new FrameQueue(5);
    }
    else
    {
        m_frameQueue->clear();
    }
    startFrameThread();

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

// =============================================================================
// 滚轮事件
// =============================================================================
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

    // 达到最大帧数：进入 Flushing → Stitching
    if (m_frameCount >= m_maxFrames)
    {
        stopListening();
        beginFlushing();
    }
}

// =============================================================================
// 完成 / 取消
// =============================================================================
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

    beginFlushing();
}

void ScrollCapture::onCancelRequested()
{
    m_state = State::Idle;

    // 关闭遮罩
    if (m_selector != nullptr)
    {
        m_selector->finish();
    }

    // 取消拼接 worker（如果在运行）
    if (m_stitchWorker != nullptr)
    {
        m_stitchWorker->cancel();
    }

    stopListening();
    stopFrameThread();

    Q_EMIT cancelled();
}

// =============================================================================
// Flushing → Stitching 流程
// =============================================================================
void ScrollCapture::beginFlushing()
{
    m_state = State::Flushing;

    // 停止帧队列入队（不再 enqueue），等工作线程处理完剩余帧
    if (m_frameQueue != nullptr)
    {
        m_frameQueue->stop();  // 唤醒阻塞的 dequeue
    }

    // 等待帧处理线程排空并退出
    stopFrameThread();

    SK_LOG_INFO() << "帧处理完成，有效帧数:" << m_frames.size();

    // 进入拼接阶段
    finishStitching();
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

    // 创建拼接 worker（如果尚未创建）
    if (m_stitchWorker == nullptr)
    {
        m_stitchWorker = new StitchWorker(this);
        connect(m_stitchWorker, &StitchWorker::stitchFinished,
                this, &ScrollCapture::onStitchFinished);
        connect(m_stitchWorker, &StitchWorker::stitchCancelled,
                this, &ScrollCapture::onStitchCancelled);
    }

    SK_LOG_INFO() << "开始拼接" << m_frames.size() << "帧...";

    // 将 m_frames 所有权转移给 worker
    QVector<QImage> framesToStitch;
    framesToStitch.swap(m_frames);
    m_stitchWorker->run(std::move(framesToStitch));

    // overlay 显示"拼接中..."
    if (m_overlay != nullptr)
    {
        m_overlay->setFrameCount(-1);  // -1 表示拼接中
    }
}

void ScrollCapture::onStitchFinished(const QImage& result)
{
    SK_LOG_INFO() << "拼接完成，结果高度:" << result.height();
    Q_EMIT captureFinished(result);
}

void ScrollCapture::onStitchCancelled()
{
    SK_LOG_INFO() << "拼接取消";
    Q_EMIT cancelled();
}

void ScrollCapture::onFrameProcessed(int count)
{
    // 从工作线程通过 QueuedConnection 调用，安全更新 QWidget
    if (m_overlay != nullptr)
    {
        m_overlay->setFrameCount(count);
    }
    Q_EMIT progress(count, m_maxFrames);
}

// =============================================================================
// 抓帧（主线程，简化为 grab + enqueue）
// =============================================================================
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

    // 抓取屏幕 -> 裁剪到目标区域（含 DPI 校正）
    // 实际截屏区域比画框选区每边内缩 2px，避免截到蓝色边框
    QPixmap full = screen->grabWindow(G_FULLSCREEN_WID);
    qreal ratio = full.devicePixelRatio();
    QRect captureRect = m_targetRect.adjusted(1, 1, -1, -1);
    QRectF deviceRect(captureRect.x() * ratio, captureRect.y() * ratio,
                      captureRect.width() * ratio, captureRect.height() * ratio);
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

    // 深拷贝入队（脱离隐式共享，安全传递给工作线程）
    if (m_frameQueue != nullptr)
    {
        m_frameQueue->enqueue(frame.copy());
    }

    m_frameCount++;
}

// =============================================================================
// 帧处理工作线程
// =============================================================================
void ScrollCapture::startFrameThread()
{
    m_frameThreadRunning.store(true);

    m_frameThread = std::make_unique<std::thread>(&ScrollCapture::frameThreadLoop, this);
}

void ScrollCapture::stopFrameThread()
{
    if (m_frameQueue != nullptr)
    {
        m_frameQueue->stop();
    }

    m_frameThreadRunning.store(false);

    if (m_frameThread && m_frameThread->joinable())
    {
        m_frameThread->join();
    }
    m_frameThread.reset();
}

void ScrollCapture::frameThreadLoop()
{
    QImage lastValid;  // 上一帧有效帧（用于重复检查）

    while (m_frameThreadRunning.load())
    {
        QImage frame;
        // 阻塞等待，超时 500ms 检查运行标志
        if (!m_frameQueue->dequeue(frame, 500))
        {
            // 超时或队列已停止
            if (!m_frameThreadRunning.load())
            {
                break;
            }
            // 尝试排空剩余帧
            while (m_frameQueue->dequeue(frame, 0))
            {
                // 处理剩余帧
                processOneFrame(frame, lastValid);
            }
            continue;
        }

        processOneFrame(frame, lastValid);
    }

    // 排空队列中剩余帧
    QImage frame;
    while (m_frameQueue->dequeue(frame, 0))
    {
        processOneFrame(frame, lastValid);
    }
}

void ScrollCapture::processOneFrame(QImage& frame, QImage& lastValid)
{
    // 黑帧质量门：≥70% 像素接近纯黑 → 丢弃
    if (isBlackFrame(frame))
    {
        SK_LOG_CAP() << "丢弃黑帧";
        return;
    }

    // 与上一帧完全相同则跳过，避免到底后重复抓帧
    if (!lastValid.isNull() && (lastValid == frame))
    {
        return;
    }

    m_frames.append(frame);
    lastValid = frame;  // 更新上一帧引用（QImage COW，安全）

    int count = static_cast<int>(m_frames.size());
    m_processedFrameCount.store(count);

    // 通过信号通知主线程更新 overlay（QueuedConnection）
    Q_EMIT frameProcessed(count);
}

// =============================================================================
// 停止监听
// =============================================================================
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
