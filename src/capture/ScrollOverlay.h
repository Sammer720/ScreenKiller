/**
 * \file ScrollOverlay.h
 * \brief 滚动截屏提示浮窗
 *
 * 在滚动截屏过程中显示一个置顶浮窗，提示用户如何操作，
 * 并提供完成 / 取消按钮。浮窗不会抢夺目标窗口焦点。
 */
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

/**
 * @brief 滚动截屏操作提示浮窗
 *
 * 窗口属性：
 *   - Frameless + Tool + WindowStaysOnTopHint
 *   - WA_ShowWithoutActivating：显示时不激活，不抢目标窗口焦点
 *   - 使用项目全局 QSS 样式（Lavender & Cream 主题）
 */
class ScrollOverlay : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ScrollOverlay(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ScrollOverlay() override;

    /**
     * @brief 设置已捕获帧数
     * @param count 已捕获帧数
     */
    void setFrameCount(int count);

Q_SIGNALS:
    /**
     * @brief 用户点击完成按钮
     */
    void finishRequested();

    /**
     * @brief 用户点击取消按钮
     */
    void cancelRequested();

protected:
    /**
     * @brief 按键事件：Esc 取消 / Enter 完成
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent* event) override;

private:
    /**
     * @brief 定位到主屏顶部居中
     */
    void positionAtTopCenter();

private:
    QLabel*       m_titleLabel   = nullptr;  ///< 提示标题
    QLabel*       m_countLabel   = nullptr;  ///< 已捕获帧数标签
    QPushButton*  m_finishButton  = nullptr;  ///< 完成按钮
    QPushButton*  m_cancelButton  = nullptr;  ///< 取消按钮
};