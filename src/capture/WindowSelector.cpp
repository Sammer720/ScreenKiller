/**
 * \file WindowSelector.cpp
 * \brief 窗口选择器实现
 */
#include "WindowSelector.h"

#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPen>
#include <QTimer>
#include <QFontMetrics>
#include <QString>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "platform/WinApi.h"

namespace {

/// \brief 遮罩透明度
constexpr int G_MASK_ALPHA = 110;
/// \brief 遮罩关闭后等待画面刷新的延迟（毫秒），确保截屏时遮罩已隐藏
constexpr int G_CLOSE_SETTLE_MS = 50;
/// \brief Win11 强调色 RGB 分量
const int G_ACCENT_R = 0;
const int G_ACCENT_G = 120;
const int G_ACCENT_B = 215;
/// \brief 高亮边框透明度
constexpr int G_BORDER_ALPHA = 240;
/// \brief 高亮边框线宽
constexpr int G_BORDER_WIDTH = 3;
/// \brief 标题标签透明度
constexpr int G_TITLE_BG_ALPHA = 230;
/// \brief 标题标签距窗口顶部偏移
constexpr int G_TITLE_TAG_OFFSET = 24;
/// \brief 标题标签高度
constexpr int G_TITLE_TAG_HEIGHT = 22;
/// \brief 标题标签最大宽度
constexpr int G_TITLE_TAG_MAX_WIDTH = 400;
/// \brief 标题标签字体点大小（较系统默认适当缩小）
constexpr int G_TITLE_TAG_FONT_POINT = 9;
/// \brief 标题标签内边距
constexpr int G_TITLE_TAG_PADDING = 10;
/// \brief 圆角矩形半径
constexpr int G_TAG_RADIUS = 4;

/// \brief 反色十字光标描边线宽（像素）
constexpr int G_CROSSHAIR_OUTLINE_WIDTH = 3;
/// \brief 反色十字光标主体线宽（像素）
constexpr int G_CROSSHAIR_LINE_WIDTH = 1;

} // namespace

WindowSelector::WindowSelector(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::BlankCursor);
    setMouseTracking(true);
}

WindowSelector::~WindowSelector() = default;

void WindowSelector::start()
{
    // 预截屏全屏图像，用于反色十字光标的像素采样
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen != nullptr)
    {
        m_screenCapture = screen->grabWindow(0).toImage();
    }
    m_cursorPos = QPoint(-1, -1);

    setupFullScreen();
    show();
    m_overlayHwnd = reinterpret_cast<HWND>(winId());
    activateWindow();
    setFocus();
}

void WindowSelector::setupFullScreen()
{
    QScreen* primary = QGuiApplication::primaryScreen();
    // Fail-Fast：屏幕无效时直接返回
    if (primary == nullptr)
    {
        return;
    }
    setGeometry(primary->virtualGeometry());
    // 注掉：直接悬浮并点击任务栏截屏无法可靠实现，遮罩不再挖洞
    // applyTaskbarHoleToRgn();
}

// 注掉：直接悬浮并点击任务栏截屏无法可靠实现，遮罩不再挖洞
// void WindowSelector::applyTaskbarHoleToRgn()
// {
// #ifdef Q_OS_WIN
//     QVector<QRect> taskbarRects = SK::WinApi::getTaskbarRects();
//     // 无任务栏矩形时跳过区域裁剪
//     if (taskbarRects.isEmpty())
//     {
//         return;
//     }
//
//     HWND overlayHwnd = reinterpret_cast<HWND>(winId());
//     // Fail-Fast：窗口句柄无效时直接返回
//     if (overlayHwnd == nullptr)
//     {
//         return;
//     }
//
//     // 创建覆盖整个 widget 的初始区域（window-local 坐标）
//     HRGN fullRegion = CreateRectRgn(0, 0, width(), height());
//     if (fullRegion == nullptr)
//     {
//         return;
//     }
//
//     // 获取虚拟几何偏移（屏幕坐标），用于将任务栏矩形从屏幕坐标转为 widget 本地坐标
//     QScreen* primary = QGuiApplication::primaryScreen();
//     if (primary == nullptr)
//     {
//         DeleteObject(fullRegion);
//         return;
//     }
//     QPoint geometryOffset = primary->virtualGeometry().topLeft();
//
//     // 逐个减去任务栏矩形，形成「洞」
//     for (const QRect& taskbarScreenRect : taskbarRects)
//     {
//         // 屏幕坐标 → widget 本地坐标
//         QRect localRect = taskbarScreenRect.translated(-geometryOffset);
//
//         // QRect（inclusive right/bottom）→ RECT（exclusive right/bottom）
//         RECT holeRect;
//         holeRect.left = localRect.left();
//         holeRect.top = localRect.top();
//         holeRect.right = localRect.left() + localRect.width();
//         holeRect.bottom = localRect.top() + localRect.height();
//
//         HRGN holeRegion = CreateRectRgnIndirect(&holeRect);
//         if (holeRegion != nullptr)
//         {
//             CombineRgn(fullRegion, fullRegion, holeRegion, RGN_DIFF);
//             DeleteObject(holeRegion);
//         }
//     }
//
//     // SetWindowRgn 转移 fullRegion 所有权给系统，不可再 DeleteObject(fullRegion)
//     SetWindowRgn(overlayHwnd, fullRegion, TRUE);
// #else
//     // 非 Windows 平台无操作
// #endif
// }

void WindowSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (!m_currentRect.isNull() && (m_currentHwnd != nullptr))
    {
        // 取反遮罩：用路径仅填充框外区域，框内不填充（保持透明可见）
        QPainterPath path;
        path.addRect(rect());
        path.addRect(m_currentRect);
        painter.fillPath(path, QColor(0, 0, 0, G_MASK_ALPHA));

        // 框内填充极低 alpha（1/255）以确保鼠标事件被本窗口捕获而非穿透到下层窗口
        painter.fillRect(m_currentRect, QColor(0, 0, 0, 1));

        // 在遮罩之上绘制反色十字光标，确保光标始终可见
        drawCrosshair(painter);

        drawWindowHighlight(painter);
    }
    else
    {
        painter.fillRect(rect(), QColor(0, 0, 0, G_MASK_ALPHA));

        // 无窗口高亮时仍绘制反色十字光标，保证光标始终可见
        drawCrosshair(painter);
    }
}

void WindowSelector::drawWindowHighlight(QPainter& painter)
{
#ifdef Q_OS_WIN
    // 高亮边框（遮罩处理已在 paintEvent 中完成）
    QPen pen(QColor(G_ACCENT_R, G_ACCENT_G, G_ACCENT_B, G_BORDER_ALPHA));
    pen.setWidth(G_BORDER_WIDTH);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_currentRect);

    // 显示窗口标题
    drawWindowTitleTag(painter);
#else
    Q_UNUSED(painter);
#endif
}

void WindowSelector::drawWindowTitleTag(QPainter& painter)
{
#ifdef Q_OS_WIN
    QString titleStr = SK::WinApi::getWindowTitle(m_currentHwnd);

    // 标题为空时不绘制标签
    if (titleStr.isEmpty())
    {
        return;
    }

    // 适当缩小字体，并用 QFontMetrics 实测文本宽度，使标签长度随标题自适应
    QFont font = painter.font();
    font.setPointSize(G_TITLE_TAG_FONT_POINT);
    painter.setFont(font);
    QFontMetrics fm(font);

    QString displayText = titleStr.split(" - " ).last();
    displayText = " " + displayText;

    int tagWidth = qMin(G_TITLE_TAG_MAX_WIDTH,
                        fm.horizontalAdvance(displayText) + G_TITLE_TAG_PADDING);
    QRect tagRect(m_currentRect.left(), m_currentRect.top() - G_TITLE_TAG_OFFSET,
                  tagWidth, G_TITLE_TAG_HEIGHT);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(G_ACCENT_R, G_ACCENT_G, G_ACCENT_B, G_TITLE_BG_ALPHA));
    painter.drawRoundedRect(tagRect, G_TAG_RADIUS, G_TAG_RADIUS);
    painter.setPen(QPen(Qt::white));
    painter.drawText(tagRect, Qt::AlignVCenter | Qt::AlignLeft, displayText);
#endif
}

QColor WindowSelector::computeInverseColor(const QPoint& pos) const
{
    /// @brief 无预截屏数据时返回白色作为安全兜底
    if (m_screenCapture.isNull())
    {
        return Qt::white;
    }

    QScreen* primary = QGuiApplication::primaryScreen();

    /// 获取虚拟桌面几何偏移（多点显示器场景下主屏不一定是 (0,0)）
    QPoint geoOffset = (primary != nullptr)
                       ? primary->virtualGeometry().topLeft()
                       : QPoint(0, 0);

    /// 考虑设备像素比，将本地坐标映射到截屏图像的实际像素位置
    qreal ratio = m_screenCapture.devicePixelRatio();
    int imgX = static_cast<int>((pos.x() - geoOffset.x()) * ratio);
    int imgY = static_cast<int>((pos.y() - geoOffset.y()) * ratio);

    /// 坐标越界检查，越界时返回白色避免访问无效内存
    if ((imgX < 0) || (imgX >= m_screenCapture.width())
        || (imgY < 0) || (imgY >= m_screenCapture.height()))
    {
        return Qt::white;
    }

    QRgb pixel = m_screenCapture.pixel(imgX, imgY);

    /// WindowSelector 遮罩区域判断：
    /// - 窗口高亮区域内（m_currentRect.contains）遮罩几乎透明（alpha=1），
    ///   背景接近原色，反色需按 1/255 衰减
    /// - 区域外遮罩 alpha=G_MASK_ALPHA=110，背景被遮罩压暗，
    ///   反色需按 110/255 衰减
    int maskAlpha = G_MASK_ALPHA;
    if (!m_currentRect.isNull() && m_currentRect.contains(pos))
    {
        maskAlpha = 1;
    }

    /// 按遮罩透明度衰减原始 RGB 分量后再取反
    int r = qRed(pixel)   * (255 - maskAlpha) / 255;
    int g = qGreen(pixel) * (255 - maskAlpha) / 255;
    int b = qBlue(pixel)  * (255 - maskAlpha) / 255;

    return QColor(255 - r, 255 - g, 255 - b);
}

void WindowSelector::drawCrosshair(QPainter& painter)
{
    /// @brief 光标位置无效（-1,-1）时不绘制，此时鼠标已离开遮罩
    if ((m_cursorPos.x() < 0) || (m_cursorPos.y() < 0))
    {
        return;
    }

    /// 计算光标所在位置的反色
    QColor inverseColor = computeInverseColor(m_cursorPos);

    /// 临时禁用抗锯齿，确保十字线为精确的 1px 像素直线
    bool savedAA = painter.renderHints().testFlag(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::Antialiasing, false);

    /// 第一步：绘制 3px 暗色描边，确保十字线在任何背景下都清晰可见
    QPen outlinePen(QColor(0, 0, 0, 180));
    outlinePen.setWidth(G_CROSSHAIR_OUTLINE_WIDTH);
    painter.setPen(outlinePen);
    painter.drawLine(0, m_cursorPos.y(), width(), m_cursorPos.y());
    painter.drawLine(m_cursorPos.x(), 0, m_cursorPos.x(), height());

    /// 第二步：绘制 1px 反色主体，制造视觉反差效果
    QPen mainPen(inverseColor);
    mainPen.setWidth(G_CROSSHAIR_LINE_WIDTH);
    painter.setPen(mainPen);
    painter.drawLine(0, m_cursorPos.y(), width(), m_cursorPos.y());
    painter.drawLine(m_cursorPos.x(), 0, m_cursorPos.x(), height());

    /// 恢复原来的抗锯齿设置
    painter.setRenderHint(QPainter::Antialiasing, savedAA);
}

void WindowSelector::keyPressEvent(QKeyEvent* event)
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

void WindowSelector::mouseMoveEvent(QMouseEvent* event)
{
    // 更新光标位置，用于绘制反色十字光标
    m_cursorPos = event->pos();

    QPoint globalPos = event->globalPosition().toPoint();
    HWND hwnd = SK::WinApi::findTopLevelWindowAtPoint(globalPos.x(), globalPos.y(), m_overlayHwnd);
    // 窗口变化时更新高亮
    if (hwnd != m_currentHwnd)
    {
        m_currentHwnd = hwnd;
        m_currentRect = SK::WinApi::getWindowFrameRect(hwnd);
        // 转换为本 widget 坐标
        QScreen* primary = QGuiApplication::primaryScreen();
        if (primary != nullptr)
        {
            m_currentRect.translate(-primary->virtualGeometry().topLeft());
        }
        update();
    }

    // 无条件重绘，确保十字光标实时跟随鼠标
    update();
}

void WindowSelector::leaveEvent(QEvent* event)
{
    // 鼠标离开遮罩时重置光标位置，下次绘制时不会显示十字
    m_cursorPos = QPoint(-1, -1);
    update();
    QWidget::leaveEvent(event);
}

void WindowSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 先关闭遮罩，再延迟发射信号，确保截屏时遮罩已隐藏
        close();
        if (m_currentHwnd != nullptr)
        {
            HWND selectedHwnd = m_currentHwnd;
            QTimer::singleShot(G_CLOSE_SETTLE_MS, this,
                [this, selectedHwnd]
                {
                    Q_EMIT windowSelected(selectedHwnd);
                });
        }
        else
        {
            Q_EMIT cancelled();
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        Q_EMIT cancelled();
        close();
    }
}
