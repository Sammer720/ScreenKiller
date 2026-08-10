/**
 * \file PenItem.cpp
 * \brief 自由画笔涂鸦图元实现
 */
#include "PenItem.h"

#include <algorithm>

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

void PenItem::paintContent(QPainter* painter,
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

QRectF PenItem::resizeRect() const
{
    // Fail-Fast：无控制点时返回空矩形
    if (m_points.isEmpty())
    {
        return QRectF();
    }

    // 1. 以首点为初始极值
    qreal minX = m_points[0].x();
    qreal maxX = minX;
    qreal minY = m_points[0].y();
    qreal maxY = minY;

    // 2. 遍历全部控制点，累积求得 x / y 方向的极值
    for (const QPointF& point : m_points)
    {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    // 3. 极值之差即为外接矩形尺寸
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void PenItem::setResizeRect(const QRectF& newRect)
{
    // 1. 先取当前外接矩形作为缩放基准
    QRectF oldRect = resizeRect();
    qreal oldWidth  = oldRect.width();
    qreal oldHeight = oldRect.height();

    // 2. 除零保护：任一方向过短说明图元近似退化，无法缩放
    if ((oldWidth < G_MIN_RESIZE_SIZE) || (oldHeight < G_MIN_RESIZE_SIZE))
    {
        return;
    }

    // 3. 计算 x / y 两个方向的缩放比例
    qreal scaleX = newRect.width() / oldWidth;
    qreal scaleY = newRect.height() / oldHeight;

    // 4. 所有控制点相对旧矩形原点偏移后按比例变换，再平移到新矩形原点
    QVector<QPointF> newPoints;
    newPoints.reserve(m_points.size());
    for (const QPointF& point : m_points)
    {
        newPoints.append(QPointF(
            newRect.x() + (point.x() - oldRect.x()) * scaleX,
            newRect.y() + (point.y() - oldRect.y()) * scaleY
        ));
    }
    setPoints(newPoints);
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
