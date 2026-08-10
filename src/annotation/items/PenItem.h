/**
 * \file PenItem.h
 * \brief 自由画笔涂鸦图元
 *
 * 内部维护一个路径点序列，paint 时用 QPainterPath 连接。
 */
#pragma once

#include "BaseAnnotationItem.h"

#include <QVector>
#include <QPointF>
#include <QPainterPath>

namespace SK {

/**
 * @brief 自由画笔涂鸦图元
 *
 * 调用 appendPoint() 增量追加控制点；
 * 每次追加会自动重建 QPainterPath 并触发重绘。
 */
class PenItem : public BaseAnnotationItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit PenItem(QGraphicsItem* parent = nullptr);

    /// @brief 返回包围盒（含画笔宽度边距）
    QRectF boundingRect() const override;
    /// @brief 绘制路径（NVI 内容绘制入口）
    void paintContent(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget = nullptr) override;
    /// @brief 返回所有控制点的外接矩形，作为缩放手柄拖拽的基准
    QRectF resizeRect() const override;
    /// @brief 按比例变换所有控制点以适配新的外接矩形
    /// @param newRect 目标外接矩形（由手柄拖拽产生）
    void setResizeRect(const QRectF& newRect) override;

    /// @brief 追加一个控制点
    /// @param pt 局部坐标系下的点
    void appendPoint(const QPointF& pt);
    /// @brief 批量设置控制点
    /// @param pts 点序列
    void setPoints(const QVector<QPointF>& pts);
    /// @brief 获取所有控制点
    QVector<QPointF> points() const { return m_points; }

protected:
    /// @brief 监听位置变化，便于上层联动
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    /// @brief 重建内部 QPainterPath
    void rebuildPath();

    QVector<QPointF> m_points;   ///< 控制点序列
    QPainterPath     m_path;     ///< 由 m_points 重建的可绘制路径
};

} // namespace SK
