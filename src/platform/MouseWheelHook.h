/**
 * \file MouseWheelHook.h
 * \brief Windows 低级鼠标滚轮事件全局钩子封装
 *
 * 通过 WH_MOUSE_LL 钩子监听系统级鼠标滚轮事件，
 * 仅在 Qt 主线程安装，因为回调依赖 Qt 的信号/槽机制。
 *
 * 用法：
 *   MouseWheelHook hook;
 *   connect(&hook, &MouseWheelHook::wheelScrolled, ...);
 *   hook.install();
 *   // ...
 *   hook.uninstall();
 */
#pragma once

#include <QObject>
#include <QPoint>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

/**
 * @brief Windows 低级鼠标滚轮钩子封装
 *
 * 安装 WH_MOUSE_LL 钩子后，用户在任意窗口滚动鼠标滚轮都会触发
 * wheelScrolled 信号。事件会继续传递给后续钩子，不会拦截。
 */
class MouseWheelHook : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit MouseWheelHook(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     *
     * 析构时自动卸载钩子，避免句柄泄漏。
     */
    ~MouseWheelHook() override;

    /**
     * @brief 安装低级鼠标钩子
     * @return 安装成功返回 true，失败返回 false
     */
    bool install();

    /**
     * @brief 卸载低级鼠标钩子
     */
    void uninstall();

    /**
     * @brief 判断钩子是否已安装
     * @return 已安装返回 true，否则返回 false
     */
    bool isInstalled() const;

Q_SIGNALS:
    /**
     * @brief 滚轮滚动信号
     * @param delta 滚动量，向下滚动为负，向上滚动为正（WHEEL_DELTA = 120）
     * @param pos 滚轮事件发生时的屏幕坐标
     */
    void wheelScrolled(int delta, const QPoint& pos);

private:
#ifdef Q_OS_WIN
    /**
     * @brief 钩子回调函数
     * @param nCode 钩子代码
     * @param wParam 消息参数
     * @param lParam 扩展参数，指向 MSLLHOOKSTRUCT
     * @return 是否继续传递事件
     */
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif

private:
#ifdef Q_OS_WIN
    HHOOK m_hook = nullptr;  ///< 低级鼠标钩子句柄
#endif

    static MouseWheelHook* s_instance;  ///< 当前活跃实例，供静态回调使用
};
