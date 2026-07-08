/**
 * \file EllipseItem.cpp
 * \brief 椭圆标注图元实现
 */
#include "EllipseItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace SK {

namespace {
/// \brief 画笔宽度外扩边距（避免边缘被裁剪）
constexpr qreal G_PEN_MARGIN = 2.0;
}

EllipseItem::EllipseItem(QGraphicsItem* parent)
    : BaseAnnotationItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemSendsGeometryChanges, true);
}

QRectF EllipseItem::boundingRect() const
{
    // 半线宽 + 边距，确保边框不被裁剪
    qreal margin = (m_pen.widthF() / 2.0) + G_PEN_MARGIN;
    return m_rect.adjusted(-margin, -margin, margin, margin);
}

void EllipseItem::paint(QPainter* painter,
                        const QStyleOptionGraphicsItem* option,
                        QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(m_pen);
    painter->setBrush(m_brush);
    painter->drawEllipse(m_rect);
}

} // namespace SK
