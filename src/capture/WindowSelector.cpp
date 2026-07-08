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

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <windowsx.h>
#  include <dwmapi.h>
#endif

#include "utils/Logger.h"

#ifdef Q_OS_WIN
#  pragma comment(lib, "dwmapi.lib")
#endif

namespace {

/// \brief 遮罩透明度
constexpr int G_MASK_ALPHA = 110;
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
/// \brief 标题缓冲区长度
constexpr int G_TITLE_BUFFER_LEN = 256;
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
}

WindowSelector::~WindowSelector() = default;

void WindowSelector::start()
{
    setupFullScreen();
    show();
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
}

HWND WindowSelector::hwndFromPoint(const QPoint& pt)
{
#ifdef Q_OS_WIN
    POINT point{ pt.x(), pt.y() };
    // 通过 WindowFromPoint 找到窗口
    HWND hwnd = WindowFromPoint(point);
    // Fail-Fast：找不到时返回空
    if (hwnd == nullptr)
    {
        return nullptr;
    }
    // 进一步细化到子窗口
    HWND child = ChildWindowFromPoint(hwnd, point);
    if ((child != nullptr) && (child != hwnd))
    {
        return child;
    }
    return hwnd;
#else
    Q_UNUSED(pt);
    return nullptr;
#endif
}

QRect WindowSelector::windowRectForPaint(HWND hwnd)
{
#ifdef Q_OS_WIN
    // Fail-Fast：句柄无效时返回空
    if (hwnd == nullptr)
    {
        return {};
    }
    // 优先使用 DWM 真实矩形（含阴影区），但截屏时用 GetWindowRect 更准确
    RECT windowRect{};
    if (DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                              &windowRect, sizeof(windowRect)) == S_OK)
    {
        return QRect(windowRect.left, windowRect.top,
                     windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
    }
    if (::GetWindowRect(hwnd, &windowRect) != 0)
    {
        return QRect(windowRect.left, windowRect.top,
                     windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
    }
    return {};
#else
    Q_UNUSED(hwnd);
    return {};
#endif
}

void WindowSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0, 0, 0, G_MASK_ALPHA));

    // 有当前高亮窗口时绘制
    if (!m_currentRect.isNull() && (m_currentHwnd != nullptr))
    {
        drawWindowHighlight(painter);
    }
}

void WindowSelector::drawWindowHighlight(QPainter& painter)
{
#ifdef Q_OS_WIN
    // 挖空高亮区
    QPainterPath path;
    path.addRect(rect());
    path.addRect(m_currentRect);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillPath(path, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // 高亮边框
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
    wchar_t title[G_TITLE_BUFFER_LEN] = {0};
    GetWindowTextW(m_currentHwnd, title, G_TITLE_BUFFER_LEN);
    QString titleStr = QString::fromWCharArray(title);

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
    HWND hwnd = hwndFromPoint(event->globalPosition().toPoint());
    // 窗口变化时更新高亮
    if (hwnd != m_currentHwnd)
    {
        m_currentHwnd = hwnd;
        m_currentRect = windowRectForPaint(hwnd);
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
        if (m_currentHwnd != nullptr)
        {
            Q_EMIT windowSelected(m_currentHwnd);
        }
        else
        {
            Q_EMIT cancelled();
        }
        close();
    }
    else if (event->button() == Qt::RightButton)
    {
        Q_EMIT cancelled();
        close();
    }
}
