/**
 * \file AnnotationToolBar.cpp
 * \brief AnnotationToolBar v3 实现：三级层级工具栏、参数区与信号发射
 *
 * 实现要点：
 *   1. 背景不依赖 QSS，直接在 paintEvent 自绘半透明圆角暖色矩形
 *      （WA_TranslucentBackground + QPainterPath），与 GuidePanel 同设计语言。
 *   2. 一级/二级按钮用 SK::ToolButton + IconOnly 单图标（无三态变体），
 *      高亮/悬停由 QSS 的 :checked/:hover 规则驱动。
 *   3. 展开/收起全部用 QWidget 容器 + setVisible 控制，不使用 QStackedWidget：
 *      一级仅水笔/荧光笔/文字/马赛克带参数区；几何展开二级行 + 当前图形三级参数。
 *   4. QSettings（默认构造，INI 格式）持久化：构造期读入初始化各控件状态，
 *      用户调整（选色/拖滑块/勾填充/改字体）即写回。
 *   5. 构造期不发射任何信号：先设置默认值再 connect，避免误发。
 */
#include "AnnotationToolBar.h"

#include "annotation/AnnotationConstants.h"
#include "sub_widget/ToolButton.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFont>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>  // std::as_const

namespace SK {

namespace {

// ============================ 布局常量 ============================
/// 面板内边距（像素）
constexpr int G_PANEL_MARGIN = 8;
/// 面板内控件间距（像素）
constexpr int G_PANEL_SPACING = 6;
/// 一级/二级工具按钮尺寸（像素）
constexpr int G_TOOL_BUTTON_SIZE = 36;
/// 工具按钮图标尺寸（像素，图标资源为 32x32，略缩以留出高亮底衬）
constexpr int G_TOOL_ICON_SIZE = 26;
/// 色板色块按钮尺寸（像素）
constexpr int G_COLOR_SWATCH_SIZE = 20;
/// 色块间距（像素）
constexpr int G_SWATCH_SPACING = 4;
/// 几何二级行内按钮间距（像素，4 x 36 按钮需收紧以装入固定面板宽度）
constexpr int G_GEOMETRY_ROW_SPACING = 2;
/// 滑块数值标签固定宽度（像素，避免位数变化导致滑块跳动）
constexpr int G_SLIDER_VALUE_LABEL_WIDTH = 24;
/// 几何一级按钮专用互斥组 id（Tool 枚举无 Geometry 值，几何是工具栏内部一级分组）
constexpr int G_GEOMETRY_BUTTON_ID = 100;

// ============================ 背景色（与 GuidePanel 同设计语言） ============================
// 与 GuidePanel.cpp 的 G_BG_* 保持一致，保证两个悬浮面板视觉风格统一
constexpr int G_BG_R = 255;   // #FFE4B5 柔和暖黄
constexpr int G_BG_G = 228;
constexpr int G_BG_B = 181;
constexpr int G_BG_A = 180;   // ~70% 不透明度
/// 圆角半径（像素）
constexpr qreal G_CORNER_RADIUS = 10.0;

// ============================ 图标资源 ============================
const QString G_ICON_PEN         = QStringLiteral(":/icons/pen.png");         ///< 水笔图标
const QString G_ICON_HIGHLIGHTER = QStringLiteral(":/icons/highlighter.png"); ///< 荧光笔图标
const QString G_ICON_GEOMETRY    = QStringLiteral(":/icons/geometry.png");    ///< 几何图标
const QString G_ICON_LINE        = QStringLiteral(":/icons/line.png");        ///< 直线图标
const QString G_ICON_ARROW       = QStringLiteral(":/icons/arrow.png");       ///< 箭头图标
const QString G_ICON_SQUARE      = QStringLiteral(":/icons/square.png");      ///< 方框图标
const QString G_ICON_CIRCLE      = QStringLiteral(":/icons/circle.png");      ///< 圆图标
const QString G_ICON_TEXT        = QStringLiteral(":/icons/text.png");        ///< 文字图标
const QString G_ICON_MOSAIC      = QStringLiteral(":/icons/mosaic.png");      ///< 马赛克图标

// ============================ QSettings 持久化键（annotation/ 前缀） ============================
const QString G_KEY_DEFAULT_TOOL      = QStringLiteral("annotation/defaultTool");    ///< 默认一级工具
const QString G_KEY_DEFAULT_GEOMETRY  = QStringLiteral("annotation/defaultGeometry");///< 默认几何图形
const QString G_KEY_PEN_COLOR         = QStringLiteral("annotation/pen/color");      ///< 水笔颜色
const QString G_KEY_PEN_WIDTH         = QStringLiteral("annotation/pen/width");      ///< 水笔宽度
const QString G_KEY_HL_COLOR          = QStringLiteral("annotation/highlighter/color"); ///< 荧光笔颜色
const QString G_KEY_HL_WIDTH          = QStringLiteral("annotation/highlighter/width"); ///< 荧光笔宽度
const QString G_KEY_LINE_COLOR        = QStringLiteral("annotation/line/color");     ///< 直线颜色
const QString G_KEY_LINE_WIDTH        = QStringLiteral("annotation/line/width");     ///< 直线宽度
const QString G_KEY_ARROW_COLOR       = QStringLiteral("annotation/arrow/color");    ///< 箭头颜色
const QString G_KEY_ARROW_WIDTH       = QStringLiteral("annotation/arrow/width");    ///< 箭头宽度
const QString G_KEY_RECT_COLOR        = QStringLiteral("annotation/rect/color");     ///< 方框颜色
const QString G_KEY_RECT_WIDTH        = QStringLiteral("annotation/rect/width");     ///< 方框宽度
const QString G_KEY_RECT_FILLED       = QStringLiteral("annotation/rect/filled");    ///< 方框填充开关
const QString G_KEY_ELLIPSE_COLOR     = QStringLiteral("annotation/ellipse/color");  ///< 圆颜色
const QString G_KEY_ELLIPSE_WIDTH     = QStringLiteral("annotation/ellipse/width");  ///< 圆宽度
const QString G_KEY_ELLIPSE_FILLED    = QStringLiteral("annotation/ellipse/filled"); ///< 圆填充开关
const QString G_KEY_TEXT_COLOR        = QStringLiteral("annotation/text/color");      ///< 文字颜色
const QString G_KEY_TEXT_FONT_SIZE    = QStringLiteral("annotation/text/fontSize");   ///< 文字字号
const QString G_KEY_TEXT_FONT_FAMILY  = QStringLiteral("annotation/text/fontFamily"); ///< 文字字体族
const QString G_KEY_MOSAIC_WIDTH      = QStringLiteral("annotation/mosaic/width");    ///< 马赛克宽度

// ============================ 默认值（首次使用落值） ============================
constexpr int G_DEFAULT_PEN_WIDTH     = 2;    ///< 水笔/直线/箭头/方框/圆默认宽度
constexpr int G_DEFAULT_HL_WIDTH      = 18;   ///< 荧光笔默认宽度
constexpr int G_DEFAULT_FONT_SIZE     = 12;   ///< 文字默认字号（pt）
constexpr int G_DEFAULT_MOSAIC_WIDTH  = 20;   ///< 马赛克默认宽度
const QString G_DEFAULT_FONT_FAMILY   = QStringLiteral("微软雅黑");  ///< 文字默认字体族
// 描边类颜色默认值取各色板首色（标注色板首色 #FF0000，荧光笔色板首色 #FFEB3B）

// ============================ 描边类参数区装配规格 ============================
/// @brief 水笔参数规格：标注色板 + 1~30 宽度
const AnnotationToolBar::StrokeParamSpec G_PEN_SPEC = {
    &SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_PEN_COLOR, G_KEY_PEN_WIDTH,
    static_cast<int>(SK::G_MIN_PEN_WIDTH), static_cast<int>(SK::G_MAX_PEN_WIDTH),
    G_DEFAULT_PEN_WIDTH, false, QString()
};
/// @brief 荧光笔参数规格：荧光笔色板 + 5~40 宽度
const AnnotationToolBar::StrokeParamSpec G_HIGHLIGHTER_SPEC = {
    &SK::G_HIGHLIGHTER_COLOR_PALETTE, G_KEY_HL_COLOR, G_KEY_HL_WIDTH,
    static_cast<int>(SK::G_MIN_HIGHLIGHT_WIDTH), static_cast<int>(SK::G_MAX_HIGHLIGHT_WIDTH),
    G_DEFAULT_HL_WIDTH, false, QString()
};
/// @brief 直线参数规格：标注色板 + 1~30 宽度
const AnnotationToolBar::StrokeParamSpec G_LINE_SPEC = {
    &SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_LINE_COLOR, G_KEY_LINE_WIDTH,
    static_cast<int>(SK::G_MIN_PEN_WIDTH), static_cast<int>(SK::G_MAX_PEN_WIDTH),
    G_DEFAULT_PEN_WIDTH, false, QString()
};
/// @brief 箭头参数规格：标注色板 + 1~30 宽度
const AnnotationToolBar::StrokeParamSpec G_ARROW_SPEC = {
    &SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_ARROW_COLOR, G_KEY_ARROW_WIDTH,
    static_cast<int>(SK::G_MIN_PEN_WIDTH), static_cast<int>(SK::G_MAX_PEN_WIDTH),
    G_DEFAULT_PEN_WIDTH, false, QString()
};
/// @brief 方框参数规格：标注色板 + 1~30 宽度 + 填充勾选
const AnnotationToolBar::StrokeParamSpec G_RECT_SPEC = {
    &SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_RECT_COLOR, G_KEY_RECT_WIDTH,
    static_cast<int>(SK::G_MIN_PEN_WIDTH), static_cast<int>(SK::G_MAX_PEN_WIDTH),
    G_DEFAULT_PEN_WIDTH, true, G_KEY_RECT_FILLED
};
/// @brief 圆参数规格：标注色板 + 1~30 宽度 + 填充勾选
const AnnotationToolBar::StrokeParamSpec G_ELLIPSE_SPEC = {
    &SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_ELLIPSE_COLOR, G_KEY_ELLIPSE_WIDTH,
    static_cast<int>(SK::G_MIN_PEN_WIDTH), static_cast<int>(SK::G_MAX_PEN_WIDTH),
    G_DEFAULT_PEN_WIDTH, true, G_KEY_ELLIPSE_FILLED
};

// ============================ 持久化名称 → 工具枚举映射 ============================
/// @brief 一级工具名称 → 具体场景工具（未知名称及 "pen" 兜底水笔）
/// @param toolName 持久化的工具名称（pen/highlighter/text/mosaic）
/// @return 对应工具枚举
SK::Tool toolFromName(const QString& toolName)
{
    if (toolName.compare(QStringLiteral("highlighter"), Qt::CaseInsensitive) == 0)
    {
        return SK::Tool::Highlighter;
    }
    if (toolName.compare(QStringLiteral("text"), Qt::CaseInsensitive) == 0)
    {
        return SK::Tool::Text;
    }
    if (toolName.compare(QStringLiteral("mosaic"), Qt::CaseInsensitive) == 0)
    {
        return SK::Tool::Mosaic;
    }
    return SK::Tool::Pen;
}

/// @brief 几何图形名称 → 工具枚举（未知名称及 "line" 兜底直线）
/// @param shapeName 持久化的图形名称（line/arrow/rectangle/ellipse）
/// @return 对应工具枚举
SK::Tool shapeFromName(const QString& shapeName)
{
    if (shapeName.compare(QStringLiteral("arrow"), Qt::CaseInsensitive) == 0)
    {
        return SK::Tool::Arrow;
    }
    if (shapeName.compare(QStringLiteral("rectangle"), Qt::CaseInsensitive) == 0)
    {
        return SK::Tool::Rectangle;
    }
    if (shapeName.compare(QStringLiteral("ellipse"), Qt::CaseInsensitive) == 0)
    {
        return SK::Tool::Ellipse;
    }
    return SK::Tool::Line;
}

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

    // 仅定义子控件交互样式（按钮 hover/checked、色块边框、滑块轨道/手柄）；
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
#annotationToolBar QCheckBox {
    spacing: 6px;
}
#annotationToolBar QSlider::groove:horizontal {
    height: 4px;
    background: rgba(0, 0, 0, 0.15);
    border-radius: 2px;
}
#annotationToolBar QSlider::sub-page:horizontal {
    background: #0078D4;
    border-radius: 2px;
}
#annotationToolBar QSlider::handle:horizontal {
    width: 14px;
    height: 14px;
    margin: -5px 0;
    border-radius: 7px;
    background: #0078D4;
}
#annotationToolBar QLabel#sliderValueLabel {
    color: #333333;
}
)"));

    // 配置读写：默认构造跟随 main.cpp 的 org/app 与 INI 格式
    m_settings = new QSettings(this);
    // 一级/二级互斥组：同一时刻仅允许一个工具处于选中态
    m_level1Group = new QButtonGroup(this);
    m_level1Group->setExclusive(true);
    m_level2Group = new QButtonGroup(this);
    m_level2Group->setExclusive(true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN);
    rootLayout->setSpacing(G_PANEL_SPACING);

    // ---------- 一级：水笔 ----------
    m_penButton = createLevel1Button(G_ICON_PEN, static_cast<int>(SK::Tool::Pen));
    rootLayout->addWidget(m_penButton);
    m_penParam = createStrokeParam(G_PEN_SPEC);
    rootLayout->addWidget(m_penParam);

    // ---------- 一级：荧光笔 ----------
    m_highlighterButton = createLevel1Button(G_ICON_HIGHLIGHTER, static_cast<int>(SK::Tool::Highlighter));
    rootLayout->addWidget(m_highlighterButton);
    m_highlighterParam = createStrokeParam(G_HIGHLIGHTER_SPEC);
    rootLayout->addWidget(m_highlighterParam);

    // ---------- 一级：几何（二级图形行 + 三级参数区） ----------
    m_geometryButton = createLevel1Button(G_ICON_GEOMETRY, G_GEOMETRY_BUTTON_ID);
    rootLayout->addWidget(m_geometryButton);

    m_geometryRow = new QWidget(this);
    auto* geometryRowLayout = new QHBoxLayout(m_geometryRow);
    geometryRowLayout->setContentsMargins(0, 0, 0, 0);
    geometryRowLayout->setSpacing(G_GEOMETRY_ROW_SPACING);
    geometryRowLayout->addWidget(createLevel2Button(G_ICON_LINE, SK::Tool::Line));
    geometryRowLayout->addWidget(createLevel2Button(G_ICON_ARROW, SK::Tool::Arrow));
    geometryRowLayout->addWidget(createLevel2Button(G_ICON_SQUARE, SK::Tool::Rectangle));
    geometryRowLayout->addWidget(createLevel2Button(G_ICON_CIRCLE, SK::Tool::Ellipse));
    m_geometryRow->hide();
    rootLayout->addWidget(m_geometryRow);

    // 三级参数区：每个图形独立参数区，展开时仅显示当前图形对应者
    m_shapeParams.insert(static_cast<int>(SK::Tool::Line), createStrokeParam(G_LINE_SPEC));
    m_shapeParams.insert(static_cast<int>(SK::Tool::Arrow), createStrokeParam(G_ARROW_SPEC));
    m_shapeParams.insert(static_cast<int>(SK::Tool::Rectangle), createStrokeParam(G_RECT_SPEC));
    m_shapeParams.insert(static_cast<int>(SK::Tool::Ellipse), createStrokeParam(G_ELLIPSE_SPEC));
    for (QWidget* shapeParam : std::as_const(m_shapeParams))
    {
        rootLayout->addWidget(shapeParam);
    }

    // ---------- 一级：文字 ----------
    m_textButton = createLevel1Button(G_ICON_TEXT, static_cast<int>(SK::Tool::Text));
    rootLayout->addWidget(m_textButton);
    m_textParam = createTextParam();
    rootLayout->addWidget(m_textParam);

    // ---------- 一级：马赛克 ----------
    m_mosaicButton = createLevel1Button(G_ICON_MOSAIC, static_cast<int>(SK::Tool::Mosaic));
    rootLayout->addWidget(m_mosaicButton);
    m_mosaicParam = createMosaicParam();
    rootLayout->addWidget(m_mosaicParam);

    // stretch 吸收面板底部剩余空间：参数区展开时紧贴按钮下方，收起时留白
    rootLayout->addStretch();

    // 一级/二级点击分发：idClicked 仅在用户点击时触发，程序 setChecked 不会误入
    connect(m_level1Group, &QButtonGroup::idClicked, this, &AnnotationToolBar::onLevel1Clicked);
    connect(m_level2Group, &QButtonGroup::idClicked, this,
            [this](int groupId)
    {
        onLevel2Clicked(static_cast<SK::Tool>(groupId));
    });

    // 应用持久化的默认工具（仅 UI 状态，不发射信号；截屏完成时 restoreDefaultTool 会再同步）
    const QString defaultToolName = m_settings->value(G_KEY_DEFAULT_TOOL,
                                                      QStringLiteral("pen")).toString();
    if (defaultToolName.compare(QStringLiteral("geometry"), Qt::CaseInsensitive) == 0)
    {
        const QString geometryName = m_settings->value(G_KEY_DEFAULT_GEOMETRY,
                                                       QStringLiteral("line")).toString();
        m_currentGeometryShape = shapeFromName(geometryName);
        m_currentSceneTool = m_currentGeometryShape;
        expandGeometryArea();
    }
    else
    {
        m_currentSceneTool = toolFromName(defaultToolName);
        applyToolExpansion(m_currentSceneTool);
    }
}

SK::ToolButton* AnnotationToolBar::createLevel1Button(const QString& iconResource, int groupId)
{
    auto* toolButton = new SK::ToolButton(this);
    toolButton->setIcon(QIcon(iconResource));
    toolButton->setIconSize(QSize(G_TOOL_ICON_SIZE, G_TOOL_ICON_SIZE));
    toolButton->setCheckable(true);
    // 单图标无三态变体：ToolButton 非三态时按普通按钮绘制，高亮交给 QSS
    toolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolButton->setObjectName(QStringLiteral("annotationToolButton"));
    toolButton->setAutoRaise(true);
    toolButton->setFixedSize(G_TOOL_BUTTON_SIZE, G_TOOL_BUTTON_SIZE);

    // 以 Tool 枚举值（几何用专用 id）作为组内 id，便于点击分发
    m_level1Group->addButton(toolButton, groupId);
    return toolButton;
}

SK::ToolButton* AnnotationToolBar::createLevel2Button(const QString& iconResource, SK::Tool shape)
{
    auto* toolButton = new SK::ToolButton(this);
    toolButton->setIcon(QIcon(iconResource));
    toolButton->setIconSize(QSize(G_TOOL_ICON_SIZE, G_TOOL_ICON_SIZE));
    toolButton->setCheckable(true);
    toolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolButton->setObjectName(QStringLiteral("annotationToolButton"));
    toolButton->setAutoRaise(true);
    toolButton->setFixedSize(G_TOOL_BUTTON_SIZE, G_TOOL_BUTTON_SIZE);

    // 以图形 Tool 枚举值作为组内 id，便于 onLevel2Clicked 反查
    m_level2Group->addButton(toolButton, static_cast<int>(shape));
    return toolButton;
}

QWidget* AnnotationToolBar::createColorRow(const QVector<QColor>& palette,
                                           const QString& settingsKey)
{
    auto* rowWidget = new QWidget(this);
    auto* colorLayout = new QHBoxLayout(rowWidget);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(G_SWATCH_SPACING);

    // 防御性兜底：空色板直接返回空行，避免越界访问
    if (palette.isEmpty())
    {
        return rowWidget;
    }

    // 色板互斥：同一时刻仅高亮一个颜色
    auto* colorGroup = new QButtonGroup(rowWidget);
    colorGroup->setExclusive(true);

    // 读入持久化颜色：颜色匹配该工具上次选用的色块
    const QColor storedColor = loadColor(settingsKey, palette.first());
    int checkedIndex = 0;

    for (int colorIndex = 0; colorIndex < palette.size(); ++colorIndex)
    {
        const QColor paletteColor = palette.at(colorIndex);
        const QString colorName = paletteColor.name();
        auto* colorButton = new QToolButton(rowWidget);
        colorButton->setCheckable(true);
        colorButton->setObjectName(QStringLiteral("colorSwatch"));
        colorButton->setFixedSize(G_COLOR_SWATCH_SIZE, G_COLOR_SWATCH_SIZE);
        colorButton->setToolTip(colorName);
        // 色块背景色由按钮级样式表填充（QToolButton 无内置背景色属性）
        colorButton->setStyleSheet(QStringLiteral("background-color: %1;").arg(colorName));
        colorGroup->addButton(colorButton, colorIndex);
        colorLayout->addWidget(colorButton);

        // 记录与持久化颜色一致的色块；颜色不在色板时保持勾选第一个
        if (colorName.compare(storedColor.name(), Qt::CaseInsensitive) == 0)
        {
            checkedIndex = colorIndex;
        }
        connect(colorButton, &QAbstractButton::clicked, this,
                [this, paletteColor, settingsKey]()
        {
            Q_EMIT penColorChanged(paletteColor);
            m_settings->setValue(settingsKey, paletteColor.name());
        });
    }

    // 先勾选再连接信号也安全：发射走 clicked，构造期不会误发
    QAbstractButton* checkedButton = colorGroup->button(checkedIndex);
    if (checkedButton != nullptr)
    {
        checkedButton->setChecked(true);
    }
    return rowWidget;
}

QWidget* AnnotationToolBar::createSliderRow(int minValue, int maxValue, int initialValue,
                                            QSlider** sliderOut)
{
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(G_PANEL_SPACING);

    auto* slider = new QSlider(Qt::Horizontal, rowWidget);
    slider->setObjectName(QStringLiteral("paramSlider"));
    slider->setRange(minValue, maxValue);
    slider->setValue(initialValue);

    // 数值标签无单位文字，固定宽度避免位数变化导致滑块跳动
    auto* valueLabel = new QLabel(QString::number(initialValue), rowWidget);
    valueLabel->setObjectName(QStringLiteral("sliderValueLabel"));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setFixedWidth(G_SLIDER_VALUE_LABEL_WIDTH);

    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(valueLabel);

    // 数值标签随滑块同步（以标签为上下文对象，标签销毁即自动断开）
    connect(slider, &QSlider::valueChanged, valueLabel,
            [valueLabel](int value)
    {
        valueLabel->setText(QString::number(value));
    });

    *sliderOut = slider;
    return rowWidget;
}

QWidget* AnnotationToolBar::createStrokeParam(const StrokeParamSpec& spec)
{
    auto* paramWidget = new QWidget(this);
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 颜色行（无文字标签，仅色块互斥勾选）
    paramLayout->addWidget(createColorRow(*(spec.palette), spec.colorKey));

    // 尺寸滑块行（滑块 + 右侧数值），范围按规格边界
    const int storedWidth = loadInt(spec.widthKey, spec.defaultWidth);
    QSlider* widthSlider = nullptr;
    paramLayout->addWidget(createSliderRow(spec.minWidth, spec.maxWidth, storedWidth,
                                           &widthSlider));

    // 拖动滑块：发射宽度变化 + 写回持久化
    connect(widthSlider, &QSlider::valueChanged, this,
            [this, spec](int value)
    {
        Q_EMIT penWidthChanged(static_cast<qreal>(value));
        m_settings->setValue(spec.widthKey, value);
    });

    // 方框/圆额外提供填充勾选：勾选时填充生效并禁用尺寸滑块
    if (spec.withFill)
    {
        auto* fillCheck = new QCheckBox(tr("填充"), paramWidget);
        const bool storedFilled = loadBool(spec.fillKey, false);
        fillCheck->setChecked(storedFilled);
        widthSlider->setEnabled((!storedFilled));
        connect(fillCheck, &QCheckBox::toggled, this,
                [this, spec, widthSlider](bool checked)
        {
            widthSlider->setEnabled((!checked));
            Q_EMIT brushStyleChanged(checked ? Qt::SolidPattern : Qt::NoBrush);
            m_settings->setValue(spec.fillKey, checked);
        });
        paramLayout->addWidget(fillCheck);
    }

    // 手风琴：初始收起，由展开逻辑决定何时显示
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createTextParam()
{
    auto* paramWidget = new QWidget(this);
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 字体选择（参数区上部，无标签，自带字体预览）
    auto* fontCombo = new QFontComboBox(paramWidget);
    fontCombo->setObjectName(QStringLiteral("fontCombo"));
    const QString storedFamily = m_settings->value(G_KEY_TEXT_FONT_FAMILY,
                                                   G_DEFAULT_FONT_FAMILY).toString();
    fontCombo->setCurrentFont(QFont(storedFamily));
    paramLayout->addWidget(fontCombo);

    // 颜色行（标注色板）
    paramLayout->addWidget(createColorRow(SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_TEXT_COLOR));

    // 字号滑块（8~72），范围按边界常量
    const int storedFontSize = loadInt(G_KEY_TEXT_FONT_SIZE, G_DEFAULT_FONT_SIZE);
    QSlider* fontSizeSlider = nullptr;
    paramLayout->addWidget(createSliderRow(static_cast<int>(SK::G_MIN_FONT_SIZE),
                                           static_cast<int>(SK::G_MAX_FONT_SIZE),
                                           storedFontSize, &fontSizeSlider));

    // 拖动字号滑块：发射字号变化 + 写回持久化
    connect(fontSizeSlider, &QSlider::valueChanged, this,
            [this](int value)
    {
        Q_EMIT fontSizeChanged(static_cast<qreal>(value));
        m_settings->setValue(G_KEY_TEXT_FONT_SIZE, value);
    });

    // 切换字体族：发射字体变化 + 写回持久化
    connect(fontCombo, &QFontComboBox::currentFontChanged, this,
            [this](const QFont& font)
    {
        Q_EMIT fontFamilyChanged(font.family());
        m_settings->setValue(G_KEY_TEXT_FONT_FAMILY, font.family());
    });

    // 手风琴：初始收起，由展开逻辑决定何时显示
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createMosaicParam()
{
    auto* paramWidget = new QWidget(this);
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 马赛克取背景色，无颜色设置，仅尺寸滑块（10~60）
    const int storedWidth = loadInt(G_KEY_MOSAIC_WIDTH, G_DEFAULT_MOSAIC_WIDTH);
    QSlider* mosaicSlider = nullptr;
    paramLayout->addWidget(createSliderRow(static_cast<int>(SK::G_MIN_MOSAIC_WIDTH),
                                           static_cast<int>(SK::G_MAX_MOSAIC_WIDTH),
                                           storedWidth, &mosaicSlider));

    // 拖动滑块：发射宽度变化 + 写回持久化
    connect(mosaicSlider, &QSlider::valueChanged, this,
            [this](int value)
    {
        Q_EMIT penWidthChanged(static_cast<qreal>(value));
        m_settings->setValue(G_KEY_MOSAIC_WIDTH, value);
    });

    // 手风琴：初始收起，由展开逻辑决定何时显示
    paramWidget->hide();
    return paramWidget;
}

void AnnotationToolBar::setCurrentTool(SK::Tool tool)
{
    switch (tool)
    {
    case SK::Tool::Line:
    case SK::Tool::Arrow:
    case SK::Tool::Rectangle:
    case SK::Tool::Ellipse:
        // 几何图形：记录当前图形并展开几何一级 + 二级行 + 三级参数
        m_currentGeometryShape = tool;
        m_currentSceneTool = tool;
        expandGeometryArea();
        break;
    case SK::Tool::Pen:
    case SK::Tool::Highlighter:
    case SK::Tool::Text:
    case SK::Tool::Mosaic:
        m_currentSceneTool = tool;
        applyToolExpansion(tool);
        break;
    default:
        // Select 等非工具栏工具：仅全部收起，不改变高亮与工具状态
        hideAllExpanded();
        return;
    }

    // 同步外部场景，保证场景工具与工具栏状态一致
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::collapseExpanded()
{
    // 保留一级按钮与当前工具，仅折叠二三级区域
    hideAllExpanded();
}

void AnnotationToolBar::restoreDefaultTool()
{
    const QString defaultToolName = m_settings->value(G_KEY_DEFAULT_TOOL,
                                                      QStringLiteral("pen")).toString();
    if (defaultToolName.compare(QStringLiteral("geometry"), Qt::CaseInsensitive) == 0)
    {
        // 默认工具为几何组：按持久化的默认图形展开
        const QString geometryName = m_settings->value(G_KEY_DEFAULT_GEOMETRY,
                                                       QStringLiteral("line")).toString();
        setCurrentTool(shapeFromName(geometryName));
        return;
    }
    setCurrentTool(toolFromName(defaultToolName));
}

void AnnotationToolBar::onLevel1Clicked(int groupId)
{
    if (groupId == G_GEOMETRY_BUTTON_ID)
    {
        // 几何：只展开二级行 + 当前图形三级参数，不发射 toolChanged
        expandGeometryArea();
        return;
    }

    // 具体工具：展开其参数区并通知外部切换工具
    const SK::Tool tool = static_cast<SK::Tool>(groupId);
    m_currentSceneTool = tool;
    applyToolExpansion(tool);
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::onLevel2Clicked(SK::Tool shape)
{
    m_currentGeometryShape = shape;
    m_currentSceneTool = shape;
    expandGeometryArea();
    Q_EMIT toolChanged(shape);
}

void AnnotationToolBar::applyToolExpansion(SK::Tool tool)
{
    hideAllExpanded();
    switch (tool)
    {
    case SK::Tool::Pen:
        m_penButton->setChecked(true);
        m_penParam->setVisible(true);
        break;
    case SK::Tool::Highlighter:
        m_highlighterButton->setChecked(true);
        m_highlighterParam->setVisible(true);
        break;
    case SK::Tool::Text:
        m_textButton->setChecked(true);
        m_textParam->setVisible(true);
        break;
    case SK::Tool::Mosaic:
        m_mosaicButton->setChecked(true);
        m_mosaicParam->setVisible(true);
        break;
    default:
        // 几何图形由 expandGeometryArea 负责，此处不处理
        break;
    }
}

void AnnotationToolBar::expandGeometryArea()
{
    hideAllExpanded();

    // 高亮几何一级按钮并展开二级行
    m_geometryButton->setChecked(true);
    m_geometryRow->setVisible(true);

    // 展开当前图形的三级参数区
    QWidget* shapeParam = m_shapeParams.value(static_cast<int>(m_currentGeometryShape));
    if (shapeParam != nullptr)
    {
        shapeParam->setVisible(true);
    }

    // 高亮对应的二级图形按钮
    QAbstractButton* shapeButton = m_level2Group->button(static_cast<int>(m_currentGeometryShape));
    if (shapeButton != nullptr)
    {
        shapeButton->setChecked(true);
    }
}

void AnnotationToolBar::hideAllExpanded()
{
    m_penParam->hide();
    m_highlighterParam->hide();
    m_geometryRow->hide();
    for (QWidget* shapeParam : std::as_const(m_shapeParams))
    {
        shapeParam->hide();
    }
    m_textParam->hide();
    m_mosaicParam->hide();
}

QColor AnnotationToolBar::loadColor(const QString& settingsKey, const QColor& fallbackColor)
{
    const QString storedName = m_settings->value(settingsKey, fallbackColor.name()).toString();
    QColor storedColor(storedName);
    if (!storedColor.isValid())
    {
        storedColor = fallbackColor;
    }
    return storedColor;
}

int AnnotationToolBar::loadInt(const QString& settingsKey, int fallbackValue)
{
    return m_settings->value(settingsKey, fallbackValue).toInt();
}

bool AnnotationToolBar::loadBool(const QString& settingsKey, bool fallbackValue)
{
    return m_settings->value(settingsKey, fallbackValue).toBool();
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