/**
 * \file WindowSelector.h
 * \brief 窗口选择器
 *
 * 工作流：
 *   1. start() 弹出跨屏遮罩
 *   2. 鼠标移动时通过 WindowFromPoint 找到下方窗口
 *   3. 用 GetWindowRect 绘制高亮边框
 *   4. 鼠标左键点击：emit windowSelected(hwnd)
 *   5. ESC 或右键：emit cancelled
 */
#pragma once

#include <QWidget>
#include <QRect>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

/**
 * @brief 窗口选择器
 *
 * 跨屏全屏遮罩，鼠标移动时高亮下方窗口，点击确认选择。
 */
class WindowSelector : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父窗口（建议传 nullptr，避免随父窗口隐藏）
     */
    explicit WindowSelector(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~WindowSelector() override;

    /**
     * @brief 启动选择器
     */
    void start();

Q_SIGNALS:
    /**
     * @brief 窗口选定信号
     * @param hwnd 目标窗口句柄
     */
    void windowSelected(HWND hwnd);

    /**
     * @brief 取消信号
     */
    void cancelled();

protected:
    /**
     * @brief 绘制事件：渲染遮罩与窗口高亮
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 按键事件：ESC 取消
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief 鼠标按下事件：确认或取消选择
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标移动事件：更新高亮窗口
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    /**
     * @brief 设置跨屏全屏几何
     */
    void setupFullScreen();

    /**
     * @brief 根据屏幕坐标查找窗口句柄
     * @param pt 屏幕坐标
     * @return 窗口句柄（找不到时返回 nullptr）
     */
    HWND hwndFromPoint(const QPoint& pt);

    /**
     * @brief 获取窗口用于绘制的矩形（DWM 真实边界）
     * @param hwnd 窗口句柄
     * @return 窗口矩形
     */
    QRect windowRectForPaint(HWND hwnd);

    /**
     * @brief 绘制窗口高亮（挖空遮罩 + 边框）
     * @param painter 画笔
     */
    void drawWindowHighlight(QPainter& painter);

    /**
     * @brief 绘制窗口标题标签
     * @param painter 画笔
     */
    void drawWindowTitleTag(QPainter& painter);

private:
    HWND  m_currentHwnd = nullptr;  ///< 当前高亮窗口句柄
    QRect m_currentRect;             ///< 当前高亮窗口矩形（本 widget 坐标）
};
