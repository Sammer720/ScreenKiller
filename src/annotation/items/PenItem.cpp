/**
 * \file PenItem.cpp
 * \brief 自由画笔涂鸦图元实现
 */
#include "PenItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace SK {

namespace {
/// \brief 画笔宽度外扩边距
constexpr qreal G_PEN_MARGIN = 2.0;
}

PenItem::PenItem(QGraphicsItem* parent)
    : BaseAnnotationItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemSendsGeometryChanges, true);

    // 圆角端点 / 圆角连接，使涂鸦线条更平滑
    m_pen.setCapStyle(Qt::RoundCap);
    m_pen.setJoinStyle(Qt::RoundJoin);
}

void PenItem::appendPoint(const QPointF& pt)
{
    m_points.append(pt);
    rebuildPath();
    prepareGeometryChange();
    update();
}

void PenItem::setPoints(const QVector<QPointF>& pts)
{
    m_points = pts;
    rebuildPath();
    prepareGeometryChange();
    update();
}

void PenItem::rebuildPath()
{
    m_path = QPainterPath();

    // Fail-Fast：无点直接返回空路径
    if (m_points.isEmpty())
    {
        return;
    }

    m_path.moveTo(m_points.first());
    for (int i = 1; i < m_points.size(); ++i)
    {
        m_path.lineTo(m_points[i]);
    }
}

QRectF PenItem::boundingRect() const
{
    // Fail-Fast：无点返回空矩形
    if (m_points.isEmpty())
    {
        return QRectF();
    }
    qreal margin = (m_pen.widthF() / 2.0) + G_PEN_MARGIN;
    QRectF pathRect = m_path.boundingRect();
    return pathRect.adjusted(-margin, -margin, margin, margin);
}

void PenItem::paint(QPainter* painter,
                    const QStyleOptionGraphicsItem* option,
                    QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(m_pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);
}

QVariant PenItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged)
    {
        // 移动后位置已通过 pos() 体现，无需重建路径
    }
    return BaseAnnotationItem::itemChange(change, value);
}

} // namespace SK
