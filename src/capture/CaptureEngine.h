/**
 * \file CaptureEngine.h
 * \brief 截屏引擎：统一调度四种截屏模式
 *
 * 模式枚举与 MainWindow::CaptureMode 一一对应。
 *
 * 工作流：
 *   1. start(Mode) 根据 Mode 创建相应的 Selector
 *   2. Selector 完成选择后 emit regionSelected(rect, hwnd)
 *   3. CaptureEngine 调用 QScreen::grabWindow 抓取
 *   4. emit captureFinished(QImage) 或 captureCancelled()
 *
 * 滚动模式特殊：调用 ScrollCapture 进行多帧抓取 + 拼接。
 */
#pragma once

#include <QImage>
#include <QObject>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

class RegionSelector;
class WindowSelector;
class ScrollCapture;

/**
 * @brief 截屏引擎
 *
 * 统一调度画框、全屏、窗口、滚动四种截屏模式。
 * 各模式对应的选择器延迟创建，首次使用时才实例化。
 */
class CaptureEngine : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 截屏模式枚举
     */
    enum class Mode
    {
        Region = 0,     ///< 画框截屏
        FullScreen = 1,///< 全屏截屏
        Window = 2,    ///< 窗口截屏
        Scrolling = 3  ///< 滚动截屏
    };
    Q_ENUM(Mode)

    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit CaptureEngine(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~CaptureEngine() override;

    /**
     * @brief 启动一次截屏（异步，结果通过信号返回）
     * @param mode 截屏模式
     */
    void start(Mode mode);

Q_SIGNALS:
    /**
     * @brief 截屏完成信号
     * @param image 截取的图像
     */
    void captureFinished(const QImage& image);

    /**
     * @brief 截屏取消信号
     */
    void captureCancelled();

private Q_SLOTS:
    /**
     * @brief 画框截屏选择完成槽
     * @param rect 选中的矩形区域
     */
    void onRegionSelected(const QRect& rect);

    /**
     * @brief 窗口选择完成槽
     * @param hwnd 目标窗口句柄
     */
    void onWindowSelected(HWND hwnd);

    /**
     * @brief 滚动截屏完成槽
     * @param image 拼接后的长图
     */
    void onScrollingFinished(const QImage& image);

private:
    /**
     * @brief 抓取全屏图像
     * @return 全屏图像
     */
    QImage grabFullScreen();

    /**
     * @brief 抓取指定区域图像
     * @param rect 目标区域（设备无关像素）
     * @return 区域图像
     */
    QImage grabRegion(const QRect& rect);

    /**
     * @brief 抓取指定窗口图像
     * @param hwnd 窗口句柄
     * @return 窗口图像
     */
    QImage grabWindow(HWND hwnd);

private:
    RegionSelector*   m_regionSelector   = nullptr;  ///< 画框选择器
    WindowSelector*   m_windowSelector   = nullptr;  ///< 窗口选择器
    ScrollCapture*    m_scrollCapture    = nullptr;  ///< 滚动截屏器
};
