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

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "platform/WinApi.h"
#include "utils/Logger.h"

namespace {

/// \brief 遮罩透明度
constexpr int G_MASK_ALPHA = 110;
/// \brief 遮罩关闭后等待画面刷新的延迟（毫秒），确保截图时遮罩已隐藏
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
/// \brief 标题每字符估算宽度
constexpr int G_TITLE_CHAR_WIDTH = 10;
/// \brief 标题标签内边距
constexpr int G_TITLE_TAG_PADDING = 20;
/// \brief 圆角矩形半径
constexpr int G_TAG_RADIUS = 4;

} // namespace

WindowSelector::WindowSelector(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
}

WindowSelector::~WindowSelector() = default;

void WindowSelector::start()
{
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

        drawWindowHighlight(painter);
    }
    else
    {
        painter.fillRect(rect(), QColor(0, 0, 0, G_MASK_ALPHA));
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

    int tagWidth = qMin(G_TITLE_TAG_MAX_WIDTH,
                        titleStr.size() * G_TITLE_CHAR_WIDTH + G_TITLE_TAG_PADDING);
    QRect tagRect(m_currentRect.left(), m_currentRect.top() - G_TITLE_TAG_OFFSET,
                  tagWidth, G_TITLE_TAG_HEIGHT);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(G_ACCENT_R, G_ACCENT_G, G_ACCENT_B, G_TITLE_BG_ALPHA));
    painter.drawRoundedRect(tagRect, G_TAG_RADIUS, G_TAG_RADIUS);
    painter.setPen(QPen(Qt::white));
    painter.drawText(tagRect, Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("  ") + titleStr);
#endif
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
}

void WindowSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 先关闭遮罩，再延迟发射信号，确保截图时遮罩已隐藏
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
