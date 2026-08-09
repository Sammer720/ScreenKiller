/**
 * \file AnnotationToolBar.cpp
 * \brief AnnotationToolBar 实现：手风琴工具按钮、参数区与信号发射
 *
 * 实现要点：
 *   1. 背景不依赖 QSS，直接在 paintEvent 自绘半透明圆角暖色矩形
 *      （WA_TranslucentBackground + QPainterPath），与 GuidePanel 同设计语言。
 *   2. 工具按钮用 SK::ToolButton（纯文本 TextOnly 模式回退基类自绘），
 *      emoji 占位文本 + Segoe UI Emoji 字体族，待图标资源替换。
 *   3. 手风琴参数区：每个按钮正下方内联一个参数 QWidget，初始 hidden，
 *      setCurrentTool 时仅展开当前工具的参数区，其余收起。
 *   4. 构造期不发射任何信号：色板/粗细/字号/字体均先设置默认值再 connect。
 */
#include "AnnotationToolBar.h"

#include "annotation/AnnotationConstants.h"
#include "sub_widget/ToolButton.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFont>
#include <QFontComboBox>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QtGlobal>
#include <QVBoxLayout>

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
constexpr int G_WIDTH_SWATCH_SIZE = 26;
/// 色块/粗细按钮间距（像素）
constexpr int G_SWATCH_SPACING = 4;
/// 粗细圆点字号下限（像素）
constexpr int G_MIN_DOT_FONT_SIZE = 6;
/// 粗细圆点字号上限（像素）
constexpr int G_MAX_DOT_FONT_SIZE = 24;
/// 粗细圆点字号放大系数（档位值 × 系数 = 圆点字号）
constexpr qreal G_DOT_FONT_SCALE = 0.6;

// ============================ 背景色（与 GuidePanel 同设计语言） ============================
// 与 GuidePanel.cpp 的 G_BG_* 保持一致，保证两个悬浮面板视觉风格统一
constexpr int G_BG_R = 255;   // #FFE4B5 柔和暖黄
constexpr int G_BG_G = 228;
constexpr int G_BG_B = 181;
constexpr int G_BG_A = 180;   // ~70% 不透明度
/// 圆角半径（像素）
constexpr qreal G_CORNER_RADIUS = 10.0;

// ============================ 工具按钮 emoji 占位 ============================
// TODO: 替换为图标资源，暂用 emoji 占位
const QString G_EMOJI_PEN         = QStringLiteral("✏️");   ///< 画笔按钮占位文本
const QString G_EMOJI_HIGHLIGHTER = QStringLiteral("🖍️");   ///< 荧光笔按钮占位文本
const QString G_EMOJI_LINE        = QStringLiteral("📏");   ///< 直线按钮占位文本
const QString G_EMOJI_ARROW       = QStringLiteral("➡️");   ///< 箭头按钮占位文本
const QString G_EMOJI_RECTANGLE   = QStringLiteral("🟦");   ///< 矩形按钮占位文本
const QString G_EMOJI_ELLIPSE     = QStringLiteral("⭕");   ///< 椭圆按钮占位文本
const QString G_EMOJI_TEXT        = QStringLiteral("🅰️");   ///< 文字按钮占位文本
const QString G_EMOJI_MOSAIC      = QStringLiteral("🔲");   ///< 马赛克按钮占位文本

// ============================ 粗细档位预设 ============================
// 档位取值已按各工具边界常量（G_MIN/MAX_*_WIDTH）预置于合法范围内，点击无需再 clamp
const QVector<qreal> G_PEN_WIDTH_STEPS       = { 2.0, 4.0, 8.0, 12.0, 20.0 };     ///< 画笔/几何（矩形/椭圆/直线/箭头）
const QVector<qreal> G_HIGHLIGHT_WIDTH_STEPS = { 10.0, 15.0, 20.0, 30.0, 40.0 }; ///< 荧光笔
const QVector<qreal> G_MOSAIC_WIDTH_STEPS    = { 15.0, 25.0, 40.0, 50.0, 60.0 }; ///< 马赛克

// ============================ 文字属性默认值 ============================
constexpr qreal G_DEFAULT_FONT_SIZE = 12.0;   ///< 默认字号（pt，与 AnnotationScene 默认一致）
const QString G_DEFAULT_FONT_FAMILY = QStringLiteral("微软雅黑");  ///< 默认字体族

// ============================ 工具按钮装配表 ============================
/// @brief 工具按钮装配条目：emoji 占位文本 + 对应工具枚举
struct ToolButtonEntry
{
    QString emoji;   ///< emoji 占位文本
    SK::Tool tool;   ///< 工具类型
};

/// @brief 手风琴工具按钮顺序（按任务指定：无 Select 工具）
const QVector<ToolButtonEntry> G_TOOL_BUTTON_ENTRIES = {
    { G_EMOJI_PEN,         SK::Tool::Pen },
    { G_EMOJI_HIGHLIGHTER, SK::Tool::Highlighter },
    { G_EMOJI_LINE,        SK::Tool::Line },
    { G_EMOJI_ARROW,       SK::Tool::Arrow },
    { G_EMOJI_RECTANGLE,   SK::Tool::Rectangle },
    { G_EMOJI_ELLIPSE,     SK::Tool::Ellipse },
    { G_EMOJI_TEXT,        SK::Tool::Text },
    { G_EMOJI_MOSAIC,      SK::Tool::Mosaic },
};

} // namespace

AnnotationToolBar::AnnotationToolBar(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AnnotationToolBar::setupUi()
{
    setObjectName(QStringLiteral("annotationToolBar"));
    // 半透明暖色圆角背景由 paintEvent 自绘，必须启用透明背景属性
    setAttribute(Qt::WA_TranslucentBackground, true);
    // 固定宽度与 MainWindow 定位共享同一常量，避免构造/定位宽度不一致
    setFixedWidth(SK::G_ANN_TOOLBAR_WIDTH);
    // 高度不固定：手风琴参数区随展开变化，由布局 sizeHint 与外层 setGeometry 共同决定

    // 仅定义子控件交互样式（按钮 hover/checked、色块/粗细块边框）；
    // 根对象背景不在此定义，避免破坏 paintEvent 自绘的圆角
    setStyleSheet(QStringLiteral(R"(
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

    // 手风琴装配：按表顺序「按钮 + 参数区」内联排布，参数区初始隐藏
    for (const ToolButtonEntry& toolEntry : G_TOOL_BUTTON_ENTRIES)
    {
        rootLayout->addWidget(createToolButton(toolEntry.emoji, toolEntry.tool));
        rootLayout->addWidget(createParamWidget(toolEntry.tool));
    }

    // stretch 吸收面板底部剩余空间：参数区展开时紧贴按钮下方，收起时留白
    rootLayout->addStretch();

    // 初始状态：默认锁定画笔工具并展开其参数区。
    // 构造期不调用 setCurrentTool，避免误发 toolChanged；（截屏完成时外部会再同步一次）
    QAbstractButton* penButton = m_toolButtonGroup->button(static_cast<int>(SK::Tool::Pen));
    if (penButton != nullptr)
    {
        penButton->setChecked(true);
    }
    QWidget* penParamWidget = m_paramWidgets.value(static_cast<int>(SK::Tool::Pen));
    if (penParamWidget != nullptr)
    {
        penParamWidget->setVisible(true);
    }
}

SK::ToolButton* AnnotationToolBar::createToolButton(const QString& emojiText,
                                                    SK::Tool tool)
{
    auto* toolButton = new SK::ToolButton(this);
    // TODO: 替换为图标资源，暂用 emoji 占位
    toolButton->setText(emojiText);
    toolButton->setCheckable(true);
    // 纯文本回退基类绘制：ToolButton 在 TextOnly 模式下完全交由 QToolButton 自绘
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

QWidget* AnnotationToolBar::createParamWidget(SK::Tool tool)
{
    // 各工具参数区独立创建，产出后统一注册到 m_paramWidgets 供手风琴切换
    QWidget* paramWidget = nullptr;
    switch (tool)
    {
    case SK::Tool::Pen:
    case SK::Tool::Line:
    case SK::Tool::Arrow:
        // 画笔/直线/箭头：色板 + 常规粗细档位
        paramWidget = createStrokeParam(this, G_PEN_WIDTH_STEPS, nullptr);
        break;
    case SK::Tool::Highlighter:
        // 荧光笔：色板 + 加粗档位
        paramWidget = createStrokeParam(this, G_HIGHLIGHT_WIDTH_STEPS, nullptr);
        break;
    case SK::Tool::Rectangle:
        // 矩形：色板 + 常规粗细档位 + 填充勾选
        paramWidget = createStrokeParam(this, G_PEN_WIDTH_STEPS, &m_rectFillCheck);
        break;
    case SK::Tool::Ellipse:
        // 椭圆：色板 + 常规粗细档位 + 填充勾选
        paramWidget = createStrokeParam(this, G_PEN_WIDTH_STEPS, &m_ellipseFillCheck);
        break;
    case SK::Tool::Text:
        // 文字：色板 + 字号 + 字体族
        paramWidget = createTextParam(this);
        break;
    case SK::Tool::Mosaic:
        // 马赛克：仅粗细档位（取背景色，无颜色设置）
        paramWidget = createMosaicParam(this);
        break;
    default:
        // Select 等无参数区工具：防御性兜底，本工具栏不提供此类按钮
        paramWidget = new QWidget(this);
        paramWidget->hide();
        break;
    }

    m_paramWidgets.insert(static_cast<int>(tool), paramWidget);
    return paramWidget;
}

QWidget* AnnotationToolBar::createStrokeParam(QWidget* parent,
                                              const QVector<qreal>& widthSteps,
                                              QCheckBox** fillCheckOut)
{
    auto* paramWidget = new QWidget(parent);
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 色板行 + 粗细档位行
    paramLayout->addWidget(createColorRow(paramWidget));
    paramLayout->addWidget(createWidthRow(paramWidget, widthSteps));

    // 矩形/椭圆额外提供填充勾选（通过输出参数回传勾选框指针供状态同步）
    if (fillCheckOut != nullptr)
    {
        auto* fillCheck = new QCheckBox(tr("填充"), paramWidget);
        fillCheck->setObjectName(QStringLiteral("fillCheck"));
        connect(fillCheck, &QCheckBox::toggled, this, &AnnotationToolBar::onFillToggled);
        paramLayout->addWidget(fillCheck);
        *fillCheckOut = fillCheck;
    }

    // 手风琴：初始收起，由 setCurrentTool 决定何时展开
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createTextParam(QWidget* parent)
{
    auto* paramWidget = new QWidget(parent);
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 色板行
    paramLayout->addWidget(createColorRow(paramWidget));

    // 字号行：取值范围按边界常量 G_MIN/MAX_FONT_SIZE
    auto* sizeRow = new QHBoxLayout;
    sizeRow->setSpacing(G_PANEL_SPACING);
    sizeRow->addWidget(new QLabel(tr("字号"), paramWidget));
    auto* fontSizeSpin = new QSpinBox(paramWidget);
    fontSizeSpin->setObjectName(QStringLiteral("fontSizeSpin"));
    fontSizeSpin->setRange(static_cast<int>(SK::G_MIN_FONT_SIZE), static_cast<int>(SK::G_MAX_FONT_SIZE));
    sizeRow->addWidget(fontSizeSpin, 1);
    paramLayout->addLayout(sizeRow);

    // 字体族行
    auto* fontRow = new QHBoxLayout;
    fontRow->setSpacing(G_PANEL_SPACING);
    fontRow->addWidget(new QLabel(tr("字体"), paramWidget));
    auto* fontCombo = new QFontComboBox(paramWidget);
    fontCombo->setObjectName(QStringLiteral("fontCombo"));
    fontRow->addWidget(fontCombo, 1);
    paramLayout->addLayout(fontRow);

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

    // 手风琴：初始收起，由 setCurrentTool 决定何时展开
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createMosaicParam(QWidget* parent)
{
    auto* paramWidget = new QWidget(parent);
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 马赛克取背景色，无颜色设置，仅提供粗细档位
    paramLayout->addWidget(createWidthRow(paramWidget, G_MOSAIC_WIDTH_STEPS));

    // 手风琴：初始收起，由 setCurrentTool 决定何时展开
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createColorRow(QWidget* parent)
{
    auto* rowWidget = new QWidget(parent);
    auto* colorLayout = new QHBoxLayout(rowWidget);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(G_SWATCH_SPACING);

    // 色板互斥：同一时刻仅高亮一个颜色
    auto* colorGroup = new QButtonGroup(rowWidget);
    colorGroup->setExclusive(true);

    // 遍历全局预设色板，为每个颜色生成一个可勾选的色块按钮
    for (int colorIndex = 0; colorIndex < SK::G_COLOR_PALETTE.size(); ++colorIndex)
    {
        const QColor paletteColor = SK::G_COLOR_PALETTE.at(colorIndex);
        auto* colorButton = new QToolButton(rowWidget);
        auto colorName = paletteColor.name();

        colorButton->setCheckable(true);
        colorButton->setObjectName(QStringLiteral("colorSwatch"));
        colorButton->setFixedSize(G_COLOR_SWATCH_SIZE, G_COLOR_SWATCH_SIZE);
        colorButton->setToolTip(colorName);
        // 色块背景色由按钮级样式表填充（QToolButton 无内置背景色属性）
        colorButton->setStyleSheet(
            QStringLiteral("background-color: %1;").arg(colorName));
        colorGroup->addButton(colorButton, colorIndex);
        colorLayout->addWidget(colorButton);

        // 先勾选默认色再连接信号，构造期不发射 penColorChanged
        if (colorIndex == 0)
        {
            colorButton->setChecked(true);
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
    widthLayout->setSpacing(G_SWATCH_SPACING);

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

        // 先勾选默认档位再连接信号，构造期不发射 penWidthChanged
        if (stepIndex == 0)
        {
            widthButton->setChecked(true);
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

    // 高亮对应工具按钮（枚举值与 QButtonGroup id 一致）
    QAbstractButton* targetButton = m_toolButtonGroup->button(static_cast<int>(tool));
    if (targetButton != nullptr)
    {
        targetButton->setChecked(true);
    }

    // 手风琴：仅展开当前工具的参数区，其余一律收起
    const int currentToolKey = static_cast<int>(tool);
    for (auto paramIter = m_paramWidgets.constBegin(); paramIter != m_paramWidgets.constEnd(); ++paramIter)
    {
        paramIter.value()->setVisible(paramIter.key() == currentToolKey);
    }

    // 切页后同步填充勾选框状态，保证矩形/椭圆两页表现一致
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
    if (m_currentTool == SK::Tool::Rectangle)
    {
        currentFillCheck = m_rectFillCheck;
    }
    else if (m_currentTool == SK::Tool::Ellipse)
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

void AnnotationToolBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 自绘半透明圆角背景：与 GuidePanel 同色系（#FFE4B5 + 70% 不透明度），保持风格统一
    QPainterPath path;
    path.addRoundedRect(rect(), G_CORNER_RADIUS, G_CORNER_RADIUS);
    painter.fillPath(path, QColor(G_BG_R, G_BG_G, G_BG_B, G_BG_A));
}

} // namespace SK