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

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

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
/// \brief 十字光标暗色描边宽度（像素）
constexpr int G_CROSSHAIR_OUTLINE_WIDTH = 3;
/// \brief 十字光标反色主体线宽（像素）
constexpr int G_CROSSHAIR_LINE_WIDTH = 1;

} // namespace

RegionSelector::RegionSelector(QWidget* parent)
    : QWidget(parent)
{
    // Frameless + 置顶 + Tool 窗口，半透明背景，跨屏显示
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setCursor(Qt::BlankCursor);
    setMouseTracking(true);
}

RegionSelector::~RegionSelector() = default;

void RegionSelector::setKeepOpen(bool keep)
{
    m_keepOpen = keep;
}

void RegionSelector::finish()
{
    // 清理预截屏数据，释放内存
    m_screenCapture = QImage();
    close();
}

void RegionSelector::start()
{
    // 重置选区状态，避免上次截图的选区残留
    m_dragging  = false;
    m_startPos  = QPoint();
    m_endPos    = QPoint();
    m_selection = QRect();

    // 预截屏全屏画面，用于反色十字光标实时取色
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen != nullptr)
    {
        m_screenCapture = screen->grabWindow(0).toImage();
    }
    m_cursorPos = QPoint(-1, -1);

    // 每次启动时重新设置光标，因 close() 后原生窗口销毁重建会丢失光标
    setCursor(Qt::BlankCursor);

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

    // 绘制反色十字光标（始终显示，不受选区状态影响）
    drawCrosshair(painter);
}

void RegionSelector::drawSelectionHighlight(QPainter& painter)
{
    // 仅挖空选区内部，使框内区域透明可见（框外保留半透明遮罩）
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(m_selection, Qt::transparent);
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
// 反色十字光标
// -----------------------------------------------------------------------------

/// @brief 计算指定光标位置在预截屏画面上的反色
///
/// 处理逻辑：
///   1. 空截屏保护，直接返回白色
///   2. 将控件坐标转换为预截屏图像的像素坐标（含 virtualGeometry 偏移 + DPI 缩放）
///   3. 边界检查，超出边界返回白色
///   4. 读取像素后按遮罩混合公式还原实际显示颜色
///   5. 取反返回
///
/// @param pos 光标在控件内的坐标（相对当前窗口左上角）
/// @return 反色后的颜色值
QColor RegionSelector::computeInverseColor(const QPoint& pos) const
{
    if (m_screenCapture.isNull())
    {
        return Qt::white;
    }

    // 获取虚拟屏幕偏移量，跨屏场景下修正坐标
    QScreen* primary = QGuiApplication::primaryScreen();
    QPoint geoOffset = (primary != nullptr) ? primary->virtualGeometry().topLeft()
                                            : QPoint(0, 0);
    qreal ratio = m_screenCapture.devicePixelRatio();

    // 将控件坐标映射到预截屏图像的像素坐标
    int imgX = static_cast<int>((pos.x() - geoOffset.x()) * ratio);
    int imgY = static_cast<int>((pos.y() - geoOffset.y()) * ratio);

    // 边界检查，防止越界访问
    if ((imgX < 0) || (imgX >= m_screenCapture.width())
        || (imgY < 0) || (imgY >= m_screenCapture.height()))
    {
        return Qt::white;
    }

    // 读取原始像素，并按遮罩透明度混合公式还原实际显示颜色
    QRgb pixel = m_screenCapture.pixel(imgX, imgY);
    int r = qRed(pixel)   * (255 - G_MASK_ALPHA) / 255;
    int g = qGreen(pixel) * (255 - G_MASK_ALPHA) / 255;
    int b = qBlue(pixel)  * (255 - G_MASK_ALPHA) / 255;

    return QColor(255 - r, 255 - g, 255 - b);
}

/// @brief 绘制反色十字光标
///
/// 绘制逻辑：
///   1. 光标位置无效（m_cursorPos 为负值）时直接返回
///   2. 计算光标所在像素的反色
///   3. 临时关闭抗锯齿，先绘制 3px 暗色描边，再绘制 1px 反色主体
///   4. 恢复抗锯齿状态
///
/// @param painter 画笔引用
void RegionSelector::drawCrosshair(QPainter& painter)
{
    if ((m_cursorPos.x() < 0) || (m_cursorPos.y() < 0))
    {
        return;
    }

    QColor inverseColor = computeInverseColor(m_cursorPos);

    // 临时禁用抗锯齿，确保十字线清晰锐利
    bool savedAA = painter.renderHints().testFlag(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 1. 暗色描边（3px），在黑暗背景上提供轮廓可见性
    QPen outlinePen(QColor(0, 0, 0, 180));
    outlinePen.setWidth(G_CROSSHAIR_OUTLINE_WIDTH);
    painter.setPen(outlinePen);
    painter.drawLine(0,                     m_cursorPos.y(),
                     width(),               m_cursorPos.y());
    painter.drawLine(m_cursorPos.x(),       0,
                     m_cursorPos.x(),       height());

    // 2. 反色主体（1px），精确标示光标位置
    QPen mainPen(inverseColor);
    mainPen.setWidth(G_CROSSHAIR_LINE_WIDTH);
    painter.setPen(mainPen);
    painter.drawLine(0,                     m_cursorPos.y(),
                     width(),               m_cursorPos.y());
    painter.drawLine(m_cursorPos.x(),       0,
                     m_cursorPos.x(),       height());

    // 恢复抗锯齿状态
    painter.setRenderHint(QPainter::Antialiasing, savedAA);
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
    // 实时追踪光标位置，用于反色十字光标绘制
    m_cursorPos = event->pos();
    update();

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
                m_cursorPos = QPoint(-1, -1);
                update();

#ifdef Q_OS_WIN
                // 设置 WS_EX_TRANSPARENT 让 OS 层将鼠标事件传递到下层窗口
                // 仅靠 Qt 的 WA_TransparentForMouseEvents 无法让 OS 重派发事件
                // 设置 WS_EX_TRANSPARENT 让 OS 将鼠标事件穿透到下层窗口
                // WA_TranslucentBackground 已在构造时设置 WS_EX_LAYERED
                // 此处只需追加 WS_EX_TRANSPARENT
                HWND hwnd = reinterpret_cast<HWND>(winId());
                if (hwnd != nullptr)
                {
                    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
                    SetWindowLongW(hwnd, GWL_EXSTYLE,
                                   exStyle | WS_EX_TRANSPARENT);
                }
#endif
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

void RegionSelector::leaveEvent(QEvent* event)
{
    // 鼠标离开窗口时隐藏十字光标并强制重绘
    m_cursorPos = QPoint(-1, -1);
    update();
    QWidget::leaveEvent(event);
}
