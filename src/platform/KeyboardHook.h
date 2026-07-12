/**
 * \file KeyboardHook.h
 * \brief Windows 低级键盘事件全局钩子封装
 *
 * 通过 WH_KEYBOARD_LL 钩子监听系统级键盘事件，
 * 在滚动截屏期间拦截 Esc（取消）和 Enter/Return（完成）。
 * 仅在 Qt 主线程安装。
 *
 * 用法：
 *   KeyboardHook hook;
 *   connect(&hook, &KeyboardHook::cancelTriggered, ...);
 *   connect(&hook, &KeyboardHook::finishTriggered, ...);
 *   hook.install();
 *   // ...
 *   hook.uninstall();
 */
#pragma once

#include <QObject>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

/**
 * @brief Windows 低级键盘钩子封装
 *
 * 安装 WH_KEYBOARD_LL 钩子后，系统级键盘事件会触发本钩子回调。
 * Esc 键触发 cancelTriggered 信号，Enter/Return 键触发 finishTriggered 信号。
 * 其他按键会继续传递给后续钩子，不受影响。
 */
class KeyboardHook : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit KeyboardHook(QObject* parent = nullptr);

    /**
     * @brief 析构函数，自动卸载钩子
     */
    ~KeyboardHook() override;

    /**
     * @brief 安装低级键盘钩子
     * @return 安装成功返回 true
     */
    bool install();

    /**
     * @brief 卸载低级键盘钩子
     */
    void uninstall();

    /**
     * @brief 判断钩子是否已安装
     */
    bool isInstalled() const;

Q_SIGNALS:
    /**
     * @brief 用户按下 Esc 取消
     */
    void cancelTriggered();

    /**
     * @brief 用户按下 Enter/Return 完成
     */
    void finishTriggered();

private:
#ifdef Q_OS_WIN
    /**
     * @brief 低级键盘钩子回调
     */
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif

private:
#ifdef Q_OS_WIN
    HHOOK m_hook = nullptr;                       ///< 低级键盘钩子句柄
#endif
    static KeyboardHook* s_instance;              ///< 当前活跃实例，供静态回调使用
};
