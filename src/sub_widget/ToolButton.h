/**
 * \file ToolButton.h
 * \brief 支持图标与文本间距控制、状态图标切换的 QToolButton 子类
 *
 * 设计说明：
 *   1. 间距：QToolButton 图标与文本间距由 QStyle 内部计算，QSS 无法直接控制，
 *      本子类通过重写 paintEvent() 手动布局「图标 + 文本」，使间距完全可控。
 *   2. 状态图标：通过事件重写（enter/leave/press/release）驱动 setIcon 切换
 *      默认 / 悬停 / 按下激活三种图标，无需 QSS。
 *   3. Tooltip：重写 event() 拦截 QEvent::ToolTip，阻止按钮显示任何 tooltip。
 */
#pragma once

#include <QToolButton>

class QMenu;

namespace SK {

/**
 * @brief 图标与文本间距可控、支持三态图标切换的工具栏按钮
 */
class ToolButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(int iconTextSpacing READ iconTextSpacing WRITE setIconTextSpacing)
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父窗口
     */
    explicit ToolButton(QWidget* parent = nullptr);

    /// @brief 析构
    ~ToolButton() = default;

    /**
     * @brief 设置图标与文本之间的像素间距
     * @param spacing 间距（像素，非负）
     */
    void setIconTextSpacing(int spacing);

    /**
     * @brief 获取图标与文本之间的像素间距
     * @return 当前间距（像素）
     */
    int iconTextSpacing() const;

    /**
     * @brief 注册三态图标并由事件驱动自动切换
     * @param normal   默认图标
     * @param hover    悬停图标（_hover）
     * @param selected 按下/激活图标（_selected）
     *
     * 切换规则：
     *   - 左键按下 / 菜单打开 → selected
     *   - 鼠标悬停 → hover
     *   - 其余 → normal
     */
    void setTriStateIcons(const QIcon& normal,
                          const QIcon& hover,
                          const QIcon& selected);

    /**
     * @brief 关联弹出菜单并连接其关闭信号（复位按下态图标）
     * @param menu 弹出菜单；QToolButton::setMenu 非虚，故用包装方法
     */
    void attachMenu(QMenu* menu);

    /**
     * @brief 设置是否拦截 tooltip 事件（默认 true=拦截，主工具栏按钮不显示 tooltip）
     * @param enabled true=拦截（保持现状）；false=放行，setToolTip 生效
     */
    void setToolTipEnabled(bool enabled);

protected:
    /**
     * @brief 事件入口：拦截 QEvent::ToolTip 阻止 tooltip 显示
     */
    bool event(QEvent* event) override;

    /// @brief 鼠标进入：切到悬停图标
    void enterEvent(QEnterEvent* event) override;
    /// @brief 鼠标离开：复位为默认图标
    void leaveEvent(QEvent* event) override;
    /// @brief 左键按下：切到按下激活图标
    void mousePressEvent(QMouseEvent* event) override;
    /// @brief 左键释放：按当前悬停状态恢复图标
    void mouseReleaseEvent(QMouseEvent* event) override;

    /**
     * @brief 自绘：先由样式绘制按钮外壳，再手动布局图标与文本
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 尺寸提示：将自定义间距与默认间距的差值补偿到宽度
     */
    QSize sizeHint() const override;

private:
    /// @brief 按当前鼠标状态应用对应图标
    void updateIconByState();

    int  m_iconTextSpacing = 8;  ///< 图标与文本间距（像素）
    QIcon m_iconNormal;          ///< 默认图标
    QIcon m_iconHover;           ///< 悬停图标（_hover）
    QIcon m_iconSelected;        ///< 按下/激活图标（_selected）
    bool m_triStateEnabled = false;  ///< 是否启用了三态图标
    bool m_hovered = false;          ///< 鼠标是否悬停
    bool m_pressed = false;          ///< 左键是否按下/菜单是否打开
    bool m_blockToolTip = true;      ///< 是否拦截 tooltip（默认拦截）
};

} // namespace SK