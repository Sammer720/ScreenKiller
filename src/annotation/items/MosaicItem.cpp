/**
 * \file MosaicItem.cpp
 * \brief 马赛克涂抹图元实现
 *
 * 核心思路：以画笔轨迹按线宽外扩形成的笔画区域为裁剪范围，
 * 逐块对背景图做 1x1 平均色采样并回填，实现像素化效果。
 * 背景图位于场景坐标 (0,0)，局部坐标需叠加 pos() 转换到场景坐标。
 */
#include "MosaicItem.h"
#include "AnnotationScene.h"

#include <QVector>
#include <QPointF>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QColor>
#include <QRectF>

namespace SK {

namespace {
/// \brief 至少需要的控制点数才能绘制（需 ≥2 点）
constexpr int G_MIN_POINTS_TO_DRAW = 2;
}

MosaicItem::MosaicItem(QGraphicsItem* parent)
    : PenItem(parent)
{
    // 可选中 / 可移动 / 几何变化通知等 flag 已由 PenItem 构造函数统一设置，
    // 马赛克图元无需额外配置。
}

void MosaicItem::paintContent(QPainter* painter,
                              const QStyleOptionGraphicsItem* option,
                              QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 1. 防御性判空：场景必须是 AnnotationScene 且背景图已加载，否则静默返回
    auto* annotationScene = dynamic_cast<AnnotationScene*>(scene());
    if (annotationScene == nullptr)
    {
        return;
    }
    const QImage& bgImage = annotationScene->backgroundImage();
    if (bgImage.isNull())
    {
        return;
    }

    // 2. 获取点序列并手动重建路径（点数不足无法形成涂抹区域，直接返回）
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

    // 3. 以画笔粗细为宽度、圆头端点为形状，外扩生成涂抹区域
    QPainterPathStroker stroker;
    stroker.setWidth(penWidth());
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath strokePath = stroker.createStroke(path);

    // 4. 将绘制范围裁剪到涂抹区域，避免像素化色块溢出笔画边界
    painter->save();
    painter->setClipPath(strokePath);

    // 5. 逐块采样背景平均色并回填，实现马赛克像素化
    const QPointF itemPos = pos();
    const QRectF strokeBounds = strokePath.boundingRect();
    const QRectF bgImageRect(bgImage.rect());
    // 块大小跟随笔刷宽度：拖动尺寸滑块时马赛克块粒度同步变化，
    // 让"尺寸"参数的实际效果直观可见（仅固定 8px 时几乎无感）
    const qreal blockSize = qMax(4.0, penWidth());

    for (qreal gridY = strokeBounds.top(); gridY <= strokeBounds.bottom(); gridY += blockSize)
    {
        for (qreal gridX = strokeBounds.left(); gridX <= strokeBounds.right(); gridX += blockSize)
        {
            // 5.1 局部坐标块 → 场景坐标块（背景图位于场景坐标 (0,0)）
            QRectF sceneBlock(gridX + itemPos.x(), gridY + itemPos.y(),
                              blockSize, blockSize);

            // 5.2 与背景图边界求交，防止越界导致 QImage::copy 返回空图
            QRectF clampedSceneRect = sceneBlock.intersected(bgImageRect);
            if (clampedSceneRect.isEmpty())
            {
                continue;
            }

            // 5.3 裁出该块并缩放到 1x1 得到平均色
            QImage sampledImage = bgImage.copy(clampedSceneRect.toRect());
            if (sampledImage.isNull())
            {
                continue;
            }
            QImage avgImage = sampledImage.scaled(1, 1, Qt::IgnoreAspectRatio,
                                                  Qt::SmoothTransformation);
            if (avgImage.isNull())
            {
                continue;
            }
            QColor avgColor(avgImage.pixel(0, 0));

            // 5.4 在局部坐标填充该块（超出涂抹区域的部分由裁剪路径兜底）
            QRectF localBlock(gridX, gridY, blockSize, blockSize);
            painter->fillRect(localBlock, avgColor);
        }
    }

    // 6. 恢复裁剪状态
    painter->restore();
}

} // namespace SK