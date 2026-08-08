/**
 * \file AnnotationToolBar.h
 * \brief 标注工具栏（标注页右侧悬浮）——工具选择 + 动态属性面板
 *
 * 设计说明：
 *   - 上部为 8 个互斥的工具按钮（emoji 文本占位，待替换为图标资源）
 *   - 下部为 QStackedWidget 动态属性面板，随选中工具切换：
 *       选择页为空；画笔/荧光笔/马赛克/直线/箭头/矩形/椭圆页为
 *       色板 + 粗细档位（矩形/椭圆额外含填充勾选）；文字页为
 *       色板 + 字号 + 字体族。
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
#include <Qt>

#include "annotation/AnnotationScene.h"   // Tool 枚举

class QToolButton;
class QButtonGroup;
class QStackedWidget;
class QCheckBox;
class QSpinBox;
class QFontComboBox;

namespace SK {

/**
 * @brief 标注工具栏（右侧悬浮）——工具选择 + 动态属性面板
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
     * @brief 从外部同步当前工具（如快捷键切换时高亮对应按钮并切换属性页）
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
    /// @brief 填充样式变化（仅矩形/椭圆页勾选填充时发射）
    /// @param style 填充样式（Qt::SolidPattern / Qt::NoBrush）
    void brushStyleChanged(Qt::BrushStyle style);
    /// @brief 文字字号变化
    /// @param size 新字号（pt）
    void fontSizeChanged(qreal size);
    /// @brief 文字字体族变化
    /// @param family 新字体族名称
    void fontFamilyChanged(const QString& family);

private:
    /// @brief 构建整体布局与全部子控件
    void setupUi();
    /// @brief 创建单个互斥工具按钮
    /// @param emojiText emoji 文本占位（待图标资源替换）
    /// @param tooltip 按钮提示文本
    /// @param tool 对应工具类型
    QToolButton* createToolButton(const QString& emojiText, const QString& tooltip, SK::Tool tool);
    /// @brief 按 Tool 枚举顺序构建属性页栈
    void setupPropertyPages();
    /// @brief 创建空属性页（选择工具无属性）
    QWidget* createEmptyPage();
    /// @brief 创建「色板 + 粗细档位」属性页
    /// @param widthSteps 粗细档位取值序列（按对应工具边界预置）
    /// @param fillCheckOut 输出参数：创建填充勾选框时返回其指针（矩形/椭圆页），否则保持原值
    QWidget* createStrokePage(const QVector<qreal>& widthSteps, QCheckBox** fillCheckOut);
    /// @brief 创建文字属性页（色板 + 字号 + 字体族）
    QWidget* createTextPage();
    /// @brief 创建色板按钮行（遍历 G_COLOR_PALETTE）
    /// @param parent 父控件（属性页）
    QWidget* createColorRow(QWidget* parent);
    /// @brief 创建粗细档位按钮行
    /// @param parent 父控件（属性页）
    /// @param widthSteps 档位取值序列
    QWidget* createWidthRow(QWidget* parent, const QVector<qreal>& widthSteps);
    /// @brief 工具按钮点击处理：同步高亮、切换属性页并发射 toolChanged
    /// @param tool 被点击按钮对应工具
    void onToolButtonClicked(SK::Tool tool);
    /// @brief 填充勾选状态变化处理：同步兄弟页勾选框并发射 brushStyleChanged
    /// @param checked 是否勾选填充
    void onFillToggled(bool checked);
    /// @brief 将当前填充状态同步到当前页的填充勾选框（避免矩形/椭圆页状态不一致）
    void syncFillCheckState();

    QButtonGroup* m_toolButtonGroup = nullptr;  ///< 工具按钮互斥组
    QStackedWidget* m_propertyStack = nullptr;  ///< 动态属性面板页栈（index 与 Tool 枚举一致）
    QCheckBox* m_rectFillCheck = nullptr;       ///< 矩形页填充勾选框
    QCheckBox* m_ellipseFillCheck = nullptr;    ///< 椭圆页填充勾选框
    bool m_fillChecked = false;                 ///< 当前填充开关状态（矩形/椭圆页共享）
    SK::Tool m_currentTool = SK::Tool::Select;  ///< 当前工具（用于填充状态同步）
};

} // namespace SK