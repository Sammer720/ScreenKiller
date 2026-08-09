/**
 * \file AnnotationToolBar.h
 * \brief 标注工具栏 v4：一级工具竖列 + 右侧弹出独立框体（二三级内容）
 *
 * 设计说明：
 *   - 工具栏本体仅 5 个一级按钮竖排（水笔 / 荧光笔 / 几何 / 文字 / 马赛克），
 *     不含任何内联展开区；几何二级图形页与各工具参数页统一放入工具栏右侧的
 *     独立弹出框体（m_centralStack 子控件悬浮，GuidePanel 同款半透明圆角自绘）。
 *   - 按钮 checked/hover 走全局 QSS 紫色系（#E4D9F0 hover / #B5A5D1 checked），
 *     工具栏不再携带私有 QSS；所有按钮配置 tooltip（调用
 *     ToolButton::setToolTipEnabled(false) 放行拦截）。
 *   - 参数区无文字标签：颜色行仅色块（20x20 可勾选互斥），尺寸一律滑块 + 数值。
 *     双色板：水笔/文字/几何用 G_ANNOTATION_COLOR_PALETTE，
 *     荧光笔用 G_HIGHLIGHTER_COLOR_PALETTE。
 *   - QSettings 持久化（annotation/ 前缀）：点一级工具写 defaultTool、
 *     点几何二级写 defaultGeometry，各图形参数调整即写回；restoreDefaultTool()
 *     截屏完成时从配置恢复并弹出参数框体。
 *   - 标注开始自动收回：collapseExpanded() 隐藏弹出框体（保留当前工具与高亮）。
 *
 * 本组件只负责自身 UI、状态持久化与信号发射，悬浮定位与业务接线由外部完成。
 */
#pragma once

#include <QWidget>
#include <QColor>
#include <QString>
#include <QVector>
#include <QHash>
#include <QObject>    // Q_OBJECT / Q_SIGNALS 宏
#include <QtGlobal>   // qreal
#include <Qt>

#include "annotation/AnnotationScene.h"   // Tool 枚举

class QButtonGroup;
class QPaintEvent;
class QSettings;
class QSlider;
class QStackedWidget;

namespace SK {

class ToolButton;   ///< 前向声明（实现文件再包含完整定义）

/**
 * @brief 标注工具栏（右侧悬浮）——一级工具竖列 + 右侧弹出参数框体
 */
class AnnotationToolBar : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数：构建一级按钮列并读取 QSettings 恢复默认工具/参数状态
     * @param parent 父控件；标注页中应传入中央页栈（QStackedWidget）以叠加在页面上
     */
    explicit AnnotationToolBar(QWidget* parent = nullptr);

    /// @brief 析构函数（默认实现，子控件由 Qt 父子关系自动释放）
    ~AnnotationToolBar() = default;

    /**
     * @brief 从外部同步当前工具（高亮对应一级/二级按钮并弹出其参数框体）
     *
     * Line/Arrow/Rectangle/Ellipse 会自动展开「几何一级 + 二级图形 + 该图形参数页」；
     * Select 等非工具栏工具则收起弹出框体。同步完成后发射 toolChanged，
     * 保证外部场景工具与工具栏状态一致（截屏完成恢复默认工具即依赖此信号）。
     *
     * @param tool 具体场景工具（Pen/Highlighter/Line/Arrow/Rectangle/Ellipse/Text/Mosaic）
     */
    void setCurrentTool(SK::Tool tool);

    /**
     * @brief 收起弹出框体（标注开始创建图元时调用）
     *
     * 一级/二级按钮与当前工具保持不变，仅隐藏框体，不发射任何信号。
     */
    void collapseExpanded();

    /**
     * @brief 从 QSettings 恢复默认工具（截屏完成时调用）
     *
     * 读取 annotation/defaultTool + annotation/defaultGeometry，
     * 经 setCurrentTool 应用并同步外部场景（同时弹出对应参数框体）。
     */
    void restoreDefaultTool();

    /**
     * @brief 描边类参数区装配规格（色板 + 颜色键 + 宽度键 + 滑块边界 + 填充开关）
     *
     * 水笔/荧光笔/直线/箭头/方框/圆共用同一套参数区结构，仅规格不同。
     * 定义为公开类型：实现文件在匿名命名空间中装配各工具的静态规格常量，
     * 私有嵌套类型无法在类外访问，故置于 public 段。
     */
    struct StrokeParamSpec
    {
        const QVector<QColor>* palette = nullptr;  ///< 色板（标注色板或荧光笔色板）
        QString colorKey;                          ///< 颜色持久化键（annotation/ 前缀）
        QString widthKey;                          ///< 宽度持久化键（annotation/ 前缀）
        int minWidth = 1;                          ///< 宽度滑块下限
        int maxWidth = 30;                         ///< 宽度滑块上限
        int defaultWidth = 2;                      ///< 默认宽度（首次使用落值）
        bool withFill = false;                     ///< 是否含填充勾选（仅方框/圆）
        QString fillKey;                           ///< 填充持久化键（withFill 为 true 时有效）
    };

Q_SIGNALS:
    /// @brief 工具切换（用户点击一级/二级按钮或外部 setCurrentTool 时发射）
    /// @param tool 新工具类型
    void toolChanged(SK::Tool tool);
    /// @brief 画笔颜色变化（点击参数区色块时发射）
    /// @param color 新颜色
    void penColorChanged(const QColor& color);
    /// @brief 画笔粗细变化（拖动尺寸滑块时发射）
    /// @param width 新线宽（像素）
    void penWidthChanged(qreal width);
    /// @brief 填充样式变化（仅方框/圆参数区勾选填充时发射）
    /// @param style 填充样式（Qt::SolidPattern / Qt::NoBrush）
    void brushStyleChanged(Qt::BrushStyle style);
    /// @brief 文字字号变化（拖动字号滑块时发射）
    /// @param size 新字号（pt）
    void fontSizeChanged(qreal size);
    /// @brief 文字字体族变化（切换字体选择框时发射）
    /// @param family 新字体族名称
    void fontFamilyChanged(const QString& family);

protected:
    /// @brief 自绘半透明圆角暖色背景（与 GuidePanel 同设计语言）
    /// @param event 绘制事件
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief 构建一级按钮列与全部参数页（参数页延迟装入弹出框体）
    void setupUi();
    /// @brief 创建一级工具图标按钮（互斥组 id = 工具枚举值，几何用独立专用 id）
    /// @param iconResource 图标资源路径（:/icons/xxx.png）
    /// @param toolTipText 按钮提示文本（中文，如「水笔」）
    /// @param groupId 一级互斥组内 id
    /// @return 创建完成的按钮
    SK::ToolButton* createLevel1Button(const QString& iconResource, const QString& toolTipText,
                                       int groupId);
    /// @brief 创建几何二级图形图标按钮（互斥组 id = 图形工具枚举值）
    /// @param iconResource 图标资源路径（:/icons/xxx.png）
    /// @param toolTipText 按钮提示文本（中文，如「直线」）
    /// @param shape 图形工具（Line/Arrow/Rectangle/Ellipse）
    /// @return 创建完成的按钮
    SK::ToolButton* createLevel2Button(const QString& iconResource, const QString& toolTipText,
                                       SK::Tool shape);
    /// @brief 创建颜色行（无文字标签，仅 20x20 可勾选互斥色块）
    /// @param palette 色板（遍历生成色块）
    /// @param settingsKey 颜色持久化键（读入勾选 / 点击写回）
    /// @return 颜色行容器
    QWidget* createColorRow(const QVector<QColor>& palette, const QString& settingsKey);
    /// @brief 创建尺寸滑块行（横向滑块 + 右侧数值标签，无单位文字）
    /// @param minValue 滑块下限
    /// @param maxValue 滑块上限
    /// @param initialValue 初始值（构造期由持久化状态注入）
    /// @param sliderOut 输出参数：返回滑块指针供调用方连接信号/禁用
    /// @return 滑块行容器
    QWidget* createSliderRow(int minValue, int maxValue, int initialValue, QSlider** sliderOut);
    /// @brief 创建描边类参数区（颜色行 + 尺寸滑块，可选填充勾选）
    /// @param spec 装配规格（色板/持久化键/滑块边界/填充开关）
    /// @return 参数区容器（初始隐藏，装入弹出框体页栈）
    QWidget* createStrokeParam(const StrokeParamSpec& spec);
    /// @brief 创建文字参数区（字体选择 + 颜色行 + 字号滑块）
    /// @return 参数区容器（初始隐藏，装入弹出框体页栈）
    QWidget* createTextParam();
    /// @brief 创建马赛克参数区（仅尺寸滑块，颜色取自背景不可配置）
    /// @return 参数区容器（初始隐藏，装入弹出框体页栈）
    QWidget* createMosaicParam();
    /// @brief 创建几何二级图形页（直线/箭头/方框/圆 4 个图标按钮横排）
    /// @return 二级页容器（初始隐藏，装入弹出框体页栈）
    QWidget* createGeometryPage();

    /// @brief 一级按钮点击分发：具体工具弹参数框体 + 发射 toolChanged；几何弹二级页不发射
    /// @param groupId 被点击按钮的互斥组 id
    void onLevel1Clicked(int groupId);
    /// @brief 二级图形按钮点击分发：弹该图形三级参数页并发射 toolChanged
    /// @param shape 被点击的图形工具
    void onLevel2Clicked(SK::Tool shape);
    /// @brief 按工具高亮对应一级按钮（几何由扩展点单独处理）
    /// @param tool 当前工具
    void setLevel1Checked(SK::Tool tool);
    /// @brief 按图形高亮对应二级按钮（找不到时静默）
    /// @param shape 当前几何图形
    void setLevel2Checked(SK::Tool shape);

    /// @brief 懒创建弹出框体（首次显示时创建，装入全部参数页与几何二级页）
    void ensurePopoutPanel();
    /// @brief 显示弹出框体并切到指定页（懒创建 + 定位 + 置顶）
    /// @param pageIndex 弹出框体页栈页索引
    void showPopoutPanel(int pageIndex);
    /// @brief 隐藏弹出框体（保留当前工具与高亮）
    void hidePopoutPanel();
    /// @brief 按工具栏当前位置重新定位弹出框体（贴着工具栏右侧，越界时左移钳制）
    void updatePopoutGeometry();

    /// @brief 读取持久化颜色（无记录或非法时回退默认色）
    /// @param settingsKey 颜色持久化键
    /// @param fallbackColor 回退颜色
    /// @return 读取到的颜色
    QColor loadColor(const QString& settingsKey, const QColor& fallbackColor);
    /// @brief 读取持久化整数（无记录时回退默认值）
    /// @param settingsKey 整数持久化键
    /// @param fallbackValue 回退值
    /// @return 读取到的整数
    int loadInt(const QString& settingsKey, int fallbackValue);
    /// @brief 读取持久化布尔（无记录时回退默认值）
    /// @param settingsKey 布尔持久化键
    /// @param fallbackValue 回退值
    /// @return 读取到的布尔值
    bool loadBool(const QString& settingsKey, bool fallbackValue);

    QSettings*        m_settings = nullptr;          ///< 配置读写（默认构造，INI 格式）
    QButtonGroup*     m_level1Group = nullptr;       ///< 一级工具按钮互斥组
    QButtonGroup*     m_level2Group = nullptr;       ///< 几何二级图形按钮互斥组
    SK::ToolButton*   m_penButton = nullptr;         ///< 一级：水笔按钮
    SK::ToolButton*   m_highlighterButton = nullptr; ///< 一级：荧光笔按钮
    SK::ToolButton*   m_geometryButton = nullptr;    ///< 一级：几何按钮
    SK::ToolButton*   m_textButton = nullptr;        ///< 一级：文字按钮
    SK::ToolButton*   m_mosaicButton = nullptr;      ///< 一级：马赛克按钮
    QWidget*          m_penParam = nullptr;          ///< 水笔参数页（颜色 + 尺寸）
    QWidget*          m_highlighterParam = nullptr;  ///< 荧光笔参数页（颜色 + 尺寸）
    QWidget*          m_geometryPage = nullptr;      ///< 几何二级页（直线/箭头/方框/圆横排）
    QWidget*          m_textParam = nullptr;         ///< 文字参数页（字体 + 颜色 + 字号）
    QWidget*          m_mosaicParam = nullptr;       ///< 马赛克参数页（仅尺寸）
    QHash<int, QWidget*> m_shapeParams;              ///< 几何图形参数页：图形 Tool 枚举 → 参数页
    QHash<int, int>   m_shapePageIndex;              ///< 几何图形参数页：图形 Tool 枚举 → 页索引
    QWidget*          m_popoutPanel = nullptr;       ///< 弹出框体（懒创建，parent = parentWidget()）
    QStackedWidget*   m_popoutStack = nullptr;       ///< 弹出框体内页栈（参数页/几何二级页）
    SK::Tool          m_currentGeometryShape = SK::Tool::Line;  ///< 当前几何图形（默认直线）
    SK::Tool          m_currentSceneTool = SK::Tool::Pen;       ///< 当前场景工具（默认水笔）
};

} // namespace SK