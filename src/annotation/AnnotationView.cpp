/**
 * \file AnnotationView.cpp
 * \brief 标注画布视图实现
 */
#include "AnnotationView.h"

#include "AnnotationScene.h"
#include "UndoStack.h"

#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>

namespace {

/// \brief 缩放因子（每次滚轮缩放比例）
constexpr qreal G_ZOOM_FACTOR = 1.15;
/// \brief 视图背景色（浅灰蓝）
const int G_BG_R = 245;
const int G_BG_G = 247;
const int G_BG_B = 250;

} // namespace

AnnotationView::AnnotationView(SK::AnnotationScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent), m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform
                   | QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QColor(G_BG_R, G_BG_G, G_BG_B));
}

void AnnotationView::fitToView()
{
    // 场景无效时不进行适配
    if (m_scene == nullptr)
    {
        return;
    }
    QRectF sceneRect = m_scene->sceneRect();
    // 场景矩形为空时不进行适配
    if (sceneRect.isNull())
    {
        return;
    }
    fitInView(sceneRect, Qt::KeepAspectRatio);
}

void AnnotationView::wheelEvent(QWheelEvent* event)
{
    // Ctrl + 滚轮 = 缩放
    if ((event->modifiers() & Qt::ControlModifier) != 0)
    {
        const qreal factor = (event->angleDelta().y() > 0)
                             ? G_ZOOM_FACTOR
                             : 1.0 / G_ZOOM_FACTOR;
        scale(factor, factor);
        event->accept();
    }
    else
    {
        // 普通滚轮交给基类处理垂直滚动
        QGraphicsView::wheelEvent(event);
    }
}

void AnnotationView::keyPressEvent(QKeyEvent* event)
{
    // Ctrl+Z 撤销
    if (event->matches(QKeySequence::Undo))
    {
        if ((m_scene != nullptr) && (m_scene->undoStack() != nullptr))
        {
            m_scene->undoStack()->undo();
        }
        event->accept();
        return;
    }
    // Ctrl+Y 重做
    if (event->matches(QKeySequence::Redo))
    {
        if ((m_scene != nullptr) && (m_scene->undoStack() != nullptr))
        {
            m_scene->undoStack()->redo();
        }
        event->accept();
        return;
    }
    // Delete 键删除选中图元
    if ((event->key() == Qt::Key_Delete) && (m_scene != nullptr))
    {
        m_scene->deleteSelected();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void AnnotationView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    // 视图大小变化时不强制 fit，保持用户缩放状态
}
