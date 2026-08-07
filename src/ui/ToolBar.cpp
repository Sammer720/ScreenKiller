/**
 * \file ToolBar.cpp
 * \brief ToolBar 实现：初始化所有动作与按钮、窗口拖拽逻辑
 */
#include "ToolBar.h"

#include "sub_widget/ToolButton.h"

#include <QAction>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>
#include <QStyle>
#include <QMouseEvent>
#include <QIcon>
#include <QPixmap>
#include <QLayout>

namespace SK {

namespace {
/// \brief 默认模式索引：画框截图
constexpr int G_DEFAULT_MODE_INDEX = 0;
/// \brief 工具栏图标尺寸
constexpr int G_ICON_SIZE = 20;
/// \brief 模式按钮图标与文本的间距（像素，经 ToolButton 内部布局生效）
constexpr int G_MODE_ICON_TEXT_SPACING = 8;
constexpr int G_NORMAL_ICON_TEXT_SPACING = 3;

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
/// \brief 画框截图模式图标（默认 / 悬停 / 选中）
const QString G_ICON_FRAME       = QStringLiteral(":/icons/frame.png");
const QString G_ICON_FRAME_HOVER = QStringLiteral(":/icons/frame_hover.png");
const QString G_ICON_FRAME_SEL   = QStringLiteral(":/icons/frame_selected.png");
/// \brief 窗口截图模式图标（默认 / 悬停 / 选中）
const QString G_ICON_WINDOW       = QStringLiteral(":/icons/window.png");
const QString G_ICON_WINDOW_HOVER = QStringLiteral(":/icons/window_hover.png");
const QString G_ICON_WINDOW_SEL   = QStringLiteral(":/icons/window_selected.png");
/// \brief 全屏截图模式图标（默认 / 悬停 / 选中）
const QString G_ICON_FULL       = QStringLiteral(":/icons/full.png");
const QString G_ICON_FULL_HOVER = QStringLiteral(":/icons/full_hover.png");
const QString G_ICON_FULL_SEL   = QStringLiteral(":/icons/full_selected.png");
/// \brief 滚动截图模式图标（默认 / 悬停 / 选中）
const QString G_ICON_SCROLL       = QStringLiteral(":/icons/scroll.png");
const QString G_ICON_SCROLL_HOVER = QStringLiteral(":/icons/scroll_hover.png");
const QString G_ICON_SCROLL_SEL   = QStringLiteral(":/icons/scroll_selected.png");

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

/**
 * @brief 构造下拉菜单项双态图标（默认 / 悬停）
 * @param normalPath 默认图标
 * @param hoverPath  悬停图标（_hover）
 * @return 已注册多模式的 QIcon
 *
 * 说明：QMenu 绘制菜单项悬停态时使用的图标 Mode/State 组合随
 * QStyle/QSS 实现而异（Active/Selected × Off/On），这里把常见组合
 * 全部注册为 _hover 图标，确保任意路径都能命中。
 */
QIcon makeModeMenuIcon(const QString& normalPath, const QString& hoverPath)
{
    QIcon icon;
    QPixmap normalPix(normalPath);
    QPixmap hoverPix(hoverPath);
    icon.addPixmap(normalPix, QIcon::Normal,   QIcon::Off);
    icon.addPixmap(hoverPix,  QIcon::Active,   QIcon::Off);
    icon.addPixmap(hoverPix,  QIcon::Selected, QIcon::Off);
    icon.addPixmap(hoverPix,  QIcon::Selected, QIcon::On);
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
    m_actCapture = new QAction(makeHoverIcon(G_ICON_CUT, G_ICON_CUT_HOVER), tr("截屏"), this);
    m_actCapture->setShortcut(QKeySequence("Ctrl+Alt+A"));
    connect(m_actCapture, &QAction::triggered, this, &ToolBar::captureClicked);

    m_btnCapture = new ToolButton(this);
    m_btnCapture->setDefaultAction(m_actCapture);
    m_btnCapture->setIconTextSpacing(G_NORMAL_ICON_TEXT_SPACING);
    m_btnCapture->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnCapture->setObjectName("captureButton");
    addWidget(m_btnCapture);

    // ---- 模式下拉 ----
    m_btnMode = new ToolButton(this);
    m_btnMode->setText(tr("画框截图"));
    m_btnMode->setPopupMode(QToolButton::InstantPopup);
    m_btnMode->setObjectName("modeButton");
    m_btnMode->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // 图标与文本间距：通过内部布局 spacing 控制（默认 2px，此处适当加大）
    m_btnMode->setIconTextSpacing(G_MODE_ICON_TEXT_SPACING);
    // 初始模式图标：画框截图（三态：默认 / 悬停 / 按下激活，事件驱动切换）
    m_btnMode->setTriStateIcons(QIcon(G_ICON_FRAME),
                                QIcon(G_ICON_FRAME_HOVER),
                                QIcon(G_ICON_FRAME_SEL));
    m_btnMode->setMinimumWidth(135);

    QMenu* modeMenu = new QMenu(m_btnMode);
    QActionGroup* group = new QActionGroup(modeMenu);
    group->setExclusive(true);

    // 添加模式的 lambda（菜单项悬停用 _hover，按钮三态由 ToolButton 事件驱动）
    auto addMode = [&](const QString& text, int mode,
                       const QString& normalPath,
                       const QString& hoverPath,
                       const QString& selectedPath)
    {
        QAction* action = modeMenu->addAction(
            makeModeMenuIcon(normalPath, hoverPath), text);
        action->setCheckable(true);
        action->setData(mode);
        // 保存三态路径，供 setCaptureMode 恢复按钮图标
        action->setProperty("iconNormal", normalPath);
        action->setProperty("iconHover", hoverPath);
        action->setProperty("iconSelected", selectedPath);
        group->addAction(action);
        if (mode == G_DEFAULT_MODE_INDEX)
        {
            action->setChecked(true);   // 默认画框
        }
        connect(action, &QAction::triggered, this,
                [this, text, mode, normalPath, hoverPath, selectedPath]()
        {
            m_btnMode->setText(text);
            m_btnMode->setTriStateIcons(QIcon(normalPath),
                                        QIcon(hoverPath),
                                        QIcon(selectedPath));
            Q_EMIT captureModeChanged(mode);
        });
    };

    // 顺序：画框 / 窗口 / 全屏 / 滚动
    addMode(tr("画框截图"), 0, G_ICON_FRAME, G_ICON_FRAME_HOVER, G_ICON_FRAME_SEL);
    addMode(tr("窗口截图"), 2, G_ICON_WINDOW, G_ICON_WINDOW_HOVER, G_ICON_WINDOW_SEL);
    addMode(tr("全屏截图"), 1, G_ICON_FULL, G_ICON_FULL_HOVER, G_ICON_FULL_SEL);
    addMode(tr("滚动截图"), 3, G_ICON_SCROLL, G_ICON_SCROLL_HOVER, G_ICON_SCROLL_SEL);

    m_btnMode->attachMenu(modeMenu);
    addWidget(m_btnMode);

    // ---- 保存按钮（截屏完成后显示） ----
    m_btnSave = new ToolButton(this);
    m_btnSave->setText(tr("保存"));
    m_btnSave->setIcon(makeHoverIcon(G_ICON_SAVE, G_ICON_SAVE_HOVER));
    m_btnSave->setShortcut(QKeySequence::Save);
    m_btnSave->setObjectName("saveButton");
    m_btnSave->setIconTextSpacing(G_NORMAL_ICON_TEXT_SPACING);
    m_btnSave->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_btnSave, &QToolButton::clicked, this, &ToolBar::saveRequested);
    m_saveAction = addWidget(m_btnSave);
    m_saveAction->setVisible(false);

    addSeparator();

    // 弹性间隔：可拖拽的空白区域，把右侧按钮推到工具栏末端
    // 使用 DragHandleWidget 替代普通 QWidget，支持拖拽移动窗口
    auto* dragHandle = new DragHandleWidget(this);
    addWidget(dragHandle);

    // ---- 最小化 / 关闭 ----
    m_actMinimize = new QAction(makeHoverIcon(G_ICON_MINIMIZE, G_ICON_MINIMIZE_HOVER), tr("最小化"), this);
    connect(m_actMinimize, &QAction::triggered, this, &ToolBar::minimizeRequested);
    auto* btnMin = new ToolButton(this);
    btnMin->setDefaultAction(m_actMinimize);
    btnMin->setAutoRaise(true);
    btnMin->setObjectName("minimizeButton");
    addWidget(btnMin);

    m_actClose = new QAction(makeHoverIcon(G_ICON_CLOSE, G_ICON_CLOSE_HOVER), tr("关闭"), this);
    connect(m_actClose, &QAction::triggered, this, &ToolBar::closeRequested);
    auto* btnClose = new ToolButton(this);
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
            m_btnMode->setText(action->text());
            // 恢复按钮为三态图标（从 action 保存的路径重建）
            m_btnMode->setTriStateIcons(
                QIcon(action->property("iconNormal").toString()),
                QIcon(action->property("iconHover").toString()),
                QIcon(action->property("iconSelected").toString()));
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
