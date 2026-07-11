/**
 * \file RegionSelector.cpp
 * \brief 画框截屏选择器实现
 */
#include "RegionSelector.h"

#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <qcolor.h>
#include <qnamespace.h>
#include <qtmetamacros.h>

namespace {

/// \brief 遮罩透明度（0-255）
constexpr int G_MASK_ALPHA = 120;
/// \brief Win11 强调色 RGB 分量
const int G_ACCENT_R = 0;
const int G_ACCENT_G = 120;
const int G_ACCENT_B = 215;
/// \brief 选区边框透明度
constexpr int G_BORDER_ALPHA = 230;
/// \brief 选区边框线宽
constexpr int G_BORDER_WIDTH = 2;
/// \brief 尺寸标签字体大小
constexpr int G_TAG_FONT_POINT = 10;
/// \brief 尺寸标签宽度
constexpr int G_TAG_WIDTH = 120;
/// \brief 尺寸标签高度
constexpr int G_TAG_HEIGHT = 22;
/// \brief 尺寸标签距选区偏移
constexpr int G_TAG_OFFSET = 6;
/// \brief 圆角矩形半径
constexpr int G_TAG_RADIUS = 4;
/// \brief 尺寸标签背景透明度
constexpr int G_TAG_BG_ALPHA = 220;
/// \brief 选区最小有效尺寸（拖拽阈值）
constexpr int G_MIN_SELECTION_SIZE = 2;
/// \brief 选区绘制最小可见宽度
constexpr int G_MIN_VISIBLE_WIDTH = 1;

} // namespace

RegionSelector::RegionSelector(QWidget* parent)
    : QWidget(parent)
{
    // Frameless + 置顶 + Tool 窗口，半透明背景，跨屏显示
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setCursor(Qt::CrossCursor);
}

RegionSelector::~RegionSelector() = default;

void RegionSelector::setKeepOpen(bool keep)
{
    m_keepOpen = keep;
}

void RegionSelector::finish()
{
    close();
}

void RegionSelector::start()
{
    // 重置选区状态，避免上次截图的选区残留
    m_dragging  = false;
    m_startPos  = QPoint();
    m_endPos    = QPoint();
    m_selection = QRect();

    setupFullScreen();
    show();
    activateWindow();
    setFocus();
}

void RegionSelector::setupFullScreen()
{
    // 跨屏合并所有屏幕的虚拟几何
    QScreen* primary = QGuiApplication::primaryScreen();
    // Fail-Fast：屏幕无效时直接返回
    if (primary == nullptr)
    {
        return;
    }

    // 使用虚拟几何（包含所有屏幕的总区域）
    QRect virtualGeo = primary->virtualGeometry();
    setGeometry(virtualGeo);
}

// -----------------------------------------------------------------------------
// 绘制：遮罩 + 选区高亮 + 尺寸提示
// -----------------------------------------------------------------------------
void RegionSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 半透明深色遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, G_MASK_ALPHA));

    // 有有效选区时绘制高亮与尺寸提示
    if (!m_selection.isNull() && (m_selection.width() > G_MIN_VISIBLE_WIDTH))
    {
        drawSelectionHighlight(painter);
    }
}

void RegionSelector::drawSelectionHighlight(QPainter& painter)
{
    // 在选区位置"挖空"遮罩
    QPainterPath path;
    path.addRect(rect());
    path.addRect(m_selection);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillPath(path, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // Windows11 风格浅蓝色边框
    QPen pen(QColor(G_ACCENT_R, G_ACCENT_G, G_ACCENT_B, G_BORDER_ALPHA));
    pen.setWidth(G_BORDER_WIDTH);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_selection);

    // 尺寸提示标签
    drawSizeTag(painter);
}

void RegionSelector::drawSizeTag(QPainter& painter)
{
    QString sizeText = QString("%1 × %2")
                           .arg(m_selection.width())
                           .arg(m_selection.height());
    QFont font = painter.font();
    font.setPointSize(G_TAG_FONT_POINT);
    font.setBold(true);
    painter.setFont(font);

    QRect tagRect(m_selection.right() + G_TAG_OFFSET, m_selection.top(),
                  G_TAG_WIDTH, G_TAG_HEIGHT);
    // 右侧空间不足时显示在选区左侧
    if (tagRect.right() > rect().right())
    {
        tagRect.moveRight(m_selection.left() - G_TAG_OFFSET);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(G_ACCENT_R, G_ACCENT_G, G_ACCENT_B, G_TAG_BG_ALPHA));
    painter.drawRoundedRect(tagRect, G_TAG_RADIUS, G_TAG_RADIUS);
    painter.setPen(QPen(Qt::white));
    painter.drawText(tagRect, Qt::AlignCenter, sizeText);
}

// -----------------------------------------------------------------------------
// 事件
// -----------------------------------------------------------------------------
void RegionSelector::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        Q_EMIT cancelled();
        close();
    }
    else
    {
        QWidget::keyPressEvent(event);
    }
}

void RegionSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        m_startPos = event->pos();
        m_endPos   = m_startPos;
        m_selection = QRect(m_startPos, QSize());
        update();
    }
    else if (event->button() == Qt::RightButton)
    {
        Q_EMIT cancelled();
        close();
    }
}

void RegionSelector::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging)
    {
        m_endPos = event->pos();
        m_selection = QRect(m_startPos, m_endPos).normalized();
        update();
    }
}

void RegionSelector::mouseReleaseEvent(QMouseEvent* event)
{
    if ((event->button() == Qt::LeftButton) && m_dragging)
    {
        m_dragging = false;
        m_selection = QRect(m_startPos, m_endPos).normalized();
        if ((m_selection.width() > G_MIN_SELECTION_SIZE)
            && (m_selection.height() > G_MIN_SELECTION_SIZE))
        {
            // 转换为虚拟屏幕坐标
            QScreen* primary = QGuiApplication::primaryScreen();
            QRect virtualGeo = (primary != nullptr) ? primary->virtualGeometry() : rect();
            QRect globalSel = m_selection.translated(virtualGeo.topLeft());
            Q_EMIT regionSelected(globalSel);

            if (m_keepOpen)
            {
                // 保持遮罩可见，让鼠标事件穿透以便用户操作下层窗口
                setAttribute(Qt::WA_TransparentForMouseEvents, true);
                setCursor(Qt::ArrowCursor);
            }
            else
            {
                close();
            }
        }
        else
        {
            // 选区过小视为取消
            Q_EMIT cancelled();
            close();
        }
    }
}
