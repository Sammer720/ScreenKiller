/**
 * \file BaseAnnotationItem.cpp
 * \brief 标注图元基类实现（NVI paint + 手柄绘制 + 缩放交互）
 *
 * 本文件承载 NVI 模式下基类的统一职责：
 *   1. final paint()：先调用子类 paintContent() 绘制主体内容，选中时再叠加四角缩放手柄；
 *   2. 手柄绘制与命中测试：drawHandles() / handleAt() 均基于子类的 resizeRect() 定位；
 *   3. 缩放交互：鼠标按下命中手柄后进入缩放待命，拖拽时实时更新几何，
 *      释放时提交 ResizeItemCommand 到撤销栈实现可撤销缩放。
 */
#include "BaseAnnotationItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>

#include <algorithm>
#include <memory>

#include "AnnotationScene.h"
#include "UndoStack.h"

namespace {

/// \brief 手柄方块半边长（像素，item 坐标系）
constexpr qreal G_HANDLE_HALF_SIZE = 4.0;

/// \brief 手柄方块边长（像素，item 坐标系）
constexpr qreal G_HANDLE_SIZE = G_HANDLE_HALF_SIZE * 2;

/// \brief 手柄填充色（白色，与 Windows 11 浅色风格一致）
const QColor G_HANDLE_FILL_COLOR(255, 255, 255);

/// \brief 手柄边框色（Windows 强调蓝）
const QColor G_HANDLE_BORDER_COLOR(0, 120, 215);

/// \brief 手柄边框线宽（像素）
constexpr qreal G_HANDLE_BORDER_WIDTH = 1.0;

} // namespace

namespace SK {

BaseAnnotationItem::BaseAnnotationItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
}

void BaseAnnotationItem::setPenColor(const QColor& color)
{
    m_pen.setColor(color);
    update();
}

void BaseAnnotationItem::setPenWidth(qreal width)
{
    m_pen.setWidthF(width);
    update();
}

void BaseAnnotationItem::setPenStyle(Qt::PenStyle style)
{
    m_pen.setStyle(style);
    update();
}

void BaseAnnotationItem::setBrushColor(const QColor& color)
{
    m_brush.setColor(color);
    update();
}

void BaseAnnotationItem::setBrushStyle(Qt::BrushStyle style)
{
    m_brush.setStyle(style);
    update();
}

QColor BaseAnnotationItem::penColor() const
{
    return m_pen.color();
}

qreal BaseAnnotationItem::penWidth() const
{
    return m_pen.widthF();
}

Qt::PenStyle BaseAnnotationItem::penStyle() const
{
    return m_pen.style();
}

QColor BaseAnnotationItem::brushColor() const
{
    return m_brush.color();
}

Qt::BrushStyle BaseAnnotationItem::brushStyle() const
{
    return m_brush.style();
}

void BaseAnnotationItem::paint(QPainter* painter,
                               const QStyleOptionGraphicsItem* option,
                               QWidget* widget)
{
    // 1. 先让子类绘制图元主体内容（NVI 核心：内容绘制完全交给子类）
    paintContent(painter, option, widget);

    // 2. 仅单选时叠加四角缩放手柄（多选时不显示，避免多个图元的手柄重叠干扰）
    if (isSelected() && (scene() != nullptr)
        && (scene()->selectedItems().size() == 1))
    {
        drawHandles(painter);
    }
}

void BaseAnnotationItem::drawHandles(QPainter* painter) const
{
    QRectF resizeArea = resizeRect();

    // 收集四角手柄的绘制中心点（取自 resizeRect 的四个顶点）
    QPointF corners[4] = {
        resizeArea.topLeft(), resizeArea.topRight(),
        resizeArea.bottomLeft(), resizeArea.bottomRight()
    };

    painter->save();
    painter->setBrush(G_HANDLE_FILL_COLOR);
    painter->setPen(QPen(G_HANDLE_BORDER_COLOR, G_HANDLE_BORDER_WIDTH));
    for (const QPointF& corner : corners)
    {
        // 以角点为中心绘制正方形手柄方块
        painter->drawRect(QRectF(corner.x() - G_HANDLE_HALF_SIZE,
                                 corner.y() - G_HANDLE_HALF_SIZE,
                                 G_HANDLE_SIZE,
                                 G_HANDLE_SIZE));
    }
    painter->restore();
}

ResizeHandle BaseAnnotationItem::handleAt(const QPointF& pos) const
{
    QRectF resizeArea = resizeRect();

    // 依次检测四个角手柄的命中区域（以角点为中心、边长为 G_HANDLE_SIZE 的正方形）
    if (QRectF(resizeArea.topLeft().x() - G_HANDLE_HALF_SIZE,
               resizeArea.topLeft().y() - G_HANDLE_HALF_SIZE,
               G_HANDLE_SIZE, G_HANDLE_SIZE).contains(pos))
    {
        return ResizeHandle::TopLeft;
    }
    if (QRectF(resizeArea.topRight().x() - G_HANDLE_HALF_SIZE,
               resizeArea.topRight().y() - G_HANDLE_HALF_SIZE,
               G_HANDLE_SIZE, G_HANDLE_SIZE).contains(pos))
    {
        return ResizeHandle::TopRight;
    }
    if (QRectF(resizeArea.bottomLeft().x() - G_HANDLE_HALF_SIZE,
               resizeArea.bottomLeft().y() - G_HANDLE_HALF_SIZE,
               G_HANDLE_SIZE, G_HANDLE_SIZE).contains(pos))
    {
        return ResizeHandle::BottomLeft;
    }
    if (QRectF(resizeArea.bottomRight().x() - G_HANDLE_HALF_SIZE,
               resizeArea.bottomRight().y() - G_HANDLE_HALF_SIZE,
               G_HANDLE_SIZE, G_HANDLE_SIZE).contains(pos))
    {
        return ResizeHandle::BottomRight;
    }
    return ResizeHandle::None;
}

// itemChange：选中态重绘手柄 + 位置 clamp 到场景边界（截屏范围）
QVariant BaseAnnotationItem::itemChange(GraphicsItemChange change,
                                        const QVariant& value)
{
    // 选中态变化时重绘以显示/隐藏手柄
    if (change == ItemSelectedHasChanged)
    {
        update();
    }

    // 位置即将改变时 clamp 到场景边界（截屏范围），防止图元移出图片
    // 注意用 ItemPositionChange 而非 ItemPositionHasChanged：
    // 前者是位置即将改变、修改返回值生效；后者是事后通知、修改不生效
    if (change == ItemPositionChange)
    {
        QGraphicsScene* ownerScene = scene();
        if (ownerScene != nullptr)
        {
            const QRectF sceneBounds = ownerScene->sceneRect();
            if (!sceneBounds.isNull())
            {
                QPointF newPos = value.toPointF();
                QRectF itemBounds = boundingRect();
                // 计算允许的位置范围：图元外接矩形必须整体落在场景矩形内
                qreal minX = sceneBounds.left() - itemBounds.left();
                qreal maxX = sceneBounds.right() - itemBounds.right();
                qreal minY = sceneBounds.top() - itemBounds.top();
                qreal maxY = sceneBounds.bottom() - itemBounds.bottom();
                // 图元比场景还大时该轴不做约束（std::clamp 在 lo > hi 时是未定义行为）
                if (minX <= maxX)
                {
                    newPos.setX(std::clamp(newPos.x(), minX, maxX));
                }
                if (minY <= maxY)
                {
                    newPos.setY(std::clamp(newPos.y(), minY, maxY));
                }
                return newPos;
            }
        }
        // scene() 为 null（beginCreateItem 中 setPos 在 addItem 之前）
        // 或场景矩形为空（未加载图片）时不做约束，返回原值
    }

    return QGraphicsItem::itemChange(change, value);
}

void BaseAnnotationItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    // 左键按下且本图元处于"单选选中态"时，才做手柄命中测试
    // （多选时不显示手柄，也不响应手柄命中，避免不可见手柄被误命中进入缩放）
    if ((event->button() == Qt::LeftButton) && isSelected()
        && (scene() != nullptr)
        && (scene()->selectedItems().size() == 1))
    {
        ResizeHandle handle = handleAt(event->pos());
        if (handle != ResizeHandle::None)
        {
            // 命中手柄：快照当前缩放基准，进入缩放待命状态
            // （m_startResizeRect 在 mouseReleaseEvent 中用于判定是否发生实际缩放）
            m_activeHandle = handle;
            m_startResizeRect = resizeRect();
            m_startPos = event->pos();
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void BaseAnnotationItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    // 缩放待命状态：根据鼠标位移实时更新 resizeRect
    if (m_activeHandle != ResizeHandle::None)
    {
        // 1. 计算鼠标相对缩放起点的位移
        QPointF delta = event->pos() - m_startPos;
        QRectF newRect = m_startResizeRect;

        // 2. 按手柄类型移动对应角点（对角固定，另一角跟随鼠标）
        switch (m_activeHandle)
        {
        case ResizeHandle::TopLeft:
            newRect.setTopLeft(m_startResizeRect.bottomRight() + delta);
            break;
        case ResizeHandle::TopRight:
            newRect.setTopRight(m_startResizeRect.bottomLeft() + delta);
            break;
        case ResizeHandle::BottomLeft:
            newRect.setBottomLeft(m_startResizeRect.topRight() + delta);
            break;
        case ResizeHandle::BottomRight:
            newRect.setBottomRight(m_startResizeRect.topLeft() + delta);
            break;
        default:
            break;
        }
        // 3. 归一化：拖拽越过对角时自动翻转矩形方向，保持 left<=right、top<=bottom
        newRect = newRect.normalized();

        // 4. 最小尺寸保护：防止缩放到零/负尺寸（子类 setResizeRect 也有保护，此处兜底）
        if (newRect.width() < G_MIN_RESIZE_SIZE)
        {
            newRect.setWidth(G_MIN_RESIZE_SIZE);
        }
        if (newRect.height() < G_MIN_RESIZE_SIZE)
        {
            newRect.setHeight(G_MIN_RESIZE_SIZE);
        }

        // 5. 实时应用新几何（子类负责 prepareGeometryChange + update）
        setResizeRect(newRect);
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void BaseAnnotationItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    // 缩放待命状态：拖拽结束，提交撤销命令并退出缩放
    if (m_activeHandle != ResizeHandle::None)
    {
        // 1. 取最终几何，与起始快照不同才需要记录（原地按下释放不产生命令）
        QRectF newRect = resizeRect();
        if (newRect != m_startResizeRect)
        {
            // 2. 提交缩放命令到撤销栈（push 会自动执行 redo 覆盖为最终态，幂等）
            auto* annScene = dynamic_cast<AnnotationScene*>(scene());
            if ((annScene != nullptr) && (annScene->undoStack() != nullptr))
            {
                annScene->undoStack()->push(
                    std::make_unique<ResizeItemCommand>(
                        this, m_startResizeRect, newRect));
            }
        }
        // 3. 复位缩放待命状态，恢复正常鼠标处理
        m_activeHandle = ResizeHandle::None;
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

} // namespace SK
