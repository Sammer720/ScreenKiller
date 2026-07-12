/**
 * \file ScrollCapture.h
 * \brief 手动滚动截屏（长截图）
 *
 * 工作流：
 *   1. start() 先让用户框选要截屏的目标窗口 / 区域
 *   2. onRegionSelected() 用 WindowFromPoint 探测选区中心所在窗口
 *   3. 安装 MouseWheelHook 全局监听鼠标滚轮，显示 ScrollOverlay 提示浮窗
 *   4. 滚轮事件按 px 阈值累积，达到阈值后抓取一帧（不再使用定时器去抖）
 *   5. 用户点击「完成」后，抓取最后一帧再调用 ImageStitcher 进行垂直拼接
 *   6. emit captureFinished(mergedImage)
 */
#pragma once

#include <QObject>
#include <QImage>
#include <QVector>
#include <QRect>

#include <atomic>
#include <memory>
#include <thread>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

class RegionSelector;
class ImageStitcher;
class MouseWheelHook;
class KeyboardHook;
class ScrollOverlay;
class FrameQueue;
class StitchWorker;

/**
 * @brief 手动滚动截屏器
 *
 * 通过监听用户手动滚动事件驱动抓帧，不再自动模拟滚轮。
 * 所有帧收集完成后拼接为长图。
 */
class ScrollCapture : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit ScrollCapture(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ScrollCapture() override;

    /**
     * @brief 启动滚动截屏流程
     */
    void start();

    /**
     * @brief 设置最大帧数
     * @param n 最大帧数
     */
    void setMaxFrames(int n)        { m_maxFrames = n; }

    /**
     * @brief 设置滚轮触发截屏的像素阈值
     * @param px 累积滚动像素阈值
     */
    void setScrollPxThreshold(int px) { m_scrollPxThreshold = px; }

Q_SIGNALS:
    /**
     * @brief 截屏完成信号
     * @param image 拼接后的长图
     */
    void captureFinished(const QImage& image);

    /**
     * @brief 取消信号
     */
    void cancelled();

    /**
     * @brief 进度信号
     * @param capturedFrames 已抓取帧数
     * @param totalEstimate 预计总帧数
     */
    void progress(int capturedFrames, int totalEstimate);

    /**
     * @brief 帧处理完成信号（内部使用，从工作线程 QueuedConnection 到主线程）
     * @param count 已处理帧数
     */
    void frameProcessed(int count);

private Q_SLOTS:
    /**
     * @brief 选区确定槽
     * @param rect 选中的矩形区域
     */
    void onRegionSelected(const QRect& rect);

    /**
     * @brief 选区取消槽
     */
    void onRegionCancelled();

    /**
     * @brief 滚轮事件槽
     * @param delta 滚动量，向下为负
     * @param pos 滚轮事件发生时的屏幕坐标
     */
    void onWheelScrolled(int delta, const QPoint& pos);

    /**
     * @brief 用户点击完成
     */
    void onFinishRequested();

    /**
     * @brief 用户点击取消
     */
    void onCancelRequested();

    /**
     * @brief 帧处理完成槽（从工作线程 QueuedConnection 更新 overlay）
     * @param count 已处理帧数
     */
    void onFrameProcessed(int count);

    /**
     * @brief 拼接完成槽
     * @param result 拼接结果长图
     */
    void onStitchFinished(const QImage& result);

    /**
     * @brief 拼接取消槽
     */
    void onStitchCancelled();

    /**
     * @brief 完成拼接并发出结果
     */
    void finishStitching();

private:
    /**
     * @brief 当前内部状态
     */
    enum class State
    {
        Idle,             ///< 初始空闲
        SelectingRegion,  ///< 正在框选区域
        Capturing,        ///< 正在监听滚轮抓帧
        Flushing,         ///< 等待帧处理队列排空
        Stitching         ///< 正在工作线程拼接
    };

    /**
     * @brief 抓取当前区域的一帧
     */
    void captureFrame();

    /**
     * @brief 停止监听并清理资源
     */
    void stopListening();

    /**
     * @brief 启动帧处理工作线程
     */
    void startFrameThread();

    /**
     * @brief 停止帧处理工作线程（安全 join）
     */
    void stopFrameThread();

    /**
     * @brief 帧处理工作线程主循环
     */
    void frameThreadLoop();

    /**
     * @brief 处理单帧（在工作线程中调用）
     * @param frame 待处理帧
     * @param lastValid 上一帧有效帧引用（用于重复检测）
     */
    void processOneFrame(QImage& frame, QImage& lastValid);

    /**
     * @brief 进入 Flushing 状态（等队列排空后开始拼接）
     */
    void beginFlushing();

private:
    RegionSelector*    m_selector    = nullptr;  ///< 区域选择器
    ImageStitcher*     m_stitcher    = nullptr;  ///< 图像拼接器（仅供帧处理线程使用）
    MouseWheelHook*    m_hook        = nullptr;  ///< 鼠标滚轮钩子
    KeyboardHook*      m_keyboardHook = nullptr; ///< 键盘钩子（Esc/Enter 拦截）
    ScrollOverlay*     m_overlay     = nullptr;  ///< 操作提示浮窗
    StitchWorker*      m_stitchWorker = nullptr; ///< 拼接工作线程 worker
    QRect              m_targetRect;               ///< 屏幕坐标的截屏区域
    HWND               m_targetHwnd  = nullptr;    ///< 滚动目标窗口句柄

    QVector<QImage>    m_frames;                   ///< 已抓取的帧列表（工作线程独占）
    int                m_frameCount  = 0;        ///< 已抓取帧数

    int                m_maxFrames        = 60;   ///< 最大帧数
    int                m_accumulatedDelta = 0;    ///< 累积滚动像素值
    int                m_scrollPxThreshold = 10;  ///< 触发截屏的滚动像素阈值
    State              m_state            = State::Idle; ///< 当前状态

    // 帧处理流水线
    FrameQueue*        m_frameQueue   = nullptr;  ///< 线程安全帧队列
    std::unique_ptr<std::thread> m_frameThread;   ///< 帧处理工作线程
    std::atomic<bool>  m_frameThreadRunning{false}; ///< 帧处理线程运行标志
    std::atomic<int>   m_processedFrameCount{0}; ///< 工作线程已处理帧数
};
