/**
 * \file ArrowItem.cpp
 * \brief 直线/箭头标注图元实现
 */
#include "ArrowItem.h"

#include <algorithm>

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

void ArrowItem::setLine(const QLineF& line)
{
    m_line = line;
    prepareGeometryChange();
    update();
}

QLineF ArrowItem::line() const
{
    return m_line;
}

void ArrowItem::setDrawArrow(bool drawArrow)
{
    m_drawArrow = drawArrow;
    update();
}

bool ArrowItem::drawArrow() const
{
    return m_drawArrow;
}

void ArrowItem::setArrowSize(qreal arrowSize)
{
    m_arrowSize = arrowSize;
    update();
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

void ArrowItem::paintContent(QPainter* painter,
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

QRectF ArrowItem::resizeRect() const
{
    // 1. 取线段两端点坐标
    auto p1 = m_line.p1();
    auto p2 = m_line.p2();

    // 2. 两端点的最小/最大坐标即为外接矩形的两个对角
    return QRectF(
        QPointF(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y())),
        QPointF(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y()))
    );
}

void ArrowItem::setResizeRect(const QRectF& newRect)
{
    // 1. 先取当前外接矩形作为缩放基准
    QRectF oldRect = resizeRect();
    qreal oldWidth  = oldRect.width();
    qreal oldHeight = oldRect.height();

    // 2. 除零保护：任一方向过短时无法计算缩放比例，
    //    退化为直接把线段设为新矩形的对角线
    if ((oldWidth < G_MIN_RESIZE_SIZE) || (oldHeight < G_MIN_RESIZE_SIZE))
    {
        setLine(QLineF(newRect.topLeft(), newRect.bottomRight()));
        return;
    }

    // 3. 计算 x / y 两个方向的缩放比例
    qreal scaleX = newRect.width() / oldWidth;
    qreal scaleY = newRect.height() / oldHeight;

    // 4. 两端点相对旧矩形原点偏移后按比例变换，再平移到新矩形原点
    auto p1 = m_line.p1();
    auto p2 = m_line.p2();
    QPointF newP1(
        newRect.x() + (p1.x() - oldRect.x()) * scaleX,
        newRect.y() + (p1.y() - oldRect.y()) * scaleY
    );
    QPointF newP2(
        newRect.x() + (p2.x() - oldRect.x()) * scaleX,
        newRect.y() + (p2.y() - oldRect.y()) * scaleY
    );
    setLine(QLineF(newP1, newP2));
}

} // namespace SK
