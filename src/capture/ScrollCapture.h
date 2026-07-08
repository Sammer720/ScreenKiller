/**
 * \file ScrollCapture.h
 * \brief 滚动截屏（长截图）
 *
 * 工作流：
 *   1. start() 先让用户框选要截屏的目标窗口 / 区域
 *   2. 进入滚动循环：
 *      a. grabWindow 抓取当前帧 -> frames 列表
 *      b. SendInput 发送鼠标滚轮事件，让目标窗口向下滚动 N 行
 *      c. 等待滚动动画稳定（默认 250ms）
 *      d. 检测是否到达底部（连续两帧无变化 / 滚动位置未变）
 *   3. 退出循环后，调用 ImageStitcher 进行垂直拼接
 *   4. emit captureFinished(mergedImage)
 *
 * 关键参数：
 *   - 滚动步长（鼠标滚轮的 delta，默认 3 行）
 *   - 帧间隔（等待稳定的时间）
 *   - 最大帧数（避免无限滚动）
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

/**
 * @brief 滚动截屏器
 *
 * 通过循环抓取屏幕帧并拼接，实现长截图功能。
 * 滚动通过模拟鼠标滚轮事件驱动目标窗口滚动。
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
     * @brief 设置滚动步长（行数）
     * @param lines 滚动行数
     */
    void setScrollStep(int lines)   { m_scrollLines = lines; }

    /**
     * @brief 设置帧间隔（毫秒）
     * @param ms 间隔时间
     */
    void setFrameInterval(int ms)   { m_frameIntervalMs = ms; }

    /**
     * @brief 设置最大帧数
     * @param n 最大帧数
     */
    void setMaxFrames(int n)        { m_maxFrames = n; }

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
     * @brief 抓取下一帧
     */
    void captureNextFrame();

    /**
     * @brief 完成拼接并发出结果
     */
    void finishStitching();

private:
    /**
     * @brief 向目标窗口发送滚轮事件
     */
    void scrollTarget();

    /**
     * @brief 检测滚动是否停滞
     * @return 停滞返回 true（当前实现已在 captureNextFrame 中通过帧比对处理）
     */
    bool isScrollStuck();

private:
    RegionSelector*  m_selector    = nullptr;  ///< 区域选择器
    ImageStitcher*   m_stitcher    = nullptr;  ///< 图像拼接器
    QTimer           m_timer;                  ///< 帧间隔定时器

    QRect            m_targetRect;             ///< 屏幕坐标的截屏区域
    HWND             m_targetHwnd  = nullptr;  ///< 滚动目标窗口句柄

    QVector<QImage>  m_frames;                 ///< 已抓取的帧列表
    QImage           m_lastFrame;              ///< 上一帧（用于停滞检测）
    int              m_frameCount  = 0;       ///< 已抓取帧数

    int              m_scrollLines    = 3;     ///< 滚动步长（行数）
    int              m_frameIntervalMs = 250; ///< 帧间隔（毫秒）
    int              m_maxFrames      = 60;    ///< 最大帧数
};
