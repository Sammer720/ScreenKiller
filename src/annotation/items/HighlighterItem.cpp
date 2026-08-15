/**
 * \file HighlighterItem.cpp
 * \brief 荧光笔图元实现
 */
#include "HighlighterItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QColor>

namespace SK {

namespace {
/// \brief 默认荧光黄色 RGB 红色分量
constexpr int G_HIGHLIGHT_R = 255;
/// \brief 默认荧光黄色 RGB 绿色分量
constexpr int G_HIGHLIGHT_G = 235;
/// \brief 默认荧光黄色 RGB 蓝色分量
constexpr int G_HIGHLIGHT_B = 59;
/// \brief 默认荧光笔线宽（与工具栏荧光笔默认 25px 一致，创建后通常被场景参数覆盖）
constexpr qreal G_HIGHLIGHT_WIDTH = 25.0;
/// \brief 至少需要的控制点数才能绘制（需 ≥2 点）
constexpr int G_MIN_POINTS_TO_DRAW = 2;
}

HighlighterItem::HighlighterItem(QGraphicsItem* parent)
    : PenItem(parent)
{
    // 设置默认荧光黄
    QColor highlightColor(G_HIGHLIGHT_R, G_HIGHLIGHT_G, G_HIGHLIGHT_B, m_alpha);
    setPenColor(highlightColor);
    setPenWidth(G_HIGHLIGHT_WIDTH);
}

void HighlighterItem::setAlpha(int alpha)
{
    m_alpha = alpha;
    update();
}

int HighlighterItem::alpha() const
{
    return m_alpha;
}

void HighlighterItem::paintContent(QPainter* painter,
                                   const QStyleOptionGraphicsItem* option,
                                   QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, false);

    // 荧光笔效果：半透明叠加
    painter->setCompositionMode(QPainter::CompositionMode_Darken);

    QColor currentColor = penColor();
    currentColor.setAlpha(m_alpha);

    QPen pen = painter->pen();
    pen.setColor(currentColor);
    pen.setWidthF(penWidth());
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::BevelJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    // 复用父类的点序列手动重建路径并绘制
    // （父类 paintContent 已被覆盖，这里手动重画）
    QVector<QPointF> pointList = points();
    if (pointList.size() < G_MIN_POINTS_TO_DRAW)
    {
        return;
    }

    QPainterPath path;
    path.moveTo(pointList.first());
    for (int i = 1; i < pointList.size(); ++i)
    {
        path.lineTo(pointList[i]);
    }
    painter->drawPath(path);
}

} // namespace SK
