/**
 * \file RectangleItem.cpp
 * \brief 矩形标注图元实现
 */
#include "RectangleItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace SK {

namespace {
/// \brief 画笔宽度外扩边距（避免边缘被裁剪）
constexpr qreal G_PEN_MARGIN = 2.0;
}

RectangleItem::RectangleItem(QGraphicsItem* parent)
    : BaseAnnotationItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemSendsGeometryChanges, true);
}

void RectangleItem::setRect(const QRectF& rect)
{
    m_rect = rect;
    prepareGeometryChange();
    update();
}

QRectF RectangleItem::rect() const
{
    return m_rect;
}

QRectF RectangleItem::boundingRect() const
{
    // 半线宽 + 边距，确保边框不被裁剪
    qreal margin = (m_pen.widthF() / 2.0) + G_PEN_MARGIN;
    return m_rect.adjusted(-margin, -margin, margin, margin);
}

void RectangleItem::paintContent(QPainter* painter,
                                 const QStyleOptionGraphicsItem* option,
                                 QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);

    // 折角圆角化：半径取半线宽（随线宽自适应），
    // 消除直角 Miter 尖刺/斜切造成的转角缺块，并使折角圆润
    QPen pen = m_pen;
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(m_brush);
    const qreal radius = qMax(2.0, pen.widthF() * 0.5);
    painter->drawRoundedRect(m_rect, radius, radius);
}

QRectF RectangleItem::resizeRect() const
{
    return m_rect;
}

void RectangleItem::setResizeRect(const QRectF& newRect)
{
    setRect(newRect);
}

} // namespace SK
