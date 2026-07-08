/**
 * \file EllipseItem.h
 * \brief 椭圆标注图元
 */
#pragma once

#include "BaseAnnotationItem.h"

namespace SK {

/**
 * @brief 椭圆标注图元
 *
 * 通过 setRect() 设置外接矩形，paint() 内部绘制椭圆。
 */
class EllipseItem : public BaseAnnotationItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit EllipseItem(QGraphicsItem* parent = nullptr);

    /// @brief 返回包围盒（含画笔宽度边距）
    QRectF boundingRect() const override;
    /// @brief 绘制椭圆
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    /// @brief 设置外接矩形
    /// @param r 外接矩形（局部坐标系）
    void setRect(const QRectF& r) { m_rect = r; prepareGeometryChange(); update(); }
    /// @brief 获取外接矩形
    QRectF rect() const { return m_rect; }

private:
    QRectF m_rect;   ///< 椭圆外接矩形
};

} // namespace SK
