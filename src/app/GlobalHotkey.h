/**
 * \file GlobalHotkey.h
 * \brief Windows RegisterHotKey 的 Qt 封装
 *
 * 工作原理：
 *   注册的全局快捷键通过 Windows 消息 WM_HOTKEY 投递，
 *   由父窗口（MainWindow）的 nativeEvent 截获并分发。
 *
 * 注意事项：
 *   - 同一 id 重复注册会失败，必须先 UnregisterHotKey
 *   - 窗口销毁前必须 unregister，否则句柄泄漏
 */
#pragma once

#include <QObject>
#include <QList>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

/**
 * @brief Windows RegisterHotKey 的 Qt 封装类
 *
 * 用法：
 *   GlobalHotkey* hk = new GlobalHotkey(mainWindow);
 *   hk->registerShortcut(1, MOD_CONTROL | MOD_ALT, 'A');
 */
class GlobalHotkey : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象，通常是需要接收 WM_HOTKEY 的 QWidget
     */
    explicit GlobalHotkey(QObject* parent = nullptr);

    /// @brief 析构：自动 unregister 所有快捷键，避免句柄泄漏
    ~GlobalHotkey() override;

    /**
     * @brief 注册一个全局快捷键
     * @param id   自定义 ID（用于在 WM_HOTKEY 中区分）
     * @param mods 修饰键组合（MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN）
     * @param vk   虚拟键码（'A' ~ 'Z' / '0' ~ '9' / VK_F1...）
     * @return 注册是否成功
     */
    bool registerShortcut(int id, UINT mods, UINT vk);

    /**
     * @brief 取消某个快捷键
     * @param id 注册时使用的 ID
     * @return 取消是否成功
     */
    bool unregisterShortcut(int id);

    /// @brief 取消全部已注册快捷键
    void unregisterAll();

private:
    QList<int> m_registeredIds;   ///< 已注册的快捷键 ID 列表
};
