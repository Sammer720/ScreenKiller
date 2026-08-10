/**
 * \file ArrowItem.h
 * \brief 直线 / 箭头标注图元
 *
 * 通过 setDrawArrow(true) 切换是否在终点绘制箭头三角形。
 */
#pragma once

#include "BaseAnnotationItem.h"

namespace SK {

/**
 * @brief 直线/箭头标注图元
 *
 * 终点箭头通过 paintContent() 中根据 m_line 方向计算三角形顶点实现，
 * 箭头尺寸由 setArrowSize() 控制。
 */
class ArrowItem : public BaseAnnotationItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit ArrowItem(QGraphicsItem* parent = nullptr);

    /// @brief 返回包围盒（含画笔宽度与箭头尺寸边距）
    QRectF boundingRect() const override;
    /// @brief 绘制直线与可选箭头（NVI 内容绘制入口）
    void paintContent(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget = nullptr) override;
    /// @brief 返回线段外接矩形，作为缩放手柄拖拽的基准
    QRectF resizeRect() const override;
    /// @brief 按比例变换线段两端点以适配新的外接矩形
    /// @param newRect 目标外接矩形（由手柄拖拽产生）
    void setResizeRect(const QRectF& newRect) override;

    /// @brief 设置线段
    /// @param line 线段几何
    void setLine(const QLineF& line);
    /// @brief 获取线段
    QLineF line() const;

    /// @brief 设置是否绘制箭头
    /// @param b true=绘制箭头，false=仅直线
    void setDrawArrow(bool b);
    /// @brief 是否绘制箭头
    bool drawArrow() const;

    /// @brief 设置箭头尺寸（像素）
    /// @param s 箭头三角形边长
    void setArrowSize(qreal s);

private:
    QLineF m_line;              ///< 线段几何
    bool   m_drawArrow = true; ///< 是否绘制终点箭头
    qreal  m_arrowSize = 12.0; ///< 箭头尺寸（像素）
};

} // namespace SK
