/**
 * \file ToolButton.cpp
 * \brief 图标与文本间距可控的 QToolButton 子类实现
 *
 * 绘制流程：
 *   1. initStyleOption() 获取当前的样式选项（含 QSS 状态）；
 *   2. 仅水平布局（ToolButtonTextBesideIcon）时：
 *      a. 将选项中的文本/图标清空，调用 drawComplexControl(CC_ToolButton)
 *         让样式绘制外壳（背景、边框、悬停/按下状态、菜单指示器）；
 *      b. 手动计算「图标 + 间距 + 文本」的水平布局并整体居中绘制；
 *   3. 其余样式（纯图标/纯文本/文字在下）直接交回基类默认绘制。
 */
#include "ToolButton.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QFontMetrics>
#include <QEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QMenu>

namespace SK {

ToolButton::ToolButton(QWidget* parent)
    : QToolButton(parent)
{
}

void ToolButton::setIconTextSpacing(int spacing)
{
    // 间距未变化时直接返回，避免无意义的布局重算
    if (m_iconTextSpacing == spacing)
    {
        return;
    }
    m_iconTextSpacing = spacing;
    updateGeometry();  // 重新计算尺寸（含 sizeHint）
    update();          // 触发重绘
}

int ToolButton::iconTextSpacing() const
{
    return m_iconTextSpacing;
}

void ToolButton::setTriStateIcons(const QIcon& normal,
                                  const QIcon& hover,
                                  const QIcon& selected)
{
    m_iconNormal = normal;
    m_iconHover = hover;
    m_iconSelected = selected;
    // hover/selected 变体齐全才启用事件切换
    m_triStateEnabled = !hover.isNull() && !selected.isNull();
    // 无论变体是否齐全，默认图标都必须设置，避免按钮无图标显示
    if (!normal.isNull())
    {
        setIcon(normal);
    }
}

bool ToolButton::event(QEvent* event)
{
    // 拦截 ToolTip：QToolButton 收到该事件会直接显示 action 的 tooltip/text，
    // 置空 tooltip 无法阻止，必须在此吞掉事件
    if (event->type() == QEvent::ToolTip)
    {
        event->accept();
        return true;
    }
    return QToolButton::event(event);
}

void ToolButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    updateIconByState();
    QToolButton::enterEvent(event);
}

void ToolButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    // 菜单打开期间鼠标会移出按钮（到菜单上），保持按下态；
    // 仅无菜单或菜单未打开时复位
    if ((menu() == nullptr) || !menu()->isVisible())
    {
        m_pressed = false;
    }
    updateIconByState();
    QToolButton::leaveEvent(event);
}

void ToolButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_pressed = true;
        updateIconByState();
    }
    QToolButton::mousePressEvent(event);
}

void ToolButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 带菜单的按钮：按下态完全交给 aboutToShow/aboutToHide 管理
        // （InstantPopup 下 release 时序不稳定，此处不清除）
        if (menu() == nullptr)
        {
            m_pressed = false;
            updateIconByState();
        }
    }
    QToolButton::mouseReleaseEvent(event);
}

void ToolButton::attachMenu(QMenu* menu)
{
    QToolButton::setMenu(menu);
    if (menu == nullptr)
    {
        return;
    }
    // 菜单即将显示：进入按下激活态（_selected）
    connect(menu, &QMenu::aboutToShow, this, [this]()
    {
        m_pressed = true;
        updateIconByState();
    });
    // 菜单关闭：复位按下态，按悬停状态恢复正常图标
    connect(menu, &QMenu::aboutToHide, this, [this]()
    {
        m_pressed = false;
        updateIconByState();
    });
}

void ToolButton::updateIconByState()
{
    // 未启用三态图标时不做任何切换
    if (!m_triStateEnabled)
    {
        return;
    }
    if (m_pressed)
    {
        // 按下/菜单打开：按下激活图标（_selected）
        setIcon(m_iconSelected);
    }
    else if (m_hovered)
    {
        // 悬停：悬停图标（_hover）
        setIcon(m_iconHover);
    }
    else
    {
        // 常规：默认图标
        setIcon(m_iconNormal);
    }
}

void ToolButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    QStyleOptionToolButton opt;
    initStyleOption(&opt);

    // 仅水平布局（图标在左、文字在右）时手动排列；其余样式交回基类
    if ((opt.toolButtonStyle == Qt::ToolButtonTextBesideIcon)
        && !opt.text.isEmpty())
    {
        // 1) 外壳：临时清空内容，让样式只绘制背景/边框/状态/菜单指示器
        QStyleOptionToolButton bgOpt = opt;
        bgOpt.text.clear();
        bgOpt.icon = QIcon();
        style()->drawComplexControl(QStyle::CC_ToolButton, &bgOpt,
                                    &painter, this);

        // 2) 内容区域：按钮矩形，右侧预留菜单指示器空间
        QRect contentRect = opt.rect;
        if (opt.features.testFlag(QStyleOptionToolButton::HasMenu))
        {
            const int indicatorWidth = style()->pixelMetric(
                QStyle::PM_MenuButtonIndicator, &opt, this);
            contentRect.setRight(contentRect.right() - indicatorWidth);
        }

        // 3) 图标与文本的尺寸
        const QSize iconSize = opt.iconSize;
        const QFontMetrics fm(opt.font);
        const int textWidth = fm.horizontalAdvance(opt.text);

        // 4) 整体水平居中：图标 + 间距 + 文本
        const int totalWidth = iconSize.width() + m_iconTextSpacing + textWidth;
        int x = contentRect.left() + (contentRect.width() - totalWidth) / 2;
        const int centerY = contentRect.center().y();

        // 5) 绘制图标（按悬停/按下激活/禁用/常规状态取对应变体）
        const QRect iconRect(x, centerY - iconSize.height() / 2,
                             iconSize.width(), iconSize.height());
        QIcon::Mode iconMode = QIcon::Normal;
        QIcon::State iconState = QIcon::Off;
        if (!opt.state.testFlag(QStyle::State_Enabled))
        {
            // 禁用态：Disabled 变体
            iconMode = QIcon::Disabled;
        }
        else if (opt.state.testFlag(QStyle::State_Sunken)
                 || opt.state.testFlag(QStyle::State_On))
        {
            // 按下/激活态：Selected/On 变体（_selected）
            iconMode = QIcon::Selected;
            iconState = QIcon::On;
        }
        else if (opt.state.testFlag(QStyle::State_MouseOver))
        {
            // 悬停态：Active 变体（_hover）
            iconMode = QIcon::Active;
        }
        opt.icon.paint(&painter, iconRect, Qt::AlignCenter,
                       iconMode, iconState);

        // 6) 绘制文本（颜色随按钮状态取自调色板）
        x += iconSize.width() + m_iconTextSpacing;
        const QRect textRect(x, contentRect.top(),
                             textWidth, contentRect.height());
        painter.save();
        painter.setFont(opt.font);
        const QPalette::ColorGroup colorGroup =
            opt.state.testFlag(QStyle::State_Enabled)
            ? QPalette::Normal
            : QPalette::Disabled;
        painter.setPen(opt.palette.color(colorGroup, QPalette::ButtonText));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, opt.text);
        painter.restore();
        return;
    }

    QToolButton::paintEvent(event);
}

QSize ToolButton::sizeHint() const
{
    QSize base = QToolButton::sizeHint();

    // 水平布局下，将自定义间距与默认间距的差值补偿到宽度，避免文本被裁切
    if (toolButtonStyle() == Qt::ToolButtonTextBesideIcon)
    {
        const int defaultSpacing = style()->pixelMetric(
            QStyle::PM_ToolBarItemSpacing, nullptr, this);
        const int diff = m_iconTextSpacing - defaultSpacing;
        if (diff > 0)
        {
            base.setWidth(base.width() + diff);
        }
    }
    return base;
}

} // namespace SK