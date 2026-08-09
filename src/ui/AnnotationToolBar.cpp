/**
 * \file AnnotationToolBar.cpp
* \brief AnnotationToolBar v8 实现：一级工具竖列、左侧双框体级联与信号发射
 *
 * 实现要点：
 *   1. 工具栏本体背景不依赖 QSS，在 paintEvent 自绘半透明圆角暖色矩形
 *      （WA_TranslucentBackground + QPainterPath），与 GuidePanel 同设计语言；
 *      按钮 checked/hover 走全局 QSS 紫色系，工具栏不再携带私有 QSS。
 *   2. 二三级拆为两个独立弹出框体（PopoutPanel 自绘背景）向左级联：
 *      m_geometryPanel 几何二级框体（4 个图形按钮竖向）贴工具栏左侧；
 *      m_paramPanel 参数三级框体（各工具参数页）在几何框体左侧再弹出；
 *      无几何展开时参数框体贴工具栏左侧。三级参数框体只由点击二级框体中
 *      的图形按钮时弹出（唯一入口）；点击一级几何按钮只开/关二级框体。
 *   3. 一级几何启闭循环：点击几何→弹二级框体；再次点击几何→关二级（连同
 *      三级一起关）。每次点击几何按钮都把当前工具切换为当前默认几何工具
 *      （来源 QSettings annotation/defaultGeometry），而非上一个使用的工具。
 *   3. 切换工具即装载参数：onLevel1Clicked / onLevel2Clicked / setCurrentTool
 *      切换后调用 loadToolParamsToScene() 读取该工具 QSettings 存储值补发全部
 *      参数信号（颜色/宽度/填充/字号/字体），使场景立即装载该工具实例参数；
 *      syncParamControlsToTool() 同步参数区控件显示，避免参数区沿用上一工具配置。
 *   4. 标准标注色板颜色调整传播到所有应用标注色板的工具（水笔/直线/箭头/方框/
 *      圆/文字），荧光笔色板独立不传播，尺寸/填充参数独立不传播。
 *   5. 持久化：点一级写 annotation/defaultTool、点几何二级写
 *      annotation/defaultGeometry，参数调整（选色/拖滑块/勾填充/改字体）即写回；
 *      构造期只读不写，先设置默认状态再 connect，避免误发信号。
 *   6. 弹出框体懒创建：首次显示时才 new，避免工具栏独立使用时产生多余控件。
 *   7. 弹开时机：几何二级框体仅由点击一级几何按钮开/关（启闭循环），三级
 *      参数框体仅由点击二级图形按钮弹出；restoreDefaultTool() 截屏完成时
 *      只恢复工具/参数状态并补发射参数信号同步场景，不弹框体。
 *   8. 框体定位（v7）：updatePanelGeometry 用成员标志 m_geometryVisible 级联
 *      定位（不依赖 isVisible()，规避框体首次创建尚未 show 时的可见性误判）；
 *      工具栏 pos() 未就绪时按父容器宽度推导贴右缘兜底，防止跳最左；点几何而
 *      当前工具非几何时隐藏残留参数框体，防止二三级重叠。
 *   9. 框体内控件（v8）：滑块/勾选框/数值标签走主界面紫色系（滑块 groove
 *      #D9CCEE / 手柄 #8B7AB8，勾选框白底紫边、勾选 #B5A5D1 + 勾号图标），
 *      经 PopoutPanel 的 objectName（annotationPopoutPanel）由 QSS 作用域
 *      限定，不覆盖主界面紫色系控件；二级页 / 参数页 / 行列容器背景显式
 *      透明，露出框体自绘暖色背景，避免继承主界面背景导致色块差异。
 */
#include "AnnotationToolBar.h"

#include "annotation/AnnotationConstants.h"
#include "sub_widget/ToolButton.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFont>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QSignalBlocker>
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
/// 滑块数值标签固定宽度（像素，避免位数变化导致滑块跳动）
constexpr int G_SLIDER_VALUE_LABEL_WIDTH = 24;
/// 几何一级按钮专用互斥组 id（Tool 枚举无 Geometry 值，几何是工具栏内部一级分组）
constexpr int G_GEOMETRY_BUTTON_ID = 100;

// ============================ 弹出框体常量 ============================
/// 参数三级框体固定宽度（像素）
constexpr int G_POPOUT_WIDTH = 200;
/// 几何二级框体固定宽度（像素，4 个 36px 按钮竖排 + 左右内边距，与一级工具栏同宽语言）
constexpr int G_GEOMETRY_PANEL_WIDTH = G_TOOL_BUTTON_SIZE + 2 * G_PANEL_MARGIN;
/// 框体与工具栏/框体之间的间距（像素）
constexpr int G_POPOUT_OFFSET = 8;
/// 工具栏相对父容器右缘的边距（像素，与 MainWindow.cpp 的 G_ANN_TOOLBAR_MARGIN
/// 保持一致，用于工具栏 pos() 未就绪时推导贴右缘位置兜底）
constexpr int G_TOOLBAR_MARGIN = 12;

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
const QString G_KEY_HL_ALPHA          = QStringLiteral("annotation/highlighter/alpha"); ///< 荧光笔透明度
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
constexpr int G_HL_ALPHA_MIN          = 20;   ///< 荧光笔透明度下限
constexpr int G_HL_ALPHA_MAX          = 220;  ///< 荧光笔透明度上限
constexpr int G_DEFAULT_HL_ALPHA      = 100;  ///< 荧光笔默认透明度
constexpr int G_DEFAULT_FONT_SIZE     = 12;   ///< 文字默认字号（pt）
constexpr int G_DEFAULT_MOSAIC_WIDTH  = 20;   ///< 马赛克默认宽度
const QString G_DEFAULT_FONT_FAMILY   = QStringLiteral("微软雅黑");  ///< 文字默认字体族
// 描边类颜色默认值取各色板首色（标注色板首色 #FF0000，荧光笔色板首色 #FFEB3B）

// ============================ 参数框体页索引（页栈顺序与 ensureParamPanel 装入顺序一致） ============================
constexpr int G_PAGE_PEN         = 0;   ///< 水笔参数页
constexpr int G_PAGE_HIGHLIGHTER = 1;   ///< 荧光笔参数页
constexpr int G_PAGE_TEXT        = 2;   ///< 文字参数页
constexpr int G_PAGE_MOSAIC      = 3;   ///< 马赛克参数页
/// 几何图形参数页起始索引（其后按 直线/箭头/方框/圆 顺序递增）
constexpr int G_PAGE_SHAPE_BASE  = 4;

// ============================ 描边类参数区装配规格 ============================
/// @brief 水笔参数规格：标注色板 + 1~30 宽度
const AnnotationToolBar::StrokeParamSpec G_PEN_SPEC = {
    &SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_PEN_COLOR, G_KEY_PEN_WIDTH,
    static_cast<int>(SK::G_MIN_PEN_WIDTH), static_cast<int>(SK::G_MAX_PEN_WIDTH),
    G_DEFAULT_PEN_WIDTH, false, QString()
};
/// @brief 荧光笔参数规格：荧光笔色板 + 5~40 宽度 + 20~220 透明度渐变滑块
const AnnotationToolBar::StrokeParamSpec G_HIGHLIGHTER_SPEC = {
    &SK::G_HIGHLIGHTER_COLOR_PALETTE, G_KEY_HL_COLOR, G_KEY_HL_WIDTH,
    static_cast<int>(SK::G_MIN_HIGHLIGHT_WIDTH), static_cast<int>(SK::G_MAX_HIGHLIGHT_WIDTH),
    G_DEFAULT_HL_WIDTH, false, QString(),
    true, G_KEY_HL_ALPHA, G_HL_ALPHA_MIN, G_HL_ALPHA_MAX, G_DEFAULT_HL_ALPHA
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

/// @brief 工具 → 描边参数规格（Text/Mosaic 无统一规格，返回 nullptr）
/// @param tool 场景工具
/// @return 对应装配规格；Text/Mosaic 返回 nullptr 由调用方单独处理
const AnnotationToolBar::StrokeParamSpec* specOfTool(SK::Tool tool)
{
    switch (tool)
    {
    case SK::Tool::Pen:
        return &G_PEN_SPEC;
    case SK::Tool::Highlighter:
        return &G_HIGHLIGHTER_SPEC;
    case SK::Tool::Line:
        return &G_LINE_SPEC;
    case SK::Tool::Arrow:
        return &G_ARROW_SPEC;
    case SK::Tool::Rectangle:
        return &G_RECT_SPEC;
    case SK::Tool::Ellipse:
        return &G_ELLIPSE_SPEC;
    default:
        return nullptr;
    }
}

/// @brief 非几何一级工具 → 参数框体页索引（几何由 m_shapePageIndex 单独索引）
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
        return G_PAGE_PEN;  // 几何图形不使用统一参数页索引，由调用方查 m_shapePageIndex
    }
}

// ============================ 弹出框体（自绘半透明圆角背景） ============================
/// @brief 弹出框体：与 GuidePanel / 工具栏同设计语言的悬浮面板
///
/// 仅提供 WA_TranslucentBackground + paintEvent 自绘圆角暖色背景，
/// 内容由外部布局（几何二级页 / 参数页栈）填充。几何二级框体与参数
/// 三级框体复用本类，各自独立宽度与定位。
class PopoutPanel : public QWidget
{
public:
    /// @brief 构造函数：设置 objectName 供 QSS 作用域限定，启用透明背景
    /// @param parent 父控件（工具栏的父控件，即 m_centralStack）
    explicit PopoutPanel(QWidget* parent)
        : QWidget(parent)
    {
        // objectName 供全局 QSS 以「QWidget#annotationPopoutPanel ...」作用域
        // 限定框体内控件走暖色系样式，不覆盖主界面紫色系控件
        setObjectName(QStringLiteral("annotationPopoutPanel"));
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

/// @brief 荧光笔透明度渐变滑块：轨道为「透明→不透明」当前色渐变，无标签无刻度
///
/// 轨道背景用 qlineargradient 从 rgba(r,g,b,0) 渐变到 rgba(r,g,b,255)，
/// 并随 baseColor 更新；手柄用紫色圆点样式。仅作样式呈现，不提供文字刻度。
class ColorAlphaSlider : public QSlider
{
public:
    /// @brief 构造函数
    /// @param baseColor 渐变基础色（荧光笔当前色）
    /// @param min 滑块下限
    /// @param max 滑块上限
    /// @param value 初始值
    /// @param parent 父控件
    ColorAlphaSlider(const QColor& baseColor, int min, int max, int value, QWidget* parent)
        : QSlider(Qt::Horizontal, parent), m_baseColor(baseColor)
    {
        setObjectName(QStringLiteral("alphaSlider"));
        setRange(min, max);
        setValue(value);
        // 无刻度、无标签
        setTickPosition(QSlider::NoTicks);
        updateGradientStyle();
    }

    /// @brief 更新渐变基础色并重刷轨道样式
    /// @param baseColor 新基础色（荧光笔当前色）
    void setBaseColor(const QColor& baseColor)
    {
        if (m_baseColor == baseColor)
        {
            return;
        }
        m_baseColor = baseColor;
        updateGradientStyle();
    }

private:
    /// @brief 依据当前基础色生成轨道「透明→不透明」渐变与紫色圆点手柄样式
    void updateGradientStyle()
    {
        const int red   = m_baseColor.red();
        const int green = m_baseColor.green();
        const int blue  = m_baseColor.blue();
        setStyleSheet(QStringLiteral(
            "QSlider#alphaSlider::groove:horizontal {"
            "  height: 6px;"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            "    stop:0 rgba(%1,%2,%3,0), stop:1 rgba(%1,%2,%3,255));"
            "  border-radius: 3px;"
            "}"
            "QSlider#alphaSlider::handle:horizontal {"
            "  width: 16px; height: 16px; margin: -5px 0;"
            "  border-radius: 8px;"
            "  background: #8B7AB8;"
            "  border: 2px solid #FFFFFF;"
            "}"
            "QSlider#alphaSlider::handle:horizontal:hover {"
            "  background: #6B5B95;"
            "}"
        ).arg(red).arg(green).arg(blue));
    }

    QColor m_baseColor;  ///< 渐变基础色（荧光笔当前色）
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

    // ---------- 参数页：统一创建并登记控件句柄，延迟装入参数框体页栈 ----------
    // 参数页先以工具栏为父创建（读 QSettings 初始化状态），登记句柄供
    // syncParamControlsToTool 按工具刷新显示；首次显示框体时由
    // ensureParamPanel 重新挂到页栈（addWidget 自动 reparent）
    ParamHandles penHandles;
    m_penParam = createStrokeParam(G_PEN_SPEC, &penHandles);
    m_paramHandles.insert(static_cast<int>(SK::Tool::Pen), penHandles);

    ParamHandles highlighterHandles;
    m_highlighterParam = createStrokeParam(G_HIGHLIGHTER_SPEC, &highlighterHandles);
    m_paramHandles.insert(static_cast<int>(SK::Tool::Highlighter), highlighterHandles);

    ParamHandles textHandles;
    m_textParam = createTextParam(&textHandles);
    m_paramHandles.insert(static_cast<int>(SK::Tool::Text), textHandles);

    ParamHandles mosaicHandles;
    m_mosaicParam = createMosaicParam(&mosaicHandles);
    m_paramHandles.insert(static_cast<int>(SK::Tool::Mosaic), mosaicHandles);

    // 几何图形参数页：每个图形独立一页，按固定顺序装入参数框体页栈
    ParamHandles lineHandles;
    m_shapeParams.insert(static_cast<int>(SK::Tool::Line),
                         createStrokeParam(G_LINE_SPEC, &lineHandles));
    m_paramHandles.insert(static_cast<int>(SK::Tool::Line), lineHandles);

    ParamHandles arrowHandles;
    m_shapeParams.insert(static_cast<int>(SK::Tool::Arrow),
                         createStrokeParam(G_ARROW_SPEC, &arrowHandles));
    m_paramHandles.insert(static_cast<int>(SK::Tool::Arrow), arrowHandles);

    ParamHandles rectHandles;
    m_shapeParams.insert(static_cast<int>(SK::Tool::Rectangle),
                         createStrokeParam(G_RECT_SPEC, &rectHandles));
    m_paramHandles.insert(static_cast<int>(SK::Tool::Rectangle), rectHandles);

    ParamHandles ellipseHandles;
    m_shapeParams.insert(static_cast<int>(SK::Tool::Ellipse),
                         createStrokeParam(G_ELLIPSE_SPEC, &ellipseHandles));
    m_paramHandles.insert(static_cast<int>(SK::Tool::Ellipse), ellipseHandles);

    // 几何二级页懒创建于 ensureGeometryPanel（首次显示几何框体时），
    // 其按钮加入 m_level2Group，连接已在下方建立，点击照常分发

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
        // 几何二级按钮此刻尚未创建（几何页懒创建），高亮延迟到
        // ensureGeometryPanel 创建按钮后按当前几何图形补设
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
                                           const QString& settingsKey,
                                           ColorPaletteKind paletteKind,
                                           QButtonGroup** colorGroupOut)
{
    auto* rowWidget = new QWidget(this);
    // objectName 供 QSS 作用域限定：颜色行容器背景透明，露出框体自绘暖色背景
    // （不透明容器会继承主界面背景色，盖住 PopoutPanel 自绘背景）
    rowWidget->setObjectName(QStringLiteral("annotationParamRow"));
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
        // 色块为功能控件，样式元素级内联（背景色 + 圆形 + checked 暖色边框）：
        // 背景色填充与圆角是该控件不可省的功能性样式，checked 边框用暖棕系
        // （#C68B4E）与标注工具栏暖色背景协调；其余控件走框体作用域 QSS
        colorButton->setStyleSheet(QStringLiteral(R"(
QToolButton#colorSwatch {
    background-color: %1;
    border: 1px solid rgba(0, 0, 0, 0.25);
    border-radius: 10px;
    padding: 0px;
}
QToolButton#colorSwatch:checked {
    border: 2px solid #C68B4E;
}
)").arg(colorName));
        colorGroup->addButton(colorButton, colorIndex);
        colorLayout->addWidget(colorButton);

        // 记录与持久化颜色一致的色块；颜色不在色板时保持勾选第一个
        if (colorName.compare(storedColor.name(), Qt::CaseInsensitive) == 0)
        {
            checkedIndex = colorIndex;
        }
        // 色块点击：按色板类型决定写盘范围（标注色板传播 / 荧光笔独立），
        // 并实时把当前工具颜色发射给场景
        connect(colorButton, &QAbstractButton::clicked, this,
                [this, paletteColor, paletteKind]()
        {
            if (paletteKind == ColorPaletteKind::Annotation)
            {
                // 标准标注色板：颜色传播到所有应用该色板的工具
                // （水笔 / 直线 / 箭头 / 方框 / 圆 / 文字），尺寸/填充等独立参数不受影响
                m_settings->setValue(G_KEY_PEN_COLOR, paletteColor.name());
                m_settings->setValue(G_KEY_LINE_COLOR, paletteColor.name());
                m_settings->setValue(G_KEY_ARROW_COLOR, paletteColor.name());
                m_settings->setValue(G_KEY_RECT_COLOR, paletteColor.name());
                m_settings->setValue(G_KEY_ELLIPSE_COLOR, paletteColor.name());
                m_settings->setValue(G_KEY_TEXT_COLOR, paletteColor.name());
                // 同步各工具参数区色块勾选，使传播后的颜色在参数区即时反映
                syncParamControlsToTool(SK::Tool::Pen);
                syncParamControlsToTool(SK::Tool::Line);
                syncParamControlsToTool(SK::Tool::Arrow);
                syncParamControlsToTool(SK::Tool::Rectangle);
                syncParamControlsToTool(SK::Tool::Ellipse);
                syncParamControlsToTool(SK::Tool::Text);
            }
            else
            {
                // 荧光笔色板：仅写回荧光笔，独立不传播
                m_settings->setValue(G_KEY_HL_COLOR, paletteColor.name());
            }
            // 当前工具颜色实时同步到场景（场景当前工具即本参数区所属工具）
            Q_EMIT penColorChanged(paletteColor);
        });
    }

    // 颜色行固定宽度 = 色块数 × 色块尺寸 + (色块数-1) × 间距，
    // 防止框体裁剪最右色块（不设 stretch 时行会按内容收缩）
    rowWidget->setFixedWidth(palette.size() * G_COLOR_SWATCH_SIZE
                             + (palette.size() - 1) * G_SWATCH_SPACING);

    // 先勾选再连接信号也安全：发射走 clicked，构造期不会误发
    QAbstractButton* checkedButton = colorGroup->button(checkedIndex);
    if (checkedButton != nullptr)
    {
        checkedButton->setChecked(true);
    }
    if (colorGroupOut != nullptr)
    {
        *colorGroupOut = colorGroup;
    }
    return rowWidget;
}

QWidget* AnnotationToolBar::createSliderRow(int minValue, int maxValue, int initialValue,
                                            QSlider** sliderOut, const QString& unit)
{
    auto* rowWidget = new QWidget(this);
    // objectName 供 QSS 作用域限定：滑块行容器背景透明，露出框体自绘暖色背景
    rowWidget->setObjectName(QStringLiteral("annotationParamRow"));
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(G_PANEL_SPACING);

    auto* slider = new QSlider(Qt::Horizontal, rowWidget);
    slider->setObjectName(QStringLiteral("paramSlider"));
    slider->setRange(minValue, maxValue);
    slider->setValue(initialValue);
    // 滑块轨道/进度/手柄样式走全局 QSS（QSlider#paramSlider 规则），
    // 与主界面控件统一视觉语言，不再携带私有内联样式

    // 数值标签：无单位时纯数字，有单位时「数值 单位」（如 "12 px"）；
    // 固定宽度按最长的最大值文本计算，避免单位/位数变化导致滑块跳动
    const QString unitSuffix = unit.isEmpty() ? QString()
                                              : (QStringLiteral(" ") + unit);
    QString labelText = QString::number(initialValue);
    if (!unit.isEmpty())
    {
        labelText = QStringLiteral("%1 %2").arg(initialValue).arg(unit);
    }
    auto* valueLabel = new QLabel(labelText, rowWidget);
    valueLabel->setObjectName(QStringLiteral("sliderValueLabel"));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    const QString sampleText = QString::number(maxValue) + unitSuffix;
    const int computedWidth = valueLabel->fontMetrics().horizontalAdvance(sampleText);
    valueLabel->setFixedWidth(qMax(G_SLIDER_VALUE_LABEL_WIDTH, computedWidth));

    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(valueLabel);

    // 数值标签随滑块同步（以标签为上下文对象，标签销毁即自动断开）
    connect(slider, &QSlider::valueChanged, valueLabel,
            [valueLabel, unit](int value)
    {
        if (unit.isEmpty())
        {
            valueLabel->setText(QString::number(value));
        }
        else
        {
            valueLabel->setText(QStringLiteral("%1 %2").arg(value).arg(unit));
        }
    });

    *sliderOut = slider;
    return rowWidget;
}

QWidget* AnnotationToolBar::createStrokeParam(const StrokeParamSpec& spec,
                                              ParamHandles* handlesOut)
{
    auto* paramWidget = new QWidget(this);
    // objectName 供 QSS 作用域限定：参数页背景透明，露出框体自绘暖色背景
    paramWidget->setObjectName(QStringLiteral("annotationParamPage"));
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 色板类型：荧光笔色板调整独立不传播，其余（标准标注色板）传播到所有标注工具
    const bool isHighlighterPalette = (spec.palette == &SK::G_HIGHLIGHTER_COLOR_PALETTE);
    const ColorPaletteKind paletteKind = isHighlighterPalette
        ? ColorPaletteKind::Highlighter
        : ColorPaletteKind::Annotation;

    // 颜色行（无文字标签，仅色块互斥勾选）
    QButtonGroup* colorGroup = nullptr;
    paramLayout->addWidget(createColorRow(*(spec.palette), spec.colorKey, paletteKind,
                                          &colorGroup));

    // 尺寸滑块行（滑块 + 右侧数值带 px 单位），范围按规格边界
    const int storedWidth = loadInt(spec.widthKey, spec.defaultWidth);
    QSlider* widthSlider = nullptr;
    paramLayout->addWidget(createSliderRow(spec.minWidth, spec.maxWidth, storedWidth,
                                           &widthSlider, QStringLiteral("px")));

    // 拖动滑块：发射宽度变化 + 写回持久化
    connect(widthSlider, &QSlider::valueChanged, this,
            [this, spec](int value)
    {
        Q_EMIT penWidthChanged(static_cast<qreal>(value));
        m_settings->setValue(spec.widthKey, value);
    });

    // 方框/圆额外提供填充勾选：勾选时填充生效并禁用尺寸滑块
    QCheckBox* fillCheck = nullptr;
    if (spec.withFill)
    {
        fillCheck = new QCheckBox(tr("填充"), paramWidget);
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

    // 荧光笔透明度渐变滑块：宽度滑块之后，无标签，轨道随当前色「透明→不透明」渐变
    QSlider* alphaSlider = nullptr;
    if (spec.withAlpha)
    {
        const QColor baseColor = loadColor(spec.colorKey, spec.palette->first());
        const int storedAlpha = loadInt(spec.alphaKey, spec.defaultAlpha);
        auto* alphaSliderWidget = new ColorAlphaSlider(baseColor, spec.minAlpha, spec.maxAlpha,
                                                       storedAlpha, paramWidget);
        alphaSlider = alphaSliderWidget;
        paramLayout->addWidget(alphaSliderWidget);

        // 拖动：发射透明度变化 + 写回持久化
        connect(alphaSlider, &QSlider::valueChanged, this,
                [this, spec](int value)
        {
            Q_EMIT highlighterAlphaChanged(value);
            m_settings->setValue(spec.alphaKey, value);
        });

        // 切换荧光笔颜色：同步滑块轨道渐变基础色
        if (colorGroup != nullptr)
        {
            connect(colorGroup, &QButtonGroup::idClicked, this,
                    [this, spec, alphaSliderWidget](int colorIndex)
            {
                const QVector<QColor>& alphaPalette = *(spec.palette);
                if ((colorIndex >= 0) && (colorIndex < alphaPalette.size()))
                {
                    alphaSliderWidget->setBaseColor(alphaPalette.at(colorIndex));
                }
            });
        }
    }

    // 登记参数区控件句柄，供 syncParamControlsToTool 按工具刷新显示
    if (handlesOut != nullptr)
    {
        handlesOut->colorGroup = colorGroup;
        handlesOut->widthSlider = widthSlider;
        handlesOut->fillCheck = fillCheck;
        handlesOut->alphaSlider = alphaSlider;
    }

    // 初始隐藏：装入参数框体页栈后由页栈管理可见性
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createTextParam(ParamHandles* handlesOut)
{
    auto* paramWidget = new QWidget(this);
    // objectName 供 QSS 作用域限定：参数页背景透明，露出框体自绘暖色背景
    paramWidget->setObjectName(QStringLiteral("annotationParamPage"));
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

    // 颜色行（标准标注色板，调整传播到所有标注工具）
    QButtonGroup* colorGroup = nullptr;
    paramLayout->addWidget(createColorRow(SK::G_ANNOTATION_COLOR_PALETTE, G_KEY_TEXT_COLOR,
                                          ColorPaletteKind::Annotation, &colorGroup));

    // 字号滑块（8~72，右侧数值带 pt 单位），范围按边界常量
    const int storedFontSize = loadInt(G_KEY_TEXT_FONT_SIZE, G_DEFAULT_FONT_SIZE);
    QSlider* fontSizeSlider = nullptr;
    paramLayout->addWidget(createSliderRow(static_cast<int>(SK::G_MIN_FONT_SIZE),
                                           static_cast<int>(SK::G_MAX_FONT_SIZE),
                                           storedFontSize, &fontSizeSlider,
                                           QStringLiteral("pt")));

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

    // 登记参数区控件句柄，供 syncParamControlsToTool 按工具刷新显示
    if (handlesOut != nullptr)
    {
        handlesOut->colorGroup = colorGroup;
        handlesOut->fontSizeSlider = fontSizeSlider;
        handlesOut->fontCombo = fontCombo;
    }

    // 初始隐藏：装入参数框体页栈后由页栈管理可见性
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createMosaicParam(ParamHandles* handlesOut)
{
    auto* paramWidget = new QWidget(this);
    // objectName 供 QSS 作用域限定：参数页背景透明，露出框体自绘暖色背景
    paramWidget->setObjectName(QStringLiteral("annotationParamPage"));
    auto* paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->setSpacing(G_PANEL_SPACING);

    // 马赛克取背景色，无颜色设置，仅尺寸滑块（10~60，右侧数值带 px 单位）
    const int storedWidth = loadInt(G_KEY_MOSAIC_WIDTH, G_DEFAULT_MOSAIC_WIDTH);
    QSlider* mosaicSlider = nullptr;
    paramLayout->addWidget(createSliderRow(static_cast<int>(SK::G_MIN_MOSAIC_WIDTH),
                                           static_cast<int>(SK::G_MAX_MOSAIC_WIDTH),
                                           storedWidth, &mosaicSlider,
                                           QStringLiteral("px")));

    // 拖动滑块：发射宽度变化 + 写回持久化
    connect(mosaicSlider, &QSlider::valueChanged, this,
            [this](int value)
    {
        Q_EMIT penWidthChanged(static_cast<qreal>(value));
        m_settings->setValue(G_KEY_MOSAIC_WIDTH, value);
    });

    // 登记参数区控件句柄，供 syncParamControlsToTool 按工具刷新显示
    if (handlesOut != nullptr)
    {
        handlesOut->widthSlider = mosaicSlider;
    }

    // 初始隐藏：装入参数框体页栈后由页栈管理可见性
    paramWidget->hide();
    return paramWidget;
}

QWidget* AnnotationToolBar::createGeometryPage()
{
    auto* pageWidget = new QWidget(this);
    // objectName 供 QSS 作用域限定：几何二级页背景透明，露出框体自绘暖色背景
    pageWidget->setObjectName(QStringLiteral("annotationGeometryPage"));
    auto* pageLayout = new QVBoxLayout(pageWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(G_PANEL_SPACING);

    // 4 个图形按钮竖向排列，与一级工具栏按钮列视觉一致
    pageLayout->addWidget(createLevel2Button(G_ICON_LINE, tr("直线"), SK::Tool::Line));
    pageLayout->addWidget(createLevel2Button(G_ICON_ARROW, tr("箭头"), SK::Tool::Arrow));
    pageLayout->addWidget(createLevel2Button(G_ICON_SQUARE, tr("方框"), SK::Tool::Rectangle));
    pageLayout->addWidget(createLevel2Button(G_ICON_CIRCLE, tr("圆"), SK::Tool::Ellipse));

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
        // 几何图形：记录当前图形并级联展开「几何一级 + 几何二级 + 该图形参数三级」，
        // 二级框体与参数框体同时显示
        m_currentGeometryShape = tool;
        m_currentSceneTool = tool;
        m_geometryButton->setChecked(true);
        setLevel2Checked(tool);
        showGeometryPanel();
        // 先确保页索引表已填充（懒创建），避免首次查表命中兜底索引弹错参数页
        ensureParamPanel();
        showParamPanel(m_shapePageIndex.value(static_cast<int>(tool), G_PAGE_SHAPE_BASE));
        // 装载该图形存储参数到场景 + 同步参数区显示
        loadToolParamsToScene(tool);
        syncParamControlsToTool(tool);
        break;
    case SK::Tool::Pen:
    case SK::Tool::Highlighter:
    case SK::Tool::Text:
    case SK::Tool::Mosaic:
        m_currentSceneTool = tool;
        setLevel1Checked(tool);
        hideGeometryPanel();
        showParamPanel(pageIndexOfTool(tool));
        // 装载该工具存储参数到场景 + 同步参数区显示
        loadToolParamsToScene(tool);
        syncParamControlsToTool(tool);
        break;
    default:
        // Select 等非工具栏工具：仅收起弹出框体，不改变高亮与工具状态
        hidePanels();
        return;
    }

    // 同步外部场景，保证场景工具与工具栏状态一致
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::collapseExpanded()
{
    // 保留一级/二级按钮与当前工具，仅隐藏两个弹出框体
    hidePanels();
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
    loadToolParamsToScene(m_currentSceneTool);
}

void AnnotationToolBar::loadToolParamsToScene(SK::Tool tool)
{
    // 描边类工具（水笔/荧光笔/几何图形）：按装配规格读取存储值并补发参数信号
    const StrokeParamSpec* toolSpec = specOfTool(tool);
    if (toolSpec != nullptr)
    {
        Q_EMIT penColorChanged(loadColor(toolSpec->colorKey, toolSpec->palette->first()));
        Q_EMIT penWidthChanged(static_cast<qreal>(
            loadInt(toolSpec->widthKey, toolSpec->defaultWidth)));
        // 方框/圆：填充开关同步场景（未勾选时显式复位为 NoBrush，避免残留上次填充）
        if (toolSpec->withFill)
        {
            const bool storedFilled = loadBool(toolSpec->fillKey, false);
            Q_EMIT brushStyleChanged(storedFilled ? Qt::SolidPattern : Qt::NoBrush);
        }
        // 荧光笔：透明度渐变滑块同步场景
        if (toolSpec->withAlpha)
        {
            Q_EMIT highlighterAlphaChanged(
                loadInt(toolSpec->alphaKey, toolSpec->defaultAlpha));
        }
        return;
    }

    // 文字 / 马赛克：无统一装配规格，单独读取补发
    switch (tool)
    {
    case SK::Tool::Text:
        Q_EMIT penColorChanged(
            loadColor(G_KEY_TEXT_COLOR, SK::G_ANNOTATION_COLOR_PALETTE.first()));
        Q_EMIT fontSizeChanged(static_cast<qreal>(
            loadInt(G_KEY_TEXT_FONT_SIZE, G_DEFAULT_FONT_SIZE)));
        Q_EMIT fontFamilyChanged(
            m_settings->value(G_KEY_TEXT_FONT_FAMILY, G_DEFAULT_FONT_FAMILY).toString());
        break;
    case SK::Tool::Mosaic:
        Q_EMIT penWidthChanged(static_cast<qreal>(
            loadInt(G_KEY_MOSAIC_WIDTH, G_DEFAULT_MOSAIC_WIDTH)));
        break;
    default:
        // Select 等非参数工具：无参数可同步
        break;
    }
}

void AnnotationToolBar::syncParamControlsToTool(SK::Tool tool)
{
    const ParamHandles handles = m_paramHandles.value(static_cast<int>(tool));
    if ((handles.colorGroup == nullptr) && (handles.widthSlider == nullptr)
        && (handles.fillCheck == nullptr) && (handles.fontSizeSlider == nullptr)
        && (handles.fontCombo == nullptr))
    {
        return;  // 参数区尚未创建：跳过同步
    }

    // 描边类工具：按装配规格把存储值同步到色块/滑块/填充控件
    const StrokeParamSpec* toolSpec = specOfTool(tool);
    if (toolSpec != nullptr)
    {
        if (handles.colorGroup != nullptr)
        {
            checkColorInGroup(handles.colorGroup, *(toolSpec->palette),
                              loadColor(toolSpec->colorKey, toolSpec->palette->first()));
        }
        if (handles.widthSlider != nullptr)
        {
            // 阻塞信号：仅刷新显示，不触发参数发射与写回
            const QSignalBlocker widthBlocker(handles.widthSlider);
            handles.widthSlider->setValue(
                loadInt(toolSpec->widthKey, toolSpec->defaultWidth));
        }
        if ((toolSpec->withFill) && (handles.fillCheck != nullptr))
        {
            const bool storedFilled = loadBool(toolSpec->fillKey, false);
            const QSignalBlocker fillBlocker(handles.fillCheck);
            handles.fillCheck->setChecked(storedFilled);
            handles.widthSlider->setEnabled((!storedFilled));
        }
        // 荧光笔：透明度滑块值与轨道渐变基础色随存储值/当前色同步
        if ((toolSpec->withAlpha) && (handles.alphaSlider != nullptr))
        {
            const QSignalBlocker alphaBlocker(handles.alphaSlider);
            handles.alphaSlider->setValue(
                loadInt(toolSpec->alphaKey, toolSpec->defaultAlpha));
            auto* alphaSliderWidget = static_cast<ColorAlphaSlider*>(handles.alphaSlider);
            alphaSliderWidget->setBaseColor(
                loadColor(toolSpec->colorKey, toolSpec->palette->first()));
        }
        return;
    }

    // 文字：色块 / 字号滑块 / 字体选择
    if (tool == SK::Tool::Text)
    {
        if (handles.colorGroup != nullptr)
        {
            checkColorInGroup(handles.colorGroup, SK::G_ANNOTATION_COLOR_PALETTE,
                              loadColor(G_KEY_TEXT_COLOR,
                                        SK::G_ANNOTATION_COLOR_PALETTE.first()));
        }
        if (handles.fontSizeSlider != nullptr)
        {
            const QSignalBlocker sizeBlocker(handles.fontSizeSlider);
            handles.fontSizeSlider->setValue(
                loadInt(G_KEY_TEXT_FONT_SIZE, G_DEFAULT_FONT_SIZE));
        }
        if (handles.fontCombo != nullptr)
        {
            const QSignalBlocker fontBlocker(handles.fontCombo);
            handles.fontCombo->setCurrentFont(
                QFont(m_settings->value(G_KEY_TEXT_FONT_FAMILY, G_DEFAULT_FONT_FAMILY).toString()));
        }
        return;
    }

    // 马赛克：仅尺寸滑块
    if ((tool == SK::Tool::Mosaic) && (handles.widthSlider != nullptr))
    {
        const QSignalBlocker widthBlocker(handles.widthSlider);
        handles.widthSlider->setValue(loadInt(G_KEY_MOSAIC_WIDTH, G_DEFAULT_MOSAIC_WIDTH));
    }
}

void AnnotationToolBar::checkColorInGroup(QButtonGroup* colorGroup,
                                          const QVector<QColor>& palette,
                                          const QColor& color)
{
    if (colorGroup == nullptr)
    {
        return;
    }

    // 在色板中查找与目标颜色一致的色块并勾选（setChecked 不触发 clicked，安全）
    for (int colorIndex = 0; colorIndex < palette.size(); ++colorIndex)
    {
        if (palette.at(colorIndex).name().compare(color.name(), Qt::CaseInsensitive) == 0)
        {
            QAbstractButton* matchedButton = colorGroup->button(colorIndex);
            if (matchedButton != nullptr)
            {
                matchedButton->setChecked(true);
            }
            return;
        }
    }

    // 存储色不在色板：兜底勾选第一个色块，保持互斥组有选中项
    QAbstractButton* firstButton = colorGroup->button(0);
    if (firstButton != nullptr)
    {
        firstButton->setChecked(true);
    }
}

void AnnotationToolBar::onLevel1Clicked(int groupId)
{
    if (groupId == G_GEOMETRY_BUTTON_ID)
    {
        // 每次点击几何按钮，都把当前工具切换为当前默认几何工具：
        // 默认几何图形取自 QSettings annotation/defaultGeometry（点二级时写回），
        // 保证「点几何即用默认图形」，而非沿用上一个正在使用的工具
        const QString geometryName = m_settings->value(
            G_KEY_DEFAULT_GEOMETRY, QStringLiteral("line")).toString();
        m_currentGeometryShape = shapeFromName(geometryName);
        m_currentSceneTool = m_currentGeometryShape;
        m_settings->setValue(G_KEY_DEFAULT_TOOL, QStringLiteral("geometry"));
        setLevel2Checked(m_currentGeometryShape);
        loadToolParamsToScene(m_currentGeometryShape);
        Q_EMIT toolChanged(m_currentGeometryShape);

        if (m_geometryVisible)
        {
            // 启闭循环：二级框体已显示 → 关闭二级（连同三级一起关）
            hideGeometryPanel();
            hideParamPanel();
        }
        else
        {
            // 开二级（不弹三级）：三级参数框体仅由点击二级按钮时弹出（唯一入口），
            // 同时隐藏可能残留的非几何参数框体，避免与几何二级框体重叠
            showGeometryPanel();
            hideParamPanel();
        }
        return;
    }

    // 具体工具：写回默认工具、切换工具并装载该工具实例参数
    const SK::Tool tool = static_cast<SK::Tool>(groupId);
    const int targetPage = pageIndexOfTool(tool);

    // 启闭循环：参数框体已创建、可见且正显示本工具参数页时，
    // 仅收起框体（含几何框体），不重复切换工具、不重复发射信号
    if ((m_paramPanel != nullptr) && (m_paramPanel->isVisible())
        && (m_paramStack != nullptr) && (m_paramStack->currentIndex() == targetPage))
    {
        hideParamPanel();
        hideGeometryPanel();
        return;
    }

    m_currentSceneTool = tool;
    m_settings->setValue(G_KEY_DEFAULT_TOOL, toolNameOf(tool));
    hideGeometryPanel();
    showParamPanel(targetPage);
    // 装载该工具 QSettings 存储参数到场景 + 同步参数区显示
    loadToolParamsToScene(tool);
    syncParamControlsToTool(tool);
    Q_EMIT toolChanged(tool);
}

void AnnotationToolBar::onLevel2Clicked(SK::Tool shape)
{
    m_currentGeometryShape = shape;
    m_currentSceneTool = shape;
    // 点几何二级图形：写回默认几何图形，几何框体保持显示（二三级同时存在），
    // 参数三级框体在几何框体左侧弹出并装载该图形实例参数
    m_settings->setValue(G_KEY_DEFAULT_GEOMETRY, shapeNameOf(shape));
    showGeometryPanel();
    // 先确保页索引表已填充（懒创建），避免首次查表命中兜底索引弹错参数页
    ensureParamPanel();
    showParamPanel(m_shapePageIndex.value(static_cast<int>(shape), G_PAGE_SHAPE_BASE));
    loadToolParamsToScene(shape);
    syncParamControlsToTool(shape);
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

void AnnotationToolBar::ensureGeometryPanel()
{
    if (m_geometryPanel != nullptr)
    {
        return;
    }

    // 几何二级框体挂到工具栏的父控件（m_centralStack）上，与工具栏同级悬浮，
    // 这样才能叠加在中央栈页面上而不被视口遮挡
    QWidget* host = parentWidget();
    if (host == nullptr)
    {
        host = this;
    }
    m_geometryPanel = new PopoutPanel(host);
    m_geometryPanel->hide();
    m_geometryPanel->setFixedWidth(G_GEOMETRY_PANEL_WIDTH);

    auto* panelLayout = new QVBoxLayout(m_geometryPanel);
    panelLayout->setContentsMargins(G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN);
    panelLayout->setSpacing(G_PANEL_SPACING);

    // 几何二级页：4 个图形按钮竖向排列，与一级按钮列视觉一致
    m_geometryPage = createGeometryPage();
    panelLayout->addWidget(m_geometryPage);

    // 高度固定：页内容高 + 面板上下内边距
    m_geometryPanel->setFixedHeight(m_geometryPage->sizeHint().height() + 2 * G_PANEL_MARGIN);

    // 几何二级按钮此刻才创建：若持久化默认工具为几何组，按当前几何图形补高亮
    // （setupUi 阶段按钮尚不存在，setLevel2Checked 找不到按钮会静默跳过）
    if (isGeometryTool(m_currentSceneTool))
    {
        setLevel2Checked(m_currentSceneTool);
    }
}

void AnnotationToolBar::ensureParamPanel()
{
    if (m_paramPanel != nullptr)
    {
        return;
    }

    // 参数三级框体同样挂到工具栏的父控件上，与工具栏/几何框体同级悬浮
    QWidget* host = parentWidget();
    if (host == nullptr)
    {
        host = this;
    }
    m_paramPanel = new PopoutPanel(host);
    m_paramPanel->hide();
    m_paramPanel->setFixedWidth(G_POPOUT_WIDTH);

    auto* panelLayout = new QVBoxLayout(m_paramPanel);
    panelLayout->setContentsMargins(G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN, G_PANEL_MARGIN);
    panelLayout->setSpacing(G_PANEL_SPACING);

    // 页栈：非几何参数页 + 几何图形参数页统一入栈（几何二级页独立于参数框体），
    // addWidget 会自动把页面重新挂到页栈
    m_paramStack = new QStackedWidget(m_paramPanel);
    // objectName 供 QSS 作用域限定：页栈背景透明，露出框体自绘暖色背景
    m_paramStack->setObjectName(QStringLiteral("annotationParamStack"));
    panelLayout->addWidget(m_paramStack);

    m_paramStack->addWidget(m_penParam);            // G_PAGE_PEN
    m_paramStack->addWidget(m_highlighterParam);    // G_PAGE_HIGHLIGHTER
    m_paramStack->addWidget(m_textParam);           // G_PAGE_TEXT
    m_paramStack->addWidget(m_mosaicParam);         // G_PAGE_MOSAIC

    // 几何图形参数页：按固定顺序（直线/箭头/方框/圆）从 G_PAGE_SHAPE_BASE 起排
    const QVector<SK::Tool> shapeOrder = {
        SK::Tool::Line, SK::Tool::Arrow, SK::Tool::Rectangle, SK::Tool::Ellipse
    };
    int shapePage = G_PAGE_SHAPE_BASE;
    for (const SK::Tool shape : shapeOrder)
    {
        m_paramStack->addWidget(m_shapeParams.value(static_cast<int>(shape)));
        m_shapePageIndex.insert(static_cast<int>(shape), shapePage);
        ++shapePage;
    }
}

void AnnotationToolBar::showGeometryPanel()
{
    ensureGeometryPanel();
    // 先置显示标志再定位：updatePanelGeometry 依据 m_geometryVisible 决定参数
    // 框体是否级联到几何框体左侧，不依赖 isVisible()（首次创建尚未 show 时不可靠）
    m_geometryVisible = true;
    updatePanelGeometry();
    m_geometryPanel->show();
    m_geometryPanel->raise();
}

void AnnotationToolBar::showParamPanel(int pageIndex)
{
    ensureParamPanel();
    m_paramStack->setCurrentIndex(pageIndex);

    // 高度随当前页内容自适应：页高 + 面板上下内边距
    QWidget* currentPage = m_paramStack->currentWidget();
    if (currentPage != nullptr)
    {
        const int pageHeight = currentPage->sizeHint().height();
        m_paramPanel->setFixedHeight(pageHeight + 2 * G_PANEL_MARGIN);
    }

    updatePanelGeometry();
    m_paramPanel->show();
    m_paramPanel->raise();
}

void AnnotationToolBar::hideGeometryPanel()
{
    m_geometryVisible = false;
    if (m_geometryPanel != nullptr)
    {
        m_geometryPanel->hide();
    }
}

void AnnotationToolBar::hideParamPanel()
{
    if (m_paramPanel != nullptr)
    {
        m_paramPanel->hide();
    }
}

void AnnotationToolBar::hidePanels()
{
    hideGeometryPanel();
    hideParamPanel();
}

void AnnotationToolBar::updatePanelGeometry()
{
    // 早期返回：两个框体均未创建时无需定位。注意不可只判 paramPanel——
    // 首次显示几何框体时参数框体可能尚未懒创建，若此时返回则几何框体
    // 停留在默认位置（0,0 附近），表现为二级框体跳到最左边
    if ((m_geometryPanel == nullptr) && (m_paramPanel == nullptr))
    {
        return;
    }

    const int panelY = pos().y();

    // 工具栏贴右缘基准 X：正常情况下取工具栏当前 X；工具栏尚未被外部
    // （MainWindow::updateAnnotationToolBarGeometry）布局到右侧时（如刚进入
    // 标注页布局未稳定），按父容器宽度推导贴右缘位置兜底，防止框体跳最左
    const int toolbarX = pos().x();
    int anchorX = toolbarX;
    if (parentWidget() != nullptr)
    {
        const int parentWidth = parentWidget()->width();
        const int expectedToolbarX = parentWidth - width() - G_TOOLBAR_MARGIN;
        if ((parentWidth > 0) && (toolbarX < expectedToolbarX))
        {
            anchorX = expectedToolbarX;
        }
    }

    // 几何二级框体：贴工具栏左侧弹开（工具栏贴右缘，框体向左级联弹出）
    if (m_geometryPanel != nullptr)
    {
        int geometryX = anchorX - G_GEOMETRY_PANEL_WIDTH - G_POPOUT_OFFSET;
        // 防御性钳制：窗口过窄时框体可能越出左缘，钳到工具栏右侧（窄窗口兜底）
        if (geometryX < 0)
        {
            geometryX = anchorX + width() + G_POPOUT_OFFSET;
        }
        m_geometryPanel->move(geometryX, panelY);
    }

    // 参数三级框体：几何框体显示时在其左侧再弹出（二三级级联），
    // 否则贴工具栏左侧。用成员标志 m_geometryVisible 而非 isVisible()：
    // 后者在框体首次创建（尚未 show）时不可靠，会误判导致二三级重叠
    int paramX = anchorX - G_POPOUT_WIDTH - G_POPOUT_OFFSET;
    if ((m_geometryPanel != nullptr) && (m_geometryVisible))
    {
        paramX = m_geometryPanel->pos().x() - G_POPOUT_WIDTH - G_POPOUT_OFFSET;
    }
    // 防御性钳制：窗口过窄时钳到工具栏右侧（窄窗口兜底）
    if (paramX < 0)
    {
        paramX = anchorX + width() + G_POPOUT_OFFSET;
    }
    if (m_paramPanel != nullptr)
    {
        m_paramPanel->move(paramX, panelY);
    }
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