/**
 * \file AnnotationView.cpp
 * \brief 标注画布视图实现
 */
#include "AnnotationView.h"

#include "AnnotationScene.h"
#include "UndoStack.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QClipboard>
#include <QGuiApplication>

namespace {

/// \brief 缩放因子（每次滚轮缩放比例）
constexpr qreal G_ZOOM_FACTOR = 1.15;
/// \brief 视图背景色（浅灰蓝）
const int G_BG_R = 245;
const int G_BG_G = 247;
const int G_BG_B = 250;
/// \brief 平移边界占图片对应边长的比例（允许图片边缘移出视口的最大幅度）
constexpr qreal G_PAN_MARGIN_RATIO = 0.25;
/// \brief 中键「点击」与「拖动」的判定阈值（像素，曼哈顿距离）
constexpr int G_PAN_CLICK_THRESHOLD = 4;

} // namespace

AnnotationView::AnnotationView(SK::AnnotationScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent), m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform
                   | QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    // 适配后会重建滚动范围，这里重新应用平移边界
    applyPanMargin();
}

void AnnotationView::wheelEvent(QWheelEvent* event)
{
    // 滚轮向上放大、向下缩小
    // AnchorUnderMouse 变换锚点已保证缩放以光标位置为中心
    const qreal factor = (event->angleDelta().y() > 0)
                         ? G_ZOOM_FACTOR
                         : 1.0 / G_ZOOM_FACTOR;
    scale(factor, factor);
    // 缩放会重建滚动范围，这里重新应用平移边界
    applyPanMargin();
    // 通知外部监听者缩放比例已变化（如引导面板的缩放百分比显示）
    Q_EMIT zoomChanged(transform().m11());
    event->accept();
}

void AnnotationView::mousePressEvent(QMouseEvent* event)
{
    // 中键按下：记录平移起点、滚动条初值与拖动标志，进入平移模式
    // （拖动标志用于释放时区分「点击复位」与「拖动平移」）
    if (event->button() == Qt::MiddleButton)
    {
        m_panning = true;
        m_panMoved = false;
        m_panStart = event->pos();
        m_panStartHVal = horizontalScrollBar()->value();
        m_panStartVVal = verticalScrollBar()->value();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    // 右键按下：将当前标注成品图（背景 + 全部标注图元）复制到系统剪贴板
    if (event->button() == Qt::RightButton)
    {
        copyImageToClipboard();
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void AnnotationView::mouseMoveEvent(QMouseEvent* event)
{
    // 平移模式：按鼠标位移反向滚动滚动条，实现"内容跟随鼠标"的拖拽手感
    if (m_panning)
    {
        // 位移超过阈值时标记为实际拖动，
        // 保证中键「点击」（按下后几乎不动）不会被误判为平移结束后的复位
        if ((!m_panMoved)
            && ((event->pos() - m_panStart).manhattanLength() >= G_PAN_CLICK_THRESHOLD))
        {
            m_panMoved = true;
        }
        int dx = event->pos().x() - m_panStart.x();
        int dy = event->pos().y() - m_panStart.y();
        horizontalScrollBar()->setValue(m_panStartHVal - dx);
        verticalScrollBar()->setValue(m_panStartVVal - dy);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void AnnotationView::mouseReleaseEvent(QMouseEvent* event)
{
    // 中键释放：退出平移模式，恢复默认光标
    if ((event->button() == Qt::MiddleButton) && m_panning)
    {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        // 中键「点击」（未发生实际拖动）：恢复视图默认状态（自适应缩放 + 居中）
        if (!m_panMoved)
        {
            fitToView();
            // 复位后同步缩放比例给外部监听者（如引导面板的缩放百分比显示）
            Q_EMIT zoomChanged(transform().m11());
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
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
    // 尺寸变化会重建滚动范围，这里重新应用平移边界
    applyPanMargin();
}

void AnnotationView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    // 滚动条值变化可能触发范围重建（如缩放/适配时基类按场景矩形收紧范围），
    // 每次滚动后重新应用平移边界，保证超出图片边缘的平移不被弹回
    applyPanMargin();
}

void AnnotationView::applyPanMargin()
{
    QScrollBar* hBar = horizontalScrollBar();
    QScrollBar* vBar = verticalScrollBar();

    // 1. 计算常规可滚动范围（基类算法：图片可视尺寸 - 视口尺寸，单位场景像素）
    //    场景矩形起点为 (0,0) 且变换为均匀缩放，故可直接用视口尺寸除以缩放比换算
    const QRectF sceneRect = (m_scene != nullptr) ? m_scene->sceneRect() : QRectF();
    const qreal currentScale = transform().m11();
    const int regularMaxH = qMax(0, qRound(sceneRect.width() - viewport()->width() / currentScale));
    const int regularMaxV = qMax(0, qRound(sceneRect.height() - viewport()->height() / currentScale));

    // 2. 按图片尺寸比例计算平移边界（场景像素）：
    //    允许图片边缘移出视口最多「对应边长 × 比例」的距离，超出后被滚动范围锁住
    const int hMargin = qRound(sceneRect.width() * G_PAN_MARGIN_RATIO);
    const int vMargin = qRound(sceneRect.height() * G_PAN_MARGIN_RATIO);

    // 3. 绝对设置范围（幂等：不依赖当前滚动条状态，重复调用不会叠加边距）
    hBar->setRange(-hMargin, regularMaxH + hMargin);
    vBar->setRange(-vMargin, regularMaxV + vMargin);
}

void AnnotationView::copyImageToClipboard()
{
    // 防御性检查：场景不可用时直接返回
    if (m_scene == nullptr)
    {
        return;
    }
    // 导出当前画布（背景 + 全部标注图元）为单张成品图
    QImage exportedImage = m_scene->exportImage();
    // 背景为空（尚未加载图片）时导出结果为空，直接返回
    if (exportedImage.isNull())
    {
        return;
    }
    // 写入系统剪贴板（标准剪贴板模式）
    QGuiApplication::clipboard()->setImage(exportedImage, QClipboard::Clipboard);
    // 通知外部监听者（如主窗口）复制成功，用于触发托盘气泡提示
    Q_EMIT imageCopied();
}
