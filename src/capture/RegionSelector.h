/**
 * \file RegionSelector.h
 * \brief 画框截屏选择器
 *
 * 工作流：
 *   1. start() 弹出一个跨屏全屏遮罩窗口（半透明）
 *   2. 用户在遮罩上拖拽鼠标框选目标区域
 *   3. 鼠标释放：emit regionSelected(rect) 并关闭遮罩
 *   4. ESC 或右键：emit cancelled
 *
 * 遮罩支持：
 *   - 鼠标移动时实时显示当前选区大小
 *   - 选区外区域加深遮罩，选区内保持清晰
 *   - Windows11 风格的淡色边框
 */
#pragma once

#include <QWidget>
#include <QRect>
#include <QPoint>

/**
 * @brief 画框截屏选择器
 *
 * 跨屏全屏遮罩窗口，用户拖拽框选目标区域。
 * 选区确定后通过 regionSelected 信号返回全局虚拟屏幕坐标。
 */
class RegionSelector : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父窗口
     */
    explicit RegionSelector(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~RegionSelector() override;

    /**
     * @brief 启动选择器（跨屏全屏显示）
     */
    void start();

Q_SIGNALS:
    /**
     * @brief 选区确定信号
     * @param rect 选中的矩形（虚拟屏幕坐标）
     */
    void regionSelected(const QRect& rect);

    /**
     * @brief 取消信号
     */
    void cancelled();

protected:
    /**
     * @brief 绘制事件：渲染遮罩与选区高亮
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 按键事件：ESC 取消
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief 鼠标按下事件：开始拖拽或右键取消
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标移动事件：更新选区
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标释放事件：确认选区
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /**
     * @brief 设置跨屏全屏几何
     */
    void setupFullScreen();

    /**
     * @brief 绘制选区高亮（挖空遮罩 + 边框）
     * @param painter 画笔
     */
    void drawSelectionHighlight(QPainter& painter);

    /**
     * @brief 绘制尺寸提示标签
     * @param painter 画笔
     */
    void drawSizeTag(QPainter& painter);

private:
    bool     m_dragging  = false;  ///< 是否正在拖拽
    QPoint   m_startPos;          ///< 拖拽起点
    QPoint   m_endPos;            ///< 拖拽当前点
    QRect    m_selection;         ///< 当前选区
};
