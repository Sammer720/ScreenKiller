/**
 * \file GuidePanel.h
 * \brief 标注页悬浮引导面板
 *
 * 半透明背景，展示鼠标操作提示、快捷键提示和当前缩放比例。
 * 点击可折叠为小浮动控件，再次点击展开。
 *
 * 内容区采用 QGridLayout 网格而非富文本表格：
 * 富文本表格会把跨列单元格的内容宽度均摊到被跨的各列上，
 * 导致「Del + Del + Del」等较长文本把图标列撑宽；网格布局用
 * 固定图标列 + 拉伸文本列精确控制，图标列恒为 32px。
 *
 * 设计说明：
 *   作为中央页栈（QStackedWidget）的子控件叠加在标注页角落，
 *   鼠标左键点击整块面板区域即可切换 折叠 / 展开 两种形态。
 */
#pragma once

#include <QWidget>

class QGridLayout;
class QLabel;
class QMouseEvent;
class QPaintEvent;

namespace SK {

/**
 * @brief 标注页悬浮引导面板
 *
 * 半透明圆角卡片，内容布局：
 *   - 操作提示区：4 行图标提示（拖动平移 / 滚动缩放 / 点击复位 / 右键复制）
 *     + 5 行快捷键提示（Ctrl+S / Del+Del / Del+Del+Del / Ctrl+Z / Ctrl+Y）
 *   - 底部一行：左侧“点击隐藏”小字提示 + 右侧当前视图缩放百分比
 *
 * 整块面板左键点击可折叠为小浮动控件，再次点击展开。
 */
class GuidePanel : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父控件；标注页中应传入中央页栈（QStackedWidget）以叠加在页面上
     */
    explicit GuidePanel(QWidget* parent = nullptr);

    /// @brief 析构函数（默认实现，子控件由 Qt 父子关系自动释放）
    ~GuidePanel() = default;

    /// @brief 更新缩放比例显示
    /// @param scale 当前缩放因子（1.0 = 100%）
    void setZoomScale(qreal scale);

protected:
    /// @brief 鼠标点击：切换折叠/展开
    void mousePressEvent(QMouseEvent* event) override;
    /// @brief 自绘半透明圆角背景
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief 切换折叠/展开状态
    void toggleCollapsed();

    /**
     * @brief 构建操作提示行网格：图标行（图标 + 操作 + 含义）与快捷键行（按键跨列 + 含义）
     * @param guideGrid 内容容器的网格布局
     */
    void buildGuideRows(QGridLayout* guideGrid);

    QWidget* m_contentWidget = nullptr; ///< 操作提示内容容器（网格布局承载各行）
    QLabel*  m_hintLabel     = nullptr; ///< 左下角「点击隐藏」小字提示
    QLabel*  m_zoomLabel     = nullptr; ///< 缩放比例显示
    bool     m_collapsed     = false;   ///< 是否折叠状态
};

} // namespace SK