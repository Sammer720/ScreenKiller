/**
 * \file ToolBar.h
 * \brief 顶部工具栏（兼作自定义标题栏）
 *
 * 设计说明：
 *   由于主窗口采用 FramelessWindowHint 无系统标题栏，
 *   本工具栏同时承担窗口拖拽功能——在空白区域按下左键即可拖动窗口。
 *
 * 包含控件：
 *   - 截屏按钮（主操作）
 *   - 截屏模式下拉菜单（画框 / 窗口 / 全屏 / 滚动）
 *   - 保存按钮（截屏完成后显示）
 *   - 最小化按钮
 *   - 关闭按钮
 */
#pragma once

#include <QToolBar>

class QAction;
class QToolButton;

namespace SK {

class ToolBar : public QToolBar
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父控件；为 nullptr 时作为独立窗口
     */
    explicit ToolBar(QWidget* parent = nullptr);

    /// @brief 显示/隐藏保存按钮（截屏完成后显示）
    /// @param visible 是否可见
    void setShowSaveButton(bool visible);

Q_SIGNALS:
    /// @brief 截屏按钮点击
    void captureClicked();
    /// @brief 截屏模式切换
    /// @param mode 0=Region(画框), 1=FullScreen, 2=Window, 3=Scrolling
    void captureModeChanged(int mode);
    /// @brief 最小化按钮点击
    void minimizeRequested();
    /// @brief 关闭按钮点击（退出应用）
    void closeRequested();
    /// @brief 保存按钮点击
    void saveRequested();

protected:
    /// @brief 鼠标按下：判断是否在空白区域以启动窗口拖拽
    void mousePressEvent(QMouseEvent* event) override;
    /// @brief 鼠标移动：拖拽窗口
    void mouseMoveEvent(QMouseEvent* event) override;
    /// @brief 鼠标释放：结束拖拽
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// @brief 初始化所有动作与按钮
    void setupActions();

    QAction*     m_actCapture   = nullptr;  ///< 截屏动作
    QAction*     m_actMinimize  = nullptr;  ///< 最小化动作
    QAction*     m_actClose     = nullptr;  ///< 关闭动作
    QToolButton* m_btnCapture   = nullptr;  ///< 截屏按钮
    QToolButton* m_btnMode      = nullptr;  ///< 模式下拉按钮
    QToolButton* m_btnSave      = nullptr;  ///< 保存按钮
    QAction*     m_saveAction   = nullptr;  ///< 保存按钮的 wrapper action

    QPoint  m_dragOffset;          ///< 拖拽起点偏移
    bool    m_dragging = false;    ///< 当前是否正在拖拽
};

} // namespace SK
