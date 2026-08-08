/**
 * \file AnnotationToolBar.h
 * \brief 标注工具栏（标注页右侧悬浮）——伸缩式（手风琴）工具按钮
 *
 * 设计说明：
 *   - 8 个互斥的 SK::ToolButton（emoji 文本占位，待替换为图标资源），
 *     每个按钮正下方内联一个参数区 QWidget（色板 / 粗细 / 填充 / 字号 / 字体）。
 *   - 手风琴式展开：点击工具按钮，仅该工具的参数区展开，其余一律收起；
 *     参数区随按钮顺序内联在布局中，不使用 QStackedWidget 独立属性面板。
 *   - 半透明暖色圆角背景与 GuidePanel 同设计语言（paintEvent 自绘）。
 *   - 通过 toolChanged / penColorChanged / penWidthChanged /
 *     brushStyleChanged / fontSizeChanged / fontFamilyChanged 信号
 *     把用户操作通知给外部（AnnotationView / MainWindow）。
 *
 * 本组件只负责自身 UI 与信号发射，悬浮定位与业务接线由外部完成。
 */
#pragma once

#include <QWidget>
#include <QColor>
#include <QString>
#include <QVector>
#include <QHash>
#include <Qt>

#include "annotation/AnnotationScene.h"   // Tool 枚举

class QButtonGroup;
class QCheckBox;
class QSpinBox;
class QFontComboBox;
class QPaintEvent;

namespace SK {

class ToolButton;   ///< 前向声明（实现文件再包含完整定义）

/**
 * @brief 标注工具栏（右侧悬浮）——伸缩式（手风琴）工具按钮 + 内联参数区
 */
class AnnotationToolBar : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父控件；标注页中应传入中央页栈（QStackedWidget）以叠加在页面上
     */
    explicit AnnotationToolBar(QWidget* parent = nullptr);

    /// @brief 析构函数（默认实现，子控件由 Qt 父子关系自动释放）
    ~AnnotationToolBar() = default;

    /**
     * @brief 从外部同步当前工具（如截屏完成默认画笔时高亮按钮并展开其参数区）
     * @param tool 工具类型
     */
    void setCurrentTool(SK::Tool tool);

Q_SIGNALS:
    /// @brief 工具切换
    /// @param tool 新工具类型
    void toolChanged(SK::Tool tool);
    /// @brief 画笔颜色变化
    /// @param color 新颜色
    void penColorChanged(const QColor& color);
    /// @brief 画笔粗细变化
    /// @param width 新线宽（像素）
    void penWidthChanged(qreal width);
    /// @brief 填充样式变化（仅矩形/椭圆参数区勾选填充时发射）
    /// @param style 填充样式（Qt::SolidPattern / Qt::NoBrush）
    void brushStyleChanged(Qt::BrushStyle style);
    /// @brief 文字字号变化
    /// @param size 新字号（pt）
    void fontSizeChanged(qreal size);
    /// @brief 文字字体族变化
    /// @param family 新字体族名称
    void fontFamilyChanged(const QString& family);

protected:
    /// @brief 自绘半透明圆角暖色背景（与 GuidePanel 同设计语言）
    /// @param event 绘制事件
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief 构建整体布局与全部子控件
    void setupUi();
    /// @brief 创建单个互斥工具按钮（emoji 文本占位）
    /// @param emojiText emoji 占位文本（待图标资源替换）
    /// @param tool 对应工具类型
    SK::ToolButton* createToolButton(const QString& emojiText, SK::Tool tool);
    /// @brief 按工具类型创建其参数区（色板 / 粗细 / 填充 / 字号 / 字体）
    /// @param tool 工具类型
    QWidget* createParamWidget(SK::Tool tool);
    /// @brief 创建色板按钮行（遍历 G_COLOR_PALETTE）
    /// @param parent 父控件（参数区）
    QWidget* createColorRow(QWidget* parent);
    /// @brief 创建粗细档位按钮行
    /// @param parent 父控件（参数区）
    /// @param widthSteps 档位取值序列
    QWidget* createWidthRow(QWidget* parent, const QVector<qreal>& widthSteps);
    /// @brief 创建「色板 + 粗细档位」参数区（可选填充勾选）
    /// @param parent 父控件
    /// @param widthSteps 粗细档位取值序列
    /// @param fillCheckOut 输出参数：创建填充勾选框时返回其指针，否则保持原值
    QWidget* createStrokeParam(QWidget* parent, const QVector<qreal>& widthSteps, QCheckBox** fillCheckOut);
    /// @brief 创建文字参数区（色板 + 字号 + 字体族）
    /// @param parent 父控件
    QWidget* createTextParam(QWidget* parent);
    /// @brief 创建马赛克参数区（仅粗细档位，无颜色设置）
    /// @param parent 父控件
    QWidget* createMosaicParam(QWidget* parent);
    /// @brief 工具按钮点击处理：同步高亮、展开参数区并发射 toolChanged
    /// @param tool 被点击按钮对应工具
    void onToolButtonClicked(SK::Tool tool);
    /// @brief 填充勾选状态变化处理：同步矩形/椭圆两页勾选框并发射 brushStyleChanged
    /// @param checked 是否勾选填充
    void onFillToggled(bool checked);
    /// @brief 将当前填充状态同步到当前参数区的填充勾选框（避免矩形/椭圆页状态不一致）
    void syncFillCheckState();

    QButtonGroup*  m_toolButtonGroup = nullptr;  ///< 工具按钮互斥组
    QHash<int, QWidget*> m_paramWidgets;         ///< 工具枚举值 → 参数区（手风琴展开页）
    QCheckBox*     m_rectFillCheck = nullptr;    ///< 矩形参数区填充勾选框
    QCheckBox*     m_ellipseFillCheck = nullptr; ///< 椭圆参数区填充勾选框
    bool           m_fillChecked = false;        ///< 当前填充开关状态（矩形/椭圆共享）
    SK::Tool       m_currentTool = SK::Tool::Pen; ///< 当前工具（默认画笔，无 Select 工具）
};

} // namespace SK