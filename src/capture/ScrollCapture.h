/**
 * \file ScrollCapture.h
 * \brief 手动滚动截屏（长截图）
 *
 * 工作流：
 *   1. start() 先让用户框选要截屏的目标窗口 / 区域
 *   2. onRegionSelected() 用 WindowFromPoint 探测选区中心所在窗口
 *   3. 安装 MouseWheelHook 全局监听鼠标滚轮，显示 ScrollOverlay 提示浮窗
 *   4. 用户每向下滚动一次滚轮，经过去抖后抓取一帧
 *   5. 用户点击「完成」后，调用 ImageStitcher 进行垂直拼接
 *   6. emit captureFinished(mergedImage)
 */
#pragma once

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QVector>
#include <QRect>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

class RegionSelector;
class ImageStitcher;
class MouseWheelHook;
class ScrollOverlay;

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
     * @brief 设置滚轮去抖时间（毫秒）
     * @param ms 去抖时间
     */
    void setDebounceMs(int ms)      { m_debounceMs = ms; }

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
     * @brief 去抖超时槽：实际执行抓帧
     */
    void onDebounceTimeout();

    /**
     * @brief 用户点击完成
     */
    void onFinishRequested();

    /**
     * @brief 用户点击取消
     */
    void onCancelRequested();

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
        Stitching         ///< 正在拼接
    };

    /**
     * @brief 抓取当前区域的一帧
     */
    void captureFrame();

    /**
     * @brief 停止监听并清理资源
     */
    void stopListening();

private:
    RegionSelector*    m_selector    = nullptr;  ///< 区域选择器
    ImageStitcher*     m_stitcher    = nullptr;  ///< 图像拼接器
    MouseWheelHook*    m_hook        = nullptr;  ///< 鼠标滚轮钩子
    ScrollOverlay*     m_overlay     = nullptr;  ///< 操作提示浮窗
    QTimer             m_debounceTimer;            ///< 滚轮去抖定时器

    QRect              m_targetRect;               ///< 屏幕坐标的截屏区域
    HWND               m_targetHwnd  = nullptr;    ///< 滚动目标窗口句柄

    QVector<QImage>    m_frames;                   ///< 已抓取的帧列表
    int                m_frameCount  = 0;        ///< 已抓取帧数

    int                m_maxFrames   = 60;         ///< 最大帧数
    int                m_debounceMs  = 200;        ///< 滚轮去抖时间（毫秒）
    State              m_state       = State::Idle; ///< 当前状态
};
