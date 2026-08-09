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
 * 通过 setRect() 设置外接矩形，paintContent() 内部绘制椭圆。
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
    /// @brief 绘制椭圆内容（由基类 NVI paint() 回调）
    void paintContent(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget = nullptr) override;

    /// @brief 获取缩放参考矩形（即外接矩形本身）
    QRectF resizeRect() const override;
    /// @brief 按新的缩放矩形更新外接矩形
    /// @param newRect 新的外接矩形（局部坐标系）
    void setResizeRect(const QRectF& newRect) override;

    /// @brief 设置外接矩形
    /// @param r 外接矩形（局部坐标系）
    void setRect(const QRectF& r);
    /// @brief 获取外接矩形
    QRectF rect() const;

private:
    QRectF m_rect;   ///< 椭圆外接矩形
};

} // namespace SK
