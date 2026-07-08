/**
 * \file RectangleItem.h
 * \brief 矩形标注图元
 */
#pragma once

#include "BaseAnnotationItem.h"

namespace SK {

/**
 * @brief 矩形标注图元
 *
 * 通过 setRect() 设置矩形几何，paint() 内部使用 m_pen/m_brush 绘制。
 */
class RectangleItem : public BaseAnnotationItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit RectangleItem(QGraphicsItem* parent = nullptr);

    /// @brief 返回包围盒（含画笔宽度边距）
    QRectF boundingRect() const override;
    /// @brief 绘制矩形
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    /// @brief 设置矩形几何
    /// @param r 矩形坐标（局部坐标系）
    void setRect(const QRectF& r) { m_rect = r; prepareGeometryChange(); update(); }
    /// @brief 获取矩形几何
    QRectF rect() const { return m_rect; }

private:
    QRectF m_rect;   ///< 矩形几何
};

} // namespace SK
