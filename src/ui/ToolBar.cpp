/**
 * \file ToolBar.cpp
 * \brief ToolBar 实现：初始化所有动作与按钮、窗口拖拽逻辑
 */
#include "ToolBar.h"

#include <QAction>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>
#include <QStyle>
#include <QMouseEvent>
#include <QIcon>
#include <QPixmap>

namespace SK {

namespace {
/// \brief 默认模式索引：画框截图
constexpr int G_DEFAULT_MODE_INDEX = 0;
/// \brief 工具栏图标尺寸
constexpr int G_ICON_SIZE = 18;

/// \brief 截屏按钮图标（正常 / 悬停）
const QString G_ICON_CUT       = QStringLiteral(":/icons/cut.png");
const QString G_ICON_CUT_HOVER = QStringLiteral(":/icons/cut_hover.png");
/// \brief 保存按钮图标（正常 / 悬停）
const QString G_ICON_SAVE       = QStringLiteral(":/icons/save.png");
const QString G_ICON_SAVE_HOVER  = QStringLiteral(":/icons/save_hover.png");
/// \brief 最小化按钮图标（正常 / 悬停）
const QString G_ICON_MINIMIZE       = QStringLiteral(":/icons/minimize.png");
const QString G_ICON_MINIMIZE_HOVER = QStringLiteral(":/icons/minimize_hover.png");
/// \brief 关闭按钮图标（正常 / 悬停）
const QString G_ICON_CLOSE       = QStringLiteral(":/icons/close.png");
const QString G_ICON_CLOSE_HOVER = QStringLiteral(":/icons/close_hover.png");
/// \brief 画框截图模式图标
const QString G_ICON_FRAME  = QStringLiteral(":/icons/frame.png");
/// \brief 窗口截图模式图标
const QString G_ICON_WINDOW = QStringLiteral(":/icons/window.png");
/// \brief 全屏截图模式图标
const QString G_ICON_FULL   = QStringLiteral(":/icons/full.png");
/// \brief 滚动截图模式图标
const QString G_ICON_SCROLL = QStringLiteral(":/icons/scroll.png");

/**
 * @brief 构造带悬停状态的图标（用于工具按钮）
 * @param normalPath 正常状态（未悬停）图标资源路径
 * @param hoverPath  悬停状态图标资源路径
 * @return 已注册 Normal/Active 多模式的 QIcon
 *
 * 说明：
 *   - Normal/Off  → 默认图标
 *   - Active/Off  → 鼠标悬停于按钮时的图标
 */
QIcon makeHoverIcon(const QString& normalPath, const QString& hoverPath)
{
    QIcon icon;
    QPixmap normalPix(normalPath);
    QPixmap hoverPix(hoverPath);

    icon.addPixmap(normalPix, QIcon::Normal, QIcon::Off);
    icon.addPixmap(hoverPix,  QIcon::Active, QIcon::Off);
    return icon;
}
} // namespace

/**
 * @brief 可拖拽的弹性间隔控件
 *
 * 替代普通 QWidget spacer。普通 spacer 会消费鼠标事件，
 * 导致用户无法通过拖拽工具栏空白区域移动窗口。
 *
 * 本控件重写鼠标事件，在空白区域按下左键即可拖动整个窗口，
 * 同时背景透明，不影响工具栏的渐变样式。
 */
class DragHandleWidget : public QWidget
{
public:
    explicit DragHandleWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setObjectName("dragHandle");
        // 透明背景：不绘制自身底色，继承工具栏渐变
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_dragging = true;
            m_dragOffset = event->globalPosition().toPoint() - window()->pos();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_dragging)
        {
            window()->move(event->globalPosition().toPoint() - m_dragOffset);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        m_dragging = false;
        QWidget::mouseReleaseEvent(event);
    }

private:
    QPoint  m_dragOffset;          ///< 拖拽起点偏移
    bool    m_dragging = false;    ///< 当前是否正在拖拽
};

ToolBar::ToolBar(QWidget* parent)
    : QToolBar(parent)
{
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(G_ICON_SIZE, G_ICON_SIZE));
    setupActions();
}

void ToolBar::setupActions()
{
    // ---- 截屏按钮 ----
    m_actCapture = new QAction(makeHoverIcon(G_ICON_CUT, G_ICON_CUT_HOVER),
                                tr("截屏"), this);
    m_actCapture->setToolTip(tr("开始截屏  (Ctrl+Alt+A)"));
    m_actCapture->setShortcut(QKeySequence("Ctrl+Alt+A"));
    connect(m_actCapture, &QAction::triggered,
            this, &ToolBar::captureClicked);

    m_btnCapture = new QToolButton(this);
    m_btnCapture->setDefaultAction(m_actCapture);
    m_btnCapture->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnCapture->setObjectName("captureButton");
    addWidget(m_btnCapture);

    // ---- 模式下拉 ----
    m_btnMode = new QToolButton(this);
    m_btnMode->setText(tr("画框截图") + QStringLiteral(" ▾"));
    m_btnMode->setToolTip(tr("切换截屏模式"));
    m_btnMode->setPopupMode(QToolButton::InstantPopup);
    m_btnMode->setObjectName("modeButton");
    m_btnMode->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // 初始模式图标：画框截图（菜单项不使用 hover 变体）
    m_btnMode->setIcon(QIcon(G_ICON_FRAME));

    QMenu* modeMenu = new QMenu(m_btnMode);
    QActionGroup* group = new QActionGroup(modeMenu);
    group->setExclusive(true);

    // 添加模式的 lambda（菜单项只用普通图标，不使用 hover 变体）
    auto addMode = [&](const QString& text, int mode, const QString& iconPath)
    {
        QAction* action = modeMenu->addAction(QIcon(iconPath), text);
        action->setCheckable(true);
        action->setData(mode);
        group->addAction(action);
        if (mode == G_DEFAULT_MODE_INDEX)
        {
            action->setChecked(true);   // 默认画框
        }
        connect(action, &QAction::triggered, this,
                [this, text, mode, iconPath]()
        {
            m_btnMode->setText(text + QStringLiteral(" ▾"));
            m_btnMode->setIcon(QIcon(iconPath));
            Q_EMIT captureModeChanged(mode);
        });
    };

    // 顺序：画框 / 窗口 / 全屏 / 滚动
    addMode(tr("画框截图"), 0, G_ICON_FRAME);
    addMode(tr("窗口截图"), 2, G_ICON_WINDOW);
    addMode(tr("全屏截图"), 1, G_ICON_FULL);
    addMode(tr("滚动截图"), 3, G_ICON_SCROLL);

    m_btnMode->setMenu(modeMenu);
    addWidget(m_btnMode);

    // ---- 保存按钮（截屏完成后显示） ----
    m_btnSave = new QToolButton(this);
    m_btnSave->setText(tr("保存"));
    m_btnSave->setIcon(makeHoverIcon(G_ICON_SAVE, G_ICON_SAVE_HOVER));
    m_btnSave->setToolTip(tr("保存截图  (Ctrl+S)"));
    m_btnSave->setShortcut(QKeySequence::Save);
    m_btnSave->setObjectName("saveButton");
    m_btnSave->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_btnSave, &QToolButton::clicked,
            this, &ToolBar::saveRequested);
    m_saveAction = addWidget(m_btnSave);
    m_saveAction->setVisible(false);

    addSeparator();

    // 弹性间隔：可拖拽的空白区域，把右侧按钮推到工具栏末端
    // 使用 DragHandleWidget 替代普通 QWidget，支持拖拽移动窗口
    auto* dragHandle = new DragHandleWidget(this);
    addWidget(dragHandle);

    // ---- 最小化 / 关闭 ----
    m_actMinimize = new QAction(makeHoverIcon(G_ICON_MINIMIZE, G_ICON_MINIMIZE_HOVER),
                                  tr("最小化"), this);
    m_actMinimize->setToolTip(tr("最小化到系统托盘"));
    connect(m_actMinimize, &QAction::triggered,
            this, &ToolBar::minimizeRequested);
    auto* btnMin = new QToolButton(this);
    btnMin->setDefaultAction(m_actMinimize);
    btnMin->setAutoRaise(true);
    btnMin->setObjectName("minimizeButton");
    addWidget(btnMin);

    m_actClose = new QAction(makeHoverIcon(G_ICON_CLOSE, G_ICON_CLOSE_HOVER),
                              tr("关闭"), this);
    m_actClose->setToolTip(tr("退出 ScreenKiller"));
    connect(m_actClose, &QAction::triggered,
            this, &ToolBar::closeRequested);
    auto* btnClose = new QToolButton(this);
    btnClose->setDefaultAction(m_actClose);
    btnClose->setAutoRaise(true);
    btnClose->setObjectName("closeButton");
    addWidget(btnClose);
}

void ToolBar::setShowSaveButton(bool visible)
{
    if (m_saveAction != nullptr)
    {
        m_saveAction->setVisible(visible);
    }
}

void ToolBar::setCaptureMode(int mode)
{
    QMenu* modeMenu = m_btnMode->menu();
    if (modeMenu == nullptr)
    {
        return;
    }

    // 遍历菜单项，找到 data() == mode 的项并设置为选中
    for (QAction* action : modeMenu->actions())
    {
        if (action->data().toInt() == mode)
        {
            action->setChecked(true);
            m_btnMode->setText(action->text() + QStringLiteral(" ▾"));
            m_btnMode->setIcon(action->icon());
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// 窗口拖拽（在工具栏空白区域按下左键拖动）
// -----------------------------------------------------------------------------
void ToolBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // actionAt 返回 nullptr 表示点击位置无交互动作（空白区域）
        if (actionAt(event->pos()) == nullptr)
        {
            m_dragging = true;
            m_dragOffset = event->globalPosition().toPoint() - window()->pos();
            event->accept();
            return;
        }
    }
    QToolBar::mousePressEvent(event);
}

void ToolBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging)
    {
        window()->move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QToolBar::mouseMoveEvent(event);
}

void ToolBar::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QToolBar::mouseReleaseEvent(event);
}

} // namespace SK
