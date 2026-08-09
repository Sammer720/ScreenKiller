/**
 * \file AnnotationToolBar.cpp
 * \brief AnnotationToolBar v5 实现：一级工具竖列、左侧弹出框体与信号发射
 *
 * 实现要点：
 *   1. 工具栏本体背景不依赖 QSS，在 paintEvent 自绘半透明圆角暖色矩形
 *      （WA_TranslucentBackground + QPainterPath），与 GuidePanel 同设计语言；
 *      按钮 checked/hover 走全局 QSS 紫色系，工具栏不再携带私有 QSS。
 *   2. 二三级内容（几何二级页 + 各工具参数页）统一放入工具栏左侧的独立弹出
 *      框体（m_popoutStack 页栈），框体同样自绘背景，作为父控件
 *      （m_centralStack）子控件悬浮显示，定位紧贴工具栏左侧。
 *   3. 持久化：点一级写 annotation/defaultTool、点几何二级写
 *      annotation/defaultGeometry，参数调整（选色/拖滑块/勾填充/改字体）即写回；
 *      构造期只读不写，先设置默认状态再 connect，避免误发信号。
 *   4. 弹出框体懒创建：首次显示时才 new，避免工具栏独立使用时产生多余控件。
 *   5. 弹开时机：框体仅由用户点击一级/二级按钮时弹出；restoreDefaultTool()
 *      截屏完成时只恢复工具/参数状态并补发射参数信号同步场景，不弹框体。
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
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

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
/// 几何二级页内按钮间距（像素，4 x 36 按钮需收紧以装入固定面板宽度）
constexpr int G_GEOMETRY_ROW_SPACING = 2;
/// 滑块数值标签固定宽度（像素，避免位数变化导致滑块跳动）
constexpr int G_SLIDER_VALUE_LABEL_WIDTH = 24;
/// 几何一级按钮专用互斥组 id（Tool 枚举无 Geometry 值，几何是工具栏内部一级分组）
constexpr int G_GEOMETRY_BUTTON_ID = 100;

// ============================ 弹出框体常量 ============================
/// 弹出框体固定宽度（像素）
constexpr int G_POPOUT_WIDTH = 180;
/// 弹出框体与工具栏右侧的间距（像素）
constexpr int G_POPOUT_OFFSET = 8;

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

// ============================ 弹出框体页索引（页栈顺序与 ensurePopoutPanel 装入顺序一致） ============================
constexpr int G_PAGE_PEN         = 0;   ///< 水笔参数页
constexpr int G_PAGE_HIGHLIGHTER = 1;   ///< 荧光笔参数页
constexpr int G_PAGE_GEOMETRY    = 2;   ///< 几何二级页（直线/箭头/方框/圆横排）
constexpr int G_PAGE_TEXT        = 3;   ///< 文字参数页
constexpr int G_PAGE_MOSAIC      = 4;   ///< 马赛克参数页
/// 几何图形参数页起始索引（其后按 直线/箭头/方框/圆 顺序递增）
constexpr int G_PAGE_SHAPE_BASE  = 5;

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

// ============================ 工具枚举 → 持久化名称映射 ============================
/// @brief 具体场景工具 → 一级工具名称（用于点一级时写回 defaultTool）
/// @param tool 场景工具（Pen/Highlighter/Text/Mosaic，其余兜底 "pen"）
/// @return 持久化工具名称
QString toolNameOf(SK::Tool tool)
{
    switch (tool)
    {
    case SK::Tool::Highlighter:
        return QStringLiteral("highlighter");
    case SK::Tool::Text:
        return QStringLiteral("text");
    case SK::Tool::Mosaic:
        return QStringLiteral("mosaic");
    default:
        return QStringLiteral("pen");
    }
}

/// @brief 几何图形 → 图形名称（用于点二级时写回 defaultGeometry）
/// @param shape 图形工具（Arrow/Rectangle/Ellipse，其余兜底 "line"）
/// @return 持久化图形名称
QString shapeNameOf(SK::Tool shape)
{
    switch (shape)
    {
    case SK::Tool::Arrow:
        return QStringLiteral("arrow");
    case SK::Tool::Rectangle:
        return QStringLiteral("rectangle");
    case SK::Tool::Ellipse:
        return QStringLiteral("ellipse");
    default:
        return QStringLiteral("line");
    }
}

/// @brief 判断是否为几何图形工具
/// @param tool 待判断的工具
/// @return 是几何图形（Line/Arrow/Rectangle/Ellipse）返回 true
bool isGeometryTool(SK::Tool tool)
{
    return (tool == SK::Tool::Line) || (tool == SK::Tool::Arrow)
        || (tool == SK::Tool::Rectangle) || (tool == SK::Tool::Ellipse);
}

/// @brief 非几何一级工具 → 弹出框体页索引（几何由调用方单独处理）
/// @param tool 场景工具（Pen/Highlighter/Text/Mosaic）
/// @return 对应参数页索引
int pageIndexOfTool(SK::Tool tool)
{
    switch (tool)
    {
    case SK::Tool::Pen:
        return G_PAGE_PEN;
    case SK::Tool::Highlighter:
        return G_PAGE_HIGHLIGHTER;
    case SK::Tool::Text:
        return G_PAGE_TEXT;
    case SK::Tool::Mosaic:
        return G_PAGE_MOSAIC;
    default:
        return G_PAGE_GEOMETRY;
    }
}

// ============================ 弹出框体（自绘半透明圆角背景） ============================
/// @brief 弹出框体：与 GuidePanel / 工具栏同设计语言的悬浮面板
///
/// 仅提供 WA_TranslucentBackground + paintEvent 自绘圆角暖色背景，
/// 内容由外部布局（页栈）填充。
class PopoutPanel : public QWidget
{
public:
    /// @brief 构造函数：启用透明背景，背景由 paintEvent 自绘
    /// @param parent 父控件（工具栏的父控件，即 m_centralStack）
    explicit PopoutPanel(QWidget* parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

protected:
    /// @brief 自绘半透明圆角暖色背景（与 GuidePanel 同色系，保持视觉统一）
    /// @param event 绘制事件
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath path;
        path.addRoundedRect(rect(), G_CORNER_RADIUS, G_CORNER_RADIUS);
        painter.fillPath(path, QColor(G_BG_R, G_BG_G, G_BG_B, G_BG_A));
    }
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
    // 注意：不设置任何私有 QSS —— 按钮 checked/hover 走全局紫色系，
    // 工具栏与弹出框体背景均由 paintEvent 自绘圆角

    // 配置读写：默认构造跟随 main.cpp 的 org/app 与 INI 格式
    m_settings = new QSettings(this);
    // 一级/二级互斥组：同一时刻仅允许一个按钮处于选中态
    m_level1Group = new QButtonGroup(this);
    m_level1Group->setExclusive(true);
    m_level2Group = new QButtonGroup(this);
    m_level2Group->setExclusive(true);

    // ---------- 工具栏本体：仅一级按钮竖列，无任何内联展开区 ----------
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN);
    rootLayout->setSpacing(G_PANEL_SPACING);

    m_penButton = createLevel1Button(G_ICON_PEN, tr("水笔"), static_cast<int>(SK::Tool::Pen));
    rootLayout->addWidget(m_penButton);

    m_highlighterButton = createLevel1Button(G_ICON_HIGHLIGHTER, tr("荧光笔"),
                                             static_cast<int>(SK::Tool::Highlighter));
    rootLayout->addWidget(m_highlighterButton);

    m_geometryButton = createLevel1Button(G_ICON_GEOMETRY, tr("几何"), G_GEOMETRY_BUTTON_ID);
    rootLayout->addWidget(m_geometryButton);

    m_textButton = createLevel1Button(G_ICON_TEXT, tr("文字"), static_cast<int>(SK::Tool::Text));
    rootLayout->addWidget(m_textButton);

    m_mosaicButton = createLevel1Button(G_ICON_MOSAIC, tr("马赛克"),
                                        static_cast<int>(SK::Tool::Mosaic));
    rootLayout->addWidget(m_mosaicButton);

    // stretch 吸收面板底部剩余空间：按钮列顶部对齐，底部留白
    rootLayout->addStretch();

    // ---------- 二三级内容：统一创建，延迟装入弹出框体页栈 ----------
    // 参数页/二级页先以工具栏为父创建（读 QSettings 初始化状态），
    // 首次显示弹出框体时由 ensurePopoutPanel 重新挂到页栈（addWidget 自动 reparent）
    m_penParam = createStrokeParam(G_PEN_SPEC);
    m_highlighterParam = createStrokeParam(G_HIGHLIGHTER_SPEC);
    m_geometryPage = createGeometryPage();
    m_textParam = createTextParam();
    m_mosaicParam = createMosaicParam();

    // 几何图形参数页：每个图形独立一页，按固定顺序装入页栈
    m_shapeParams.insert(static_cast<int>(SK::Tool::Line), createStrokeParam(G_LINE_SPEC));
    m_shapeParams.insert(static_cast<int>(SK::Tool::Arrow), createStrokeParam(G_ARROW_SPEC));
    m_shapeParams.insert(static_cast<int>(SK::Tool::Rectangle), createStrokeParam(G_RECT_SPEC));
    m_shapeParams.insert(static_cast<int>(SK::Tool::Ellipse), createStrokeParam(G_ELLIPSE_SPEC));

    // 一级/二级点击分发：idClicked 仅在用户点击时触发，程序 setChecked 不会误入
    connect(m_level1Group, &QButtonGroup::idClicked, this, &AnnotationToolBar::onLevel1Clicked);
    connect(m_level2Group, &QButtonGroup::idClicked, this,
            [this](int groupId)
    {
        onLevel2Clicked(static_cast<SK::Tool>(groupId));
    });

    // 应用持久化的默认工具（仅 UI 高亮，不发射信号、不弹框体；
    // 截屏完成时 restoreDefaultTool 会再同步参数并同步场景，框体只由点击弹开）
    const QString defaultToolName = m_settings->value(G_KEY_DEFAULT_TOOL,
                                                      QStringLiteral("pen")).toString();
    if (defaultToolName.compare(QStringLiteral("geometry"), Qt::CaseInsensitive) == 0)
    {
        const QString geometryName = m_settings->value(G_KEY_DEFAULT_GEOMETRY,
                                                       QStringLiteral("line")).toString();
        m_currentGeometryShape = shapeFromName(geometryName);
        m_currentSceneTool = m_currentGeometryShape;
        m_geometryButton->setChecked(true);
        setLevel2Checked(m_currentGeometryShape);
    }
    else
    {
        m_currentSceneTool = toolFromName(defaultToolName);
        setLevel1Checked(m_currentSceneTool);
    }
}

SK::ToolButton* AnnotationToolBar::createLevel1Button(const QString& iconResource,
                                                      const QString& toolTipText,
                                                      int groupId)
{
    auto* toolButton = new SK::ToolButton(this);
    toolButton->setIcon(QIcon(iconResource));
    toolButton->setIconSize(QSize(G_TOOL_ICON_SIZE, G_TOOL_ICON_SIZE));
    toolButton->setCheckable(true);
    // 单图标无三态变体：ToolButton 非三态时按普通按钮绘制，高亮交给全局 QSS
    toolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolButton->setObjectName(QStringLiteral("annotationToolButton"));
    toolButton->setAutoRaise(true);
    toolButton->setFixedSize(G_TOOL_BUTTON_SIZE, G_TOOL_BUTTON_SIZE);
    // 覆盖全局 QSS 的 padding(6px 14px)——36px 窄按钮会被水平 padding 压扁图标；
    // 仅覆盖 padding，checked/hover 仍走全局紫色系
    toolButton->setStyleSheet(QStringLiteral("padding: 0px;"));

    // tooltip：ToolButton 默认拦截 QEvent::ToolTip，显式放行后 setToolTip 生效
    toolButton->setToolTip(toolTipText);
    toolButton->setToolTipEnabled(false);

    // 以 Tool 枚举值（几何用专用 id）作为组内 id，便于点击分发
    m_level1Group->addButton(toolButton, groupId);
    return toolButton;
}

SK::ToolButton* AnnotationToolBar::createLevel2Button(const QString& iconResource,
                                                      const QString& toolTipText,
                                                      SK::Tool shape)
{
    auto* toolButton = new SK::ToolButton(this);
    toolButton->setIcon(QIcon(iconResource));
    toolButton->setIconSize(QSize(G_TOOL_ICON_SIZE, G_TOOL_ICON_SIZE));
    toolButton->setCheckable(true);
    toolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolButton->setObjectName(QStringLiteral("annotationToolButton"));
    toolButton->setAutoRaise(true);
    toolButton->setFixedSize(G_TOOL_BUTTON_SIZE, G_TOOL_BUTTON_SIZE);
    // 覆盖全局 QSS 的 padding(6px 14px)——36px 窄按钮会被水平 padding 压扁图标；
    // 仅覆盖 padding，checked/hover 仍走全局紫色系
    toolButton->setStyleSheet(QStringLiteral("padding: 0px;"));

    // tooltip：ToolButton 默认拦截 QEvent::ToolTip，显式放行后 setToolTip 生效
    toolButton->setToolTip(toolTipText);
    toolButton->setToolTipEnabled(false);

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
        // 色块为功能控件，样式元素级内联（背景色 + 圆形 + checked 紫色边框）；
        // 不用工具栏私有 QSS，checked 边框与全局紫色系（#8B7AB8）保持一致
        colorButton->setStyleSheet(QStringLiteral(R"(
QToolButton#colorSwatch {
    background-color: %1;
    border: 1px solid rgba(0, 0, 0, 0.25);
    border-radius: 10px;
    padding: 0px;
}
QToolButton#colorSwatch:checked {
    border: 2px solid #8B7AB8;
}
)").arg(colorName));
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
    // 滑块为功能控件，样式元素级内联（轨道/进度/手柄，进度与手柄用全局紫色系）
    slider->setStyleSheet(QStringLiteral(R"(
QSlider#paramSlider::groove:horizontal {
    height: 4px;
    background: rgba(0, 0, 0, 0.15);
    border-radius: 2px;
}
QSlider#paramSlider::sub-page:horizontal {
    background: #B5A5D1;
    border-radius: 2px;
}
QSlider#paramSlider::handle:horizontal {
    width: 14px;
    height: 14px;
    margin: -5px 0;
    border-radius: 7px;
    background: #8B7AB8;
}
)"));

    // 数值标签无单位文字，固定宽度避免位数变化导致滑块跳动
    auto* valueLabel = new QLabel(QString::number(initialValue), rowWidget);
    valueLabel->setObjectName(QStringLiteral("sliderValueLabel"));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setFixedWidth(G_SLIDER_VALUE_LABEL_WIDTH);
    valueLabel->setStyleSheet(QStringLiteral("color: #333333;"));

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

    // 初始隐藏：装入弹出框体页栈后由页栈管理可见性
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

    // 初始隐藏：装入弹出框体页栈后由页栈管理可见性
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

    // 初始隐藏：装入弹出框体页栈后由页栈管理可见性
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createGeometryPage()
{
    auto* pageWidget = new QWidget(this);
    auto* pageLayout = new QHBoxLayout(pageWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(G_GEOMETRY_ROW_SPACING);

    pageLayout->addWidget(createLevel2Button(G_ICON_LINE, tr("直线"), SK::Tool::Line));
    pageLayout->addWidget(createLevel2Button(G_ICON_ARROW, tr("箭头"), SK::Tool::Arrow));
    pageLayout->addWidget(createLevel2Button(G_ICON_SQUARE, tr("方框"), SK::Tool::Rectangle));
    pageLayout->addWidget(createLevel2Button(G_ICON_CIRCLE, tr("圆"), SK::Tool::Ellipse));

    // 初始隐藏：装入弹出框体页栈后由页栈管理可见性
    pageWidget->hide();
    return pageWidget;
}

void AnnotationToolBar::setCurrentTool(SK::Tool tool)
{
    switch (tool)
    {
    case SK::Tool::Line:
    case SK::Tool::Arrow:
    case SK::Tool::Rectangle:
    case SK::Tool::Ellipse:
        // 几何图形：记录当前图形并展开「几何一级 + 二级图形 + 该图形参数页」
        m_currentGeometryShape = tool;
        m_currentSceneTool = tool;
        m_geometryButton->setChecked(true);
        setLevel2Checked(tool);
        showPopoutPanel(m_shapePageIndex.value(static_cast<int>(tool), G_PAGE_GEOMETRY));
        break;
    case SK::Tool::Pen:
    case SK::Tool::Highlighter:
    case SK::Tool::Text:
    case SK::Tool::Mosaic:
        m_currentSceneTool = tool;
        setLevel1Checked(tool);
        showPopoutPanel(pageIndexOfTool(tool));
        break;
    default:
        // Select 等非工具栏工具：仅收起弹出框体，不改变高亮与工具状态
        hidePopoutPanel();
        return;
    }

    // 同步外部场景，保证场景工具与工具栏状态一致
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::collapseExpanded()
{
    // 保留一级/二级按钮与当前工具，仅隐藏弹出框体
    hidePopoutPanel();
}

void AnnotationToolBar::restoreDefaultTool()
{
    const QString defaultToolName = m_settings->value(G_KEY_DEFAULT_TOOL,
                                                      QStringLiteral("pen")).toString();
    // 仅恢复工具/参数状态并同步外部场景，不弹框体（二三级只由用户点击弹开）
    if (defaultToolName.compare(QStringLiteral("geometry"), Qt::CaseInsensitive) == 0)
    {
        // 默认工具为几何组：按持久化的默认图形恢复高亮与状态
        const QString geometryName = m_settings->value(G_KEY_DEFAULT_GEOMETRY,
                                                       QStringLiteral("line")).toString();
        const SK::Tool geometryTool = shapeFromName(geometryName);
        m_currentGeometryShape = geometryTool;
        m_currentSceneTool = geometryTool;
        m_geometryButton->setChecked(true);
        setLevel2Checked(geometryTool);
        Q_EMIT toolChanged(geometryTool);
    }
    else
    {
        const SK::Tool tool = toolFromName(defaultToolName);
        m_currentSceneTool = tool;
        setLevel1Checked(tool);
        Q_EMIT toolChanged(tool);
    }

    // 同步场景参数：构造期控件 setValue 发射的信号早于 MainWindow connect，
    // 场景仍持默认值，此处按当前工具读取 QSettings 存储值补发
    applyCurrentParametersToScene();
}

void AnnotationToolBar::applyCurrentParametersToScene()
{
    const SK::Tool currentTool = m_currentSceneTool;
    switch (currentTool)
    {
    case SK::Tool::Pen:
        Q_EMIT penColorChanged(loadColor(G_KEY_PEN_COLOR, SK::G_ANNOTATION_COLOR_PALETTE.first()));
        Q_EMIT penWidthChanged(static_cast<qreal>(loadInt(G_KEY_PEN_WIDTH, G_DEFAULT_PEN_WIDTH)));
        break;
    case SK::Tool::Highlighter:
        Q_EMIT penColorChanged(loadColor(G_KEY_HL_COLOR, SK::G_HIGHLIGHTER_COLOR_PALETTE.first()));
        Q_EMIT penWidthChanged(static_cast<qreal>(loadInt(G_KEY_HL_WIDTH, G_DEFAULT_HL_WIDTH)));
        break;
    case SK::Tool::Line:
    case SK::Tool::Arrow:
    case SK::Tool::Rectangle:
    case SK::Tool::Ellipse:
        // 几何类：先按当前几何图形确定形状，再取该形状参数（颜色 + 宽度，方框/圆补填充）
        applyGeometryParametersToScene();
        break;
    case SK::Tool::Text:
        Q_EMIT penColorChanged(loadColor(G_KEY_TEXT_COLOR, SK::G_ANNOTATION_COLOR_PALETTE.first()));
        Q_EMIT fontSizeChanged(static_cast<qreal>(loadInt(G_KEY_TEXT_FONT_SIZE, G_DEFAULT_FONT_SIZE)));
        Q_EMIT fontFamilyChanged(
            m_settings->value(G_KEY_TEXT_FONT_FAMILY, G_DEFAULT_FONT_FAMILY).toString());
        break;
    case SK::Tool::Mosaic:
        Q_EMIT penWidthChanged(static_cast<qreal>(loadInt(G_KEY_MOSAIC_WIDTH, G_DEFAULT_MOSAIC_WIDTH)));
        break;
    default:
        // Select 等非参数工具：无参数可同步
        break;
    }
}

void AnnotationToolBar::applyGeometryParametersToScene()
{
    // 按当前几何图形取对应装配规格（与 createStrokeParam 共用同一规格常量）
    const StrokeParamSpec* shapeSpec = nullptr;
    switch (m_currentGeometryShape)
    {
    case SK::Tool::Line:
        shapeSpec = &G_LINE_SPEC;
        break;
    case SK::Tool::Arrow:
        shapeSpec = &G_ARROW_SPEC;
        break;
    case SK::Tool::Rectangle:
        shapeSpec = &G_RECT_SPEC;
        break;
    case SK::Tool::Ellipse:
        shapeSpec = &G_ELLIPSE_SPEC;
        break;
    default:
        break;
    }
    if (shapeSpec == nullptr)
    {
        return;
    }

    const qreal storedWidth = static_cast<qreal>(loadInt(shapeSpec->widthKey,
                                                         shapeSpec->defaultWidth));
    Q_EMIT penColorChanged(loadColor(shapeSpec->colorKey, shapeSpec->palette->first()));
    Q_EMIT penWidthChanged(storedWidth);

    // 方框/圆：填充开关同步场景（未勾选时显式复位为 NoBrush，避免残留上次填充）
    if (shapeSpec->withFill)
    {
        const bool storedFilled = loadBool(shapeSpec->fillKey, false);
        Q_EMIT brushStyleChanged(storedFilled ? Qt::SolidPattern : Qt::NoBrush);
    }
}

void AnnotationToolBar::onLevel1Clicked(int groupId)
{
    if (groupId == G_GEOMETRY_BUTTON_ID)
    {
        // 几何：写回默认工具「几何组」，不发射 toolChanged（尚未确定具体图形）。
        // 已选中具体图形时直接弹其参数页（快捷调参），否则弹二级选择页。
        m_settings->setValue(G_KEY_DEFAULT_TOOL, QStringLiteral("geometry"));
        if (isGeometryTool(m_currentSceneTool))
        {
            const int shapePage = m_shapePageIndex.value(
                static_cast<int>(m_currentSceneTool), G_PAGE_GEOMETRY);
            showPopoutPanel(shapePage);
        }
        else
        {
            showPopoutPanel(G_PAGE_GEOMETRY);
            setLevel2Checked(m_currentGeometryShape);
        }
        return;
    }

    // 具体工具：写回默认工具、弹出其参数框体并通知外部切换工具
    const SK::Tool tool = static_cast<SK::Tool>(groupId);
    m_currentSceneTool = tool;
    m_settings->setValue(G_KEY_DEFAULT_TOOL, toolNameOf(tool));
    showPopoutPanel(pageIndexOfTool(tool));
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::onLevel2Clicked(SK::Tool shape)
{
    m_currentGeometryShape = shape;
    m_currentSceneTool = shape;
    // 点几何二级图形：写回默认几何图形，弹该图形三级参数页并同步外部场景
    m_settings->setValue(G_KEY_DEFAULT_GEOMETRY, shapeNameOf(shape));
    showPopoutPanel(m_shapePageIndex.value(static_cast<int>(shape), G_PAGE_GEOMETRY));
    Q_EMIT toolChanged(shape);
}

void AnnotationToolBar::setLevel1Checked(SK::Tool tool)
{
    switch (tool)
    {
    case SK::Tool::Pen:
        m_penButton->setChecked(true);
        break;
    case SK::Tool::Highlighter:
        m_highlighterButton->setChecked(true);
        break;
    case SK::Tool::Text:
        m_textButton->setChecked(true);
        break;
    case SK::Tool::Mosaic:
        m_mosaicButton->setChecked(true);
        break;
    default:
        // 几何由 setCurrentTool 的几何分支单独处理
        break;
    }
}

void AnnotationToolBar::setLevel2Checked(SK::Tool shape)
{
    QAbstractButton* shapeButton = m_level2Group->button(static_cast<int>(shape));
    if (shapeButton != nullptr)
    {
        shapeButton->setChecked(true);
    }
}

void AnnotationToolBar::ensurePopoutPanel()
{
    if (m_popoutPanel != nullptr)
    {
        return;
    }

    // 弹出框体挂到工具栏的父控件（m_centralStack）上，与工具栏同级悬浮，
    // 这样才能叠加在中央栈页面上而不被视口遮挡
    QWidget* host = parentWidget();
    if (host == nullptr)
    {
        host = this;
    }
    m_popoutPanel = new PopoutPanel(host);
    m_popoutPanel->hide();
    m_popoutPanel->setFixedWidth(G_POPOUT_WIDTH);

    auto* panelLayout = new QVBoxLayout(m_popoutPanel);
    panelLayout->setContentsMargins(G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN);
    panelLayout->setSpacing(G_PANEL_SPACING);

    // 页栈：参数页/二级页统一入栈，addWidget 会自动把页面重新挂到页栈
    m_popoutStack = new QStackedWidget(m_popoutPanel);
    panelLayout->addWidget(m_popoutStack);

    m_popoutStack->addWidget(m_penParam);            // G_PAGE_PEN
    m_popoutStack->addWidget(m_highlighterParam);    // G_PAGE_HIGHLIGHTER
    m_popoutStack->addWidget(m_geometryPage);        // G_PAGE_GEOMETRY
    m_popoutStack->addWidget(m_textParam);           // G_PAGE_TEXT
    m_popoutStack->addWidget(m_mosaicParam);         // G_PAGE_MOSAIC

    // 几何图形参数页：按固定顺序（直线/箭头/方框/圆）从 G_PAGE_SHAPE_BASE 起排
    const QVector<SK::Tool> shapeOrder = {
        SK::Tool::Line, SK::Tool::Arrow, SK::Tool::Rectangle, SK::Tool::Ellipse
    };
    int shapePage = G_PAGE_SHAPE_BASE;
    for (const SK::Tool shape : shapeOrder)
    {
        m_popoutStack->addWidget(m_shapeParams.value(static_cast<int>(shape)));
        m_shapePageIndex.insert(static_cast<int>(shape), shapePage);
        ++shapePage;
    }
}

void AnnotationToolBar::showPopoutPanel(int pageIndex)
{
    ensurePopoutPanel();
    m_popoutStack->setCurrentIndex(pageIndex);

    // 高度随当前页内容自适应：页高 + 面板上下内边距
    QWidget* currentPage = m_popoutStack->currentWidget();
    if (currentPage != nullptr)
    {
        const int pageHeight = currentPage->sizeHint().height();
        m_popoutPanel->setFixedHeight(pageHeight + 2 * G_PANEL_MARGIN);
    }

    updatePopoutGeometry();
    m_popoutPanel->show();
    m_popoutPanel->raise();
}

void AnnotationToolBar::hidePopoutPanel()
{
    if (m_popoutPanel != nullptr)
    {
        m_popoutPanel->hide();
    }
}

void AnnotationToolBar::updatePopoutGeometry()
{
    if (m_popoutPanel == nullptr)
    {
        return;
    }

    // 工具栏贴右缘，二三级框体在其左侧弹开（避免与右侧工具栏/窗口边缘挤在一处）
    const int panelX = pos().x() - G_POPOUT_WIDTH - G_POPOUT_OFFSET;
    const int panelY = pos().y();

    // 防御性钳制：窗口过窄时框体可能越出左缘，钳到工具栏右侧（窄窗口兜底）
    int finalX = panelX;
    if (parentWidget() != nullptr)
    {
        if (finalX < 0)
        {
            finalX = pos().x() + width() + G_POPOUT_OFFSET;
        }
    }
    m_popoutPanel->move(finalX, panelY);
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