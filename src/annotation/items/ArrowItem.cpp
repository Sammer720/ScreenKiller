/**
 * \file ArrowItem.cpp
 * \brief 直线/箭头标注图元实现
 */
#include "ArrowItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QLineF>
#include <QPointF>
#include <cmath>

namespace SK {

namespace {
/// \brief 画笔宽度外扩边距
constexpr qreal G_PEN_MARGIN     = 2.0;
/// \brief 箭头张角的一半（30°，左右各 30°）
constexpr qreal G_ARROW_HALF_RAD = M_PI / 6.0;
}

ArrowItem::ArrowItem(QGraphicsItem* parent)
    : BaseAnnotationItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemSendsGeometryChanges, true);
}

QRectF ArrowItem::boundingRect() const
{
    // 半线宽 + 箭头尺寸 + 边距
    qreal margin = (m_pen.widthF() / 2.0) + m_arrowSize + G_PEN_MARGIN;

    // 取线段两端点的最小/最大坐标作为基础矩形
    QPointF p1 = m_line.p1();
    QPointF p2 = m_line.p2();
    return QRectF(
        QPointF(qMin(p1.x(), p2.x()), qMin(p1.y(), p2.y())),
        QPointF(qMax(p1.x(), p2.x()), qMax(p1.y(), p2.y()))
    ).adjusted(-margin, -margin, margin, margin);
}

void ArrowItem::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(m_pen);
    painter->setBrush(m_brush);

    // 主线段
    painter->drawLine(m_line);

    // 终点箭头
    if (m_drawArrow)
    {
        // 根据线段方向计算箭头三角形两个侧顶点
        qreal angle = std::atan2(m_line.dy(), m_line.dx());
        qreal leftAngle = angle + G_ARROW_HALF_RAD;
        qreal rightAngle = angle - G_ARROW_HALF_RAD;
        QPointF tip   = m_line.p2();
        QPointF left  = tip - QPointF(std::cos(leftAngle) * m_arrowSize,
                                      std::sin(leftAngle) * m_arrowSize);
        QPointF right = tip - QPointF(std::cos(rightAngle) * m_arrowSize,
                                      std::sin(rightAngle) * m_arrowSize);

        QPolygonF arrow;
        arrow << tip << left << right;
        painter->setBrush(m_pen.color());
        painter->drawPolygon(arrow);
    }
}

} // namespace SK
