/**
 * \file AnnotationToolBar.h
 * \brief 标注工具栏 v6：一级工具竖列 + 左侧双框体级联（几何二级 + 参数三级）
 *
 * 设计说明：
 *   - 工具栏本体仅 5 个一级按钮竖排（水笔 / 荧光笔 / 几何 / 文字 / 马赛克），
 *     不含任何内联展开区；二三级拆为两个独立弹出框体（PopoutPanel 自绘半透明
 *     圆角暖色背景，GuidePanel 同设计语言）向左级联弹出。
 *   - 双框体交互：m_geometryPanel 几何二级框体（4 个图形按钮竖向）贴工具栏
 *     左侧；m_paramPanel 参数三级框体（各工具参数页）在几何框体左侧再弹出；
 *     无几何展开时参数框体贴工具栏左侧。三级参数框体只由点击二级框体中的
 *     图形按钮时弹出（唯一入口）；点击一级几何按钮只开/关二级框体，不直接
 *     弹三级。
 *   - 一级几何启闭循环：点击几何→弹二级框体；再次点击几何→关二级（连同
 *     三级一起关）。每次点击几何按钮都将当前工具切换为当前默认几何工具
 *     （来源 QSettings annotation/defaultGeometry），而非上一个使用的工具。
 *   - 切换工具即装载参数：onLevel1Clicked / onLevel2Clicked / setCurrentTool
 *     切换工具后调用 loadToolParamsToScene() 读取该工具 QSettings 存储值并
 *     补发全部参数信号，使场景立即装载该工具实例参数（修复"切荧光笔沿用上一
 *     工具参数"的缺陷）；syncParamControlsToTool() 同时把存储值同步到参数区
 *     控件（色块勾选 / 滑块数值 / 填充勾选 / 字号 / 字体）。
 *   - 标准标注色板（G_ANNOTATION_COLOR_PALETTE）调整传播到所有应用该色板的
 *     工具（水笔 / 直线 / 箭头 / 方框 / 圆 / 文字）；荧光笔色板独立不传播；
 *     尺寸 / 填充等参数独立不传播。
 *   - QSettings 持久化（annotation/ 前缀）：点一级写 defaultTool、点几何二级写
 *     defaultGeometry，各图形参数调整即写回；restoreDefaultTool() 截屏完成时
 *     从配置恢复工具/参数状态并同步场景，但不弹框体（仅点击弹开）。
 *   - 参数框体内滑块 / 复选框 / 数值标签等控件走主界面紫色系（滑块 groove
 *     #D9CCEE / 手柄 #8B7AB8，勾选框白底紫边、勾选 #B5A5D1 + 勾号图标），
 *     通过 QSS 作用域限定：弹出框体（PopoutPanel objectName =
 *     annotationPopoutPanel）内的控件走 windows11_light.qss 的
 *     annotationPopoutPanel 作用域规则，不覆盖主界面紫色系控件；二级页 /
 *     参数页 / 行列容器背景显式透明，露出框体自绘暖色背景；色块按钮保留
 *     功能性内联样式（背景色填充 + 圆角 + 边框）。
 *   - 框体显示状态用成员标志 m_geometryVisible 跟踪（不依赖 isVisible()），
 *     updatePanelGeometry 据此级联定位参数框体，避免框体首次创建即显示时
 *     因可见性时序误判导致二三级框体重叠。
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
class QCheckBox;
class QFontComboBox;
class QPaintEvent;
class QSettings;
class QSlider;
class QStackedWidget;

namespace SK {

class ToolButton;   ///< 前向声明（实现文件再包含完整定义）

/**
 * @brief 标注工具栏（右侧悬浮）——一级工具竖列 + 左侧双框体级联（几何二级 + 参数三级）
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
     * @brief 从外部同步当前工具（高亮对应一级/二级按钮并级联弹出其参数框体）
     *
     * Line/Arrow/Rectangle/Ellipse 会自动展开「几何一级 + 几何二级 + 该图形参数
     * 三级」；Select 等非工具栏工具则收起弹出框体。切换后装载该工具存储参数到
     * 场景并发射 toolChanged，保证外部场景工具与工具栏状态一致。截屏完成恢复
     * 默认工具不走本路径（restoreDefaultTool 只恢复状态不弹框体）。
     *
     * @param tool 具体场景工具（Pen/Highlighter/Line/Arrow/Rectangle/Ellipse/Text/Mosaic）
     */
    void setCurrentTool(SK::Tool tool);

    /**
     * @brief 收起弹出框体（标注开始创建图元时调用）
     *
     * 一级/二级按钮与当前工具保持不变，仅隐藏两个框体，不发射任何信号。
     */
    void collapseExpanded();

    /**
     * @brief 从 QSettings 恢复默认工具（截屏完成时调用）
     *
     * 读取 annotation/defaultTool + annotation/defaultGeometry，
     * 恢复工具/参数状态并同步外部场景（发射 toolChanged + 当前工具参数信号），
     * 但不弹框体——二三级框体仅由用户点击一级/二级按钮时弹开。
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
    /**
     * @brief 色板类型（决定颜色调整是否传播到其他工具）
     *
     * 标注色板调整会传播到所有应用该色板的工具；荧光笔色板独立，不传播。
     */
    enum class ColorPaletteKind
    {
        Annotation,   ///< 标准标注色板（水笔/直线/箭头/方框/圆/文字共用）
        Highlighter   ///< 荧光笔色板（独立，调整不传播）
    };

    /**
     * @brief 参数区控件句柄（syncParamControlsToTool 按工具更新控件显示）
     *
     * 各参数页创建时把内部控件登记到句柄，切换工具时按该工具 QSettings
     * 存储值刷新控件显示（色块勾选 / 滑块数值 / 填充勾选 / 字号 / 字体）。
     */
    struct ParamHandles
    {
        QButtonGroup* colorGroup = nullptr;   ///< 色块互斥组（按色板索引查色块按钮）
        QSlider* widthSlider = nullptr;       ///< 宽度滑块（描边类工具 / 马赛克）
        QCheckBox* fillCheck = nullptr;       ///< 填充勾选（仅方框/圆）
        QSlider* fontSizeSlider = nullptr;    ///< 字号滑块（仅文字）
        QFontComboBox* fontCombo = nullptr;   ///< 字体选择（仅文字）
    };

    /// @brief 构建一级按钮列与全部参数页（参数页延迟装入参数框体页栈）
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
    /// @param paletteKind 色板类型（标注色板调整传播到其他工具，荧光笔独立）
    /// @param colorGroupOut 输出参数：返回色块互斥组供控件同步
    /// @return 颜色行容器
    QWidget* createColorRow(const QVector<QColor>& palette, const QString& settingsKey,
                            ColorPaletteKind paletteKind, QButtonGroup** colorGroupOut = nullptr);
    /// @brief 创建尺寸滑块行（横向滑块 + 右侧数值标签，无单位文字）
    /// @param minValue 滑块下限
    /// @param maxValue 滑块上限
    /// @param initialValue 初始值（构造期由持久化状态注入）
    /// @param sliderOut 输出参数：返回滑块指针供调用方连接信号/禁用
    /// @return 滑块行容器
    QWidget* createSliderRow(int minValue, int maxValue, int initialValue, QSlider** sliderOut);
    /// @brief 创建描边类参数区（颜色行 + 尺寸滑块，可选填充勾选）
    /// @param spec 装配规格（色板/持久化键/滑块边界/填充开关）
    /// @param handlesOut 输出参数：登记参数区控件句柄（可空）
    /// @return 参数区容器（初始隐藏，装入参数框体页栈）
    QWidget* createStrokeParam(const StrokeParamSpec& spec, ParamHandles* handlesOut = nullptr);
    /// @brief 创建文字参数区（字体选择 + 颜色行 + 字号滑块）
    /// @param handlesOut 输出参数：登记参数区控件句柄（可空）
    /// @return 参数区容器（初始隐藏，装入参数框体页栈）
    QWidget* createTextParam(ParamHandles* handlesOut = nullptr);
    /// @brief 创建马赛克参数区（仅尺寸滑块，颜色取自背景不可配置）
    /// @param handlesOut 输出参数：登记参数区控件句柄（可空）
    /// @return 参数区容器（初始隐藏，装入参数框体页栈）
    QWidget* createMosaicParam(ParamHandles* handlesOut = nullptr);
    /// @brief 创建几何二级图形页（直线/箭头/方框/圆 4 个图标按钮竖向排列）
    /// @return 二级页容器（装入几何二级框体）
    QWidget* createGeometryPage();

    /// @brief 一级按钮点击分发：具体工具切页 + 装载参数 + 发射 toolChanged；
    ///        几何分支开/关二级框体（启闭循环）并切换当前工具为默认几何工具
    /// @param groupId 被点击按钮的互斥组 id
    void onLevel1Clicked(int groupId);
    /// @brief 二级图形按钮点击分发：弹该图形三级参数页（三级唯一入口）并发射 toolChanged
    /// @param shape 被点击的图形工具
    void onLevel2Clicked(SK::Tool shape);
    /// @brief 按工具高亮对应一级按钮（几何由扩展点单独处理）
    /// @param tool 当前工具
    void setLevel1Checked(SK::Tool tool);
    /// @brief 按图形高亮对应二级按钮（找不到时静默）
    /// @param shape 当前几何图形
    void setLevel2Checked(SK::Tool shape);

    /// @brief 懒创建几何二级框体（4 个图形按钮竖向，贴工具栏左侧）
    void ensureGeometryPanel();
    /// @brief 懒创建参数三级框体（各工具参数页 + 几何图形参数页页栈）
    void ensureParamPanel();
    /// @brief 显示几何二级框体（懒创建 + 重定位 + 置顶）
    void showGeometryPanel();
    /// @brief 显示参数三级框体并切到指定页（懒创建 + 自适应高度 + 重定位 + 置顶）
    /// @param pageIndex 参数框体页栈页索引
    void showParamPanel(int pageIndex);
    /// @brief 隐藏几何二级框体（参数框体不受影响）
    void hideGeometryPanel();
    /// @brief 隐藏参数三级框体（几何框体不受影响，仅隐藏不重置任何状态）
    void hideParamPanel();
    /// @brief 隐藏两个弹出框体（保留当前工具与高亮）
    void hidePanels();
    /// @brief 按工具栏当前位置级联定位两个框体（几何贴工具栏左侧，参数在几何左侧）
    void updatePanelGeometry();

    /// @brief 按指定工具读取 QSettings 存储值补发全部参数信号（同步外部场景参数实例）
    /// @param tool 目标工具（Pen/Highlighter/Line/Arrow/Rectangle/Ellipse/Text/Mosaic）
    void loadToolParamsToScene(SK::Tool tool);
    /// @brief 按指定工具把 QSettings 存储值同步到参数区控件显示（色块/滑块/填充/字号/字体）
    /// @param tool 目标工具；参数区尚未创建时静默跳过
    void syncParamControlsToTool(SK::Tool tool);
    /// @brief 在色块互斥组内勾选与目标颜色一致的色块（颜色不在色板时兜底勾选第一个）
    /// @param colorGroup 色块互斥组
    /// @param palette 该色组对应的色板（按索引对应色块）
    /// @param color 目标颜色
    static void checkColorInGroup(QButtonGroup* colorGroup, const QVector<QColor>& palette,
                                  const QColor& color);

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
    QWidget*          m_geometryPage = nullptr;      ///< 几何二级页（直线/箭头/方框/圆竖排）
    QWidget*          m_textParam = nullptr;         ///< 文字参数页（字体 + 颜色 + 字号）
    QWidget*          m_mosaicParam = nullptr;       ///< 马赛克参数页（仅尺寸）
    QHash<int, QWidget*> m_shapeParams;              ///< 几何图形参数页：图形 Tool 枚举 → 参数页
    QHash<int, int>   m_shapePageIndex;              ///< 几何图形参数页：图形 Tool 枚举 → 页索引
    QHash<int, ParamHandles> m_paramHandles;         ///< 工具 Tool 枚举 → 参数区控件句柄
    QWidget*          m_geometryPanel = nullptr;     ///< 几何二级框体（懒创建，parent = parentWidget()）
    QWidget*          m_paramPanel = nullptr;        ///< 参数三级框体（懒创建，parent = parentWidget()）
    QStackedWidget*   m_paramStack = nullptr;        ///< 参数框体内页栈（各工具参数页）
    bool              m_geometryVisible = false;      ///< 几何二级框体是否处于显示状态（定位参数框体用）
    SK::Tool          m_currentGeometryShape = SK::Tool::Line;  ///< 当前几何图形（默认直线）
    SK::Tool          m_currentSceneTool = SK::Tool::Pen;       ///< 当前场景工具（默认水笔）
};

} // namespace SK