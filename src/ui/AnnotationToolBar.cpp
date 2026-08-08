/**
 * \file AnnotationToolBar.cpp
 * \brief AnnotationToolBar 实现：工具按钮组、动态属性面板与信号发射
 */
#include "AnnotationToolBar.h"

#include "annotation/AnnotationConstants.h"

#include <QButtonGroup>
#include <QStackedWidget>
#include <QToolButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>

namespace SK {

namespace {

// ============================ 布局常量 ============================
/// 面板内边距（像素）
constexpr int G_PANEL_MARGIN = 8;
/// 面板内控件间距（像素）
constexpr int G_PANEL_SPACING = 6;
/// 工具按钮尺寸（像素）
constexpr int G_TOOL_BUTTON_SIZE = 36;
/// 工具按钮 emoji 字号（pt）
constexpr int G_TOOL_EMOJI_POINT_SIZE = 14;
/// 色板色块按钮尺寸（像素）
constexpr int G_COLOR_SWATCH_SIZE = 20;
/// 粗细档位按钮尺寸（像素）
constexpr int G_WIDTH_SWATCH_SIZE = 28;
/// 粗细圆点字号下限（像素）
constexpr int G_MIN_DOT_FONT_SIZE = 6;
/// 粗细圆点字号上限（像素）
constexpr int G_MAX_DOT_FONT_SIZE = 24;
/// 粗细圆点字号放大系数（档位值 × 系数 = 圆点字号）
constexpr qreal G_DOT_FONT_SCALE = 0.6;

// ============================ 工具按钮 emoji 占位 ============================
// TODO: 替换为图标资源，暂用 emoji 占位
/// 选择按钮占位文本
const QString G_EMOJI_SELECT      = QStringLiteral("🖱️");
/// 画笔按钮占位文本
const QString G_EMOJI_PEN         = QStringLiteral("✏️");
/// 荧光笔按钮占位文本
const QString G_EMOJI_HIGHLIGHTER = QStringLiteral("🖍️");
/// 直线按钮占位文本
const QString G_EMOJI_LINE        = QStringLiteral("📏");
/// 箭头按钮占位文本
const QString G_EMOJI_ARROW       = QStringLiteral("➡️");
/// 矩形按钮占位文本
const QString G_EMOJI_RECTANGLE   = QStringLiteral("🟦");
/// 椭圆按钮占位文本
const QString G_EMOJI_ELLIPSE     = QStringLiteral("⭕");
/// 文字按钮占位文本
const QString G_EMOJI_TEXT        = QStringLiteral("🅰️");
/// 马赛克按钮占位文本
const QString G_EMOJI_MOSAIC      = QStringLiteral("🔲");

// ============================ 粗细档位预设 ============================
// 档位取值已按各工具边界常量（G_MIN/MAX_*_WIDTH）预置于合法范围内，点击无需再 clamp
/// 画笔/几何（矩形/椭圆/直线/箭头）档位（像素）
const QVector<qreal> G_PEN_WIDTH_STEPS       = { 2.0, 4.0, 8.0, 12.0, 20.0 };
/// 荧光笔档位（像素）
const QVector<qreal> G_HIGHLIGHT_WIDTH_STEPS = { 10.0, 15.0, 20.0, 30.0, 40.0 };
/// 马赛克档位（像素）
const QVector<qreal> G_MOSAIC_WIDTH_STEPS    = { 15.0, 25.0, 40.0, 50.0, 60.0 };

// ============================ 文字属性默认值 ============================
/// 默认字号（pt，与 AnnotationScene 默认一致）
constexpr qreal G_DEFAULT_FONT_SIZE = 12.0;
/// 默认字体族（与 AnnotationScene 默认一致）
const QString G_DEFAULT_FONT_FAMILY = QStringLiteral("微软雅黑");

} // namespace

AnnotationToolBar::AnnotationToolBar(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AnnotationToolBar::setupUi()
{
    setObjectName(QStringLiteral("annotationToolBar"));
    setFixedWidth(SK::G_ANN_TOOLBAR_WIDTH);
    setStyleSheet(QStringLiteral(R"(
#annotationToolBar {
    background-color: rgba(255, 255, 255, 0.92);
    border: 1px solid rgba(0, 0, 0, 0.12);
    border-radius: 10px;
}
#annotationToolBar QToolButton {
    background: transparent;
    border: none;
    border-radius: 6px;
    padding: 4px;
}
#annotationToolBar QToolButton:hover {
    background-color: rgba(0, 0, 0, 0.06);
}
#annotationToolBar QToolButton:checked {
    background-color: rgba(0, 120, 215, 0.16);
}
#annotationToolBar QToolButton#colorSwatch {
    padding: 0px;
    border: 1px solid rgba(0, 0, 0, 0.25);
    border-radius: 10px;
}
#annotationToolBar QToolButton#colorSwatch:checked {
    border: 2px solid #0078D4;
}
#annotationToolBar QToolButton#widthSwatch {
    padding: 0px;
    border: 1px solid rgba(0, 0, 0, 0.2);
    border-radius: 14px;
    color: #333333;
}
#annotationToolBar QToolButton#widthSwatch:checked {
    border: 2px solid #0078D4;
}
#annotationToolBar QCheckBox {
    spacing: 6px;
}
)"));

    // 工具按钮互斥组：同一时刻仅允许一个工具处于选中态
    m_toolButtonGroup = new QButtonGroup(this);
    m_toolButtonGroup->setExclusive(true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN);
    rootLayout->setSpacing(G_PANEL_SPACING);

    // ---- 上部工具按钮列（竖排） ----
    rootLayout->addWidget(createToolButton(G_EMOJI_SELECT,      tr("选择/移动"), Tool::Select));
    rootLayout->addWidget(createToolButton(G_EMOJI_PEN,         tr("画笔"),   Tool::Pen));
    rootLayout->addWidget(createToolButton(G_EMOJI_HIGHLIGHTER, tr("荧光笔"), Tool::Highlighter));
    rootLayout->addWidget(createToolButton(G_EMOJI_LINE,        tr("直线"),   Tool::Line));
    rootLayout->addWidget(createToolButton(G_EMOJI_ARROW,       tr("箭头"),   Tool::Arrow));
    rootLayout->addWidget(createToolButton(G_EMOJI_RECTANGLE,   tr("矩形"),   Tool::Rectangle));
    rootLayout->addWidget(createToolButton(G_EMOJI_ELLIPSE,     tr("椭圆"),   Tool::Ellipse));
    rootLayout->addWidget(createToolButton(G_EMOJI_TEXT,        tr("文字"),   Tool::Text));
    rootLayout->addWidget(createToolButton(G_EMOJI_MOSAIC,      tr("马赛克"), Tool::Mosaic));

    // ---- 下部动态属性面板（stretch 占剩余空间） ----
    m_propertyStack = new QStackedWidget(this);
    m_propertyStack->setObjectName(QStringLiteral("propertyStack"));
    rootLayout->addWidget(m_propertyStack, 1);

    setupPropertyPages();
}

QToolButton* AnnotationToolBar::createToolButton(const QString& emojiText,
                                                 const QString& tooltip,
                                                 SK::Tool tool)
{
    auto* toolButton = new QToolButton(this);
    // TODO: 替换为图标资源，暂用 emoji 占位
    toolButton->setText(emojiText);
    toolButton->setToolTip(tooltip);
    toolButton->setCheckable(true);
    toolButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolButton->setObjectName(QStringLiteral("annotationToolButton"));
    toolButton->setAutoRaise(true);
    toolButton->setFixedSize(G_TOOL_BUTTON_SIZE, G_TOOL_BUTTON_SIZE);

    // 使用 emoji 字体族渲染，保证占位符号在 Windows 上的显示效果
    QFont emojiFont = toolButton->font();
    emojiFont.setFamily(QStringLiteral("Segoe UI Emoji"));
    emojiFont.setPointSize(G_TOOL_EMOJI_POINT_SIZE);
    toolButton->setFont(emojiFont);

    // 以 Tool 枚举值作为组内 id，便于 setCurrentTool 反查按钮
    m_toolButtonGroup->addButton(toolButton, static_cast<int>(tool));

    connect(toolButton, &QAbstractButton::clicked, this,
            [this, tool]()
    {
        onToolButtonClicked(tool);
    });
    return toolButton;
}

void AnnotationToolBar::setupPropertyPages()
{
    // 页面顺序与 Tool 枚举下标严格一致，setCurrentTool 可直接按枚举值切页
    m_propertyStack->addWidget(createEmptyPage());                                  // Select
    m_propertyStack->addWidget(createStrokePage(G_PEN_WIDTH_STEPS, &m_rectFillCheck));       // Rectangle
    m_propertyStack->addWidget(createStrokePage(G_PEN_WIDTH_STEPS, &m_ellipseFillCheck));    // Ellipse
    m_propertyStack->addWidget(createStrokePage(G_PEN_WIDTH_STEPS, nullptr));                 // Arrow
    m_propertyStack->addWidget(createStrokePage(G_PEN_WIDTH_STEPS, nullptr));                 // Line
    m_propertyStack->addWidget(createStrokePage(G_PEN_WIDTH_STEPS, nullptr));                 // Pen
    m_propertyStack->addWidget(createStrokePage(G_HIGHLIGHT_WIDTH_STEPS, nullptr));           // Highlighter
    m_propertyStack->addWidget(createStrokePage(G_MOSAIC_WIDTH_STEPS, nullptr));              // Mosaic
    m_propertyStack->addWidget(createTextPage());                                            // Text

    // 初始无工具选中：显示选择页（空白页），避免空面板崩溃
    m_propertyStack->setCurrentIndex(static_cast<int>(Tool::Select));
}

QWidget* AnnotationToolBar::createEmptyPage()
{
    // 选择工具无属性控件，仅展示操作提示，避免纯空白面板
    auto* pageWidget = new QWidget(m_propertyStack);
    auto* pageLayout = new QVBoxLayout(pageWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    auto* hintLabel = new QLabel(tr("选择模式：点击图元选中，拖动移动位置"), pageWidget);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #666666; font-size: 12px;"));
    pageLayout->addWidget(hintLabel);
    return pageWidget;
}

QWidget* AnnotationToolBar::createStrokePage(const QVector<qreal>& widthSteps,
                                             QCheckBox** fillCheckOut)
{
    auto* pageWidget = new QWidget(m_propertyStack);
    auto* pageLayout = new QVBoxLayout(pageWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(G_PANEL_SPACING);

    pageLayout->addWidget(createColorRow(pageWidget));
    pageLayout->addWidget(createWidthRow(pageWidget, widthSteps));

    // 矩形/椭圆页额外提供填充勾选（通过输出参数回传勾选框指针供状态同步）
    if (fillCheckOut != nullptr)
    {
        auto* fillCheck = new QCheckBox(tr("填充"), pageWidget);
        fillCheck->setObjectName(QStringLiteral("fillCheck"));
        connect(fillCheck, &QCheckBox::toggled, this, &AnnotationToolBar::onFillToggled);
        pageLayout->addWidget(fillCheck);
        *fillCheckOut = fillCheck;
    }
    return pageWidget;
}

QWidget* AnnotationToolBar::createTextPage()
{
    auto* pageWidget = new QWidget(m_propertyStack);
    auto* pageLayout = new QVBoxLayout(pageWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(G_PANEL_SPACING);

    pageLayout->addWidget(createColorRow(pageWidget));

    // 字号行：取值范围按边界常量 G_MIN/MAX_FONT_SIZE
    auto* sizeRow = new QHBoxLayout;
    sizeRow->setSpacing(G_PANEL_SPACING);
    sizeRow->addWidget(new QLabel(tr("字号"), pageWidget));
    auto* fontSizeSpin = new QSpinBox(pageWidget);
    fontSizeSpin->setObjectName(QStringLiteral("fontSizeSpin"));
    fontSizeSpin->setRange(static_cast<int>(G_MIN_FONT_SIZE), static_cast<int>(G_MAX_FONT_SIZE));
    sizeRow->addWidget(fontSizeSpin, 1);
    pageLayout->addLayout(sizeRow);

    // 字体族行
    auto* fontRow = new QHBoxLayout;
    fontRow->setSpacing(G_PANEL_SPACING);
    fontRow->addWidget(new QLabel(tr("字体"), pageWidget));
    auto* fontCombo = new QFontComboBox(pageWidget);
    fontCombo->setObjectName(QStringLiteral("fontCombo"));
    fontRow->addWidget(fontCombo, 1);
    pageLayout->addLayout(fontRow);

    // 先设置默认值再连接信号，避免构造阶段误发信号
    fontSizeSpin->setValue(static_cast<int>(G_DEFAULT_FONT_SIZE));
    fontCombo->setCurrentFont(QFont(G_DEFAULT_FONT_FAMILY));

    connect(fontSizeSpin, &QSpinBox::valueChanged, this,
            [this](int value)
    {
        Q_EMIT fontSizeChanged(static_cast<qreal>(value));
    });
    connect(fontCombo, &QFontComboBox::currentFontChanged, this,
            [this](const QFont& font)
    {
        Q_EMIT fontFamilyChanged(font.family());
    });

    return pageWidget;
}

QWidget* AnnotationToolBar::createColorRow(QWidget* parent)
{
    auto* rowWidget = new QWidget(parent);
    auto* colorLayout = new QHBoxLayout(rowWidget);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(4);

    // 色板互斥：同一时刻仅高亮一个颜色
    auto* colorGroup = new QButtonGroup(rowWidget);
    colorGroup->setExclusive(true);

    // 遍历全局预设色板，为每个颜色生成一个可勾选的色块按钮
    for (int colorIndex = 0; colorIndex < SK::G_COLOR_PALETTE.size(); ++colorIndex)
    {
        const QColor paletteColor = SK::G_COLOR_PALETTE.at(colorIndex);
        auto* colorButton = new QToolButton(rowWidget);
        colorButton->setCheckable(true);
        colorButton->setObjectName(QStringLiteral("colorSwatch"));
        colorButton->setFixedSize(G_COLOR_SWATCH_SIZE, G_COLOR_SWATCH_SIZE);
        colorButton->setToolTip(paletteColor.name());
        // 色块背景色由按钮级样式表填充（QToolButton 无内置背景色属性）
        colorButton->setStyleSheet(
            QStringLiteral("background-color: %1;").arg(paletteColor.name()));
        colorGroup->addButton(colorButton, colorIndex);
        colorLayout->addWidget(colorButton);

        if (colorIndex == 0)
        {
            colorButton->setChecked(true);   // 默认选中色板第一个颜色
        }
        connect(colorButton, &QAbstractButton::clicked, this,
                [this, paletteColor]()
        {
            Q_EMIT penColorChanged(paletteColor);
        });
    }
    return rowWidget;
}

QWidget* AnnotationToolBar::createWidthRow(QWidget* parent, const QVector<qreal>& widthSteps)
{
    auto* rowWidget = new QWidget(parent);
    auto* widthLayout = new QHBoxLayout(rowWidget);
    widthLayout->setContentsMargins(0, 0, 0, 0);
    widthLayout->setSpacing(4);

    // 档位互斥：同一时刻仅高亮一个粗细
    auto* widthGroup = new QButtonGroup(rowWidget);
    widthGroup->setExclusive(true);

    for (int stepIndex = 0; stepIndex < widthSteps.size(); ++stepIndex)
    {
        const qreal stepValue = widthSteps.at(stepIndex);
        auto* widthButton = new QToolButton(rowWidget);
        widthButton->setCheckable(true);
        widthButton->setObjectName(QStringLiteral("widthSwatch"));
        widthButton->setFixedSize(G_WIDTH_SWATCH_SIZE, G_WIDTH_SWATCH_SIZE);
        widthButton->setToolTip(QStringLiteral("%1 px").arg(stepValue));
        widthGroup->addButton(widthButton, stepIndex);
        widthLayout->addWidget(widthButton);

        // 按钮文本为圆点，字号随档位值线性放大，直观呈现粗细差异
        const int dotFontSize = qBound(G_MIN_DOT_FONT_SIZE,
                                       qRound(stepValue * G_DOT_FONT_SCALE),
                                       G_MAX_DOT_FONT_SIZE);
        widthButton->setText(QStringLiteral("●"));
        widthButton->setStyleSheet(
            QStringLiteral("font-size: %1px; color: #333333;").arg(dotFontSize));

        if (stepIndex == 0)
        {
            widthButton->setChecked(true);   // 默认选中第一个档位
        }
        connect(widthButton, &QAbstractButton::clicked, this,
                [this, stepValue]()
        {
            Q_EMIT penWidthChanged(stepValue);
        });
    }
    return rowWidget;
}

void AnnotationToolBar::setCurrentTool(SK::Tool tool)
{
    m_currentTool = tool;

    // 高亮对应工具按钮（Select 已有独立按钮，统一对所有工具高亮）
    QAbstractButton* targetButton = m_toolButtonGroup->button(static_cast<int>(tool));
    if (targetButton != nullptr)
    {
        targetButton->setChecked(true);
    }

    // 切换到对应属性页（页面 index 与 Tool 枚举一致）
    const int pageIndex = static_cast<int>(tool);
    if ((pageIndex >= 0) && (pageIndex < m_propertyStack->count()))
    {
        m_propertyStack->setCurrentIndex(pageIndex);
    }

    // 切页后同步填充勾选框状态，保证矩形/椭圆页表现一致
    syncFillCheckState();

    // 同步场景工具：外部 setCurrentTool 与按钮点击统一经此信号分发
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::onToolButtonClicked(SK::Tool tool)
{
    setCurrentTool(tool);
}

void AnnotationToolBar::onFillToggled(bool checked)
{
    m_fillChecked = checked;
    // 同步兄弟页的勾选框，避免矩形/椭圆页填充状态不一致
    syncFillCheckState();
    Q_EMIT brushStyleChanged(checked ? Qt::SolidPattern : Qt::NoBrush);
}

void AnnotationToolBar::syncFillCheckState()
{
    QCheckBox* currentFillCheck = nullptr;
    if (m_currentTool == Tool::Rectangle)
    {
        currentFillCheck = m_rectFillCheck;
    }
    else if (m_currentTool == Tool::Ellipse)
    {
        currentFillCheck = m_ellipseFillCheck;
    }

    if ((currentFillCheck != nullptr) && (currentFillCheck->isChecked() != m_fillChecked))
    {
        // 屏蔽信号防止触发 onFillToggled 造成递归
        QSignalBlocker stateBlocker(currentFillCheck);
        currentFillCheck->setChecked(m_fillChecked);
    }
}

} // namespace SK