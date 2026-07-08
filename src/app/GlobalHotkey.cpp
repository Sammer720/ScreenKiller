/**
 * \file GlobalHotkey.cpp
 * \brief GlobalHotkey 实现
 */
#include "GlobalHotkey.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "utils/Logger.h"
#include <QWidget>

GlobalHotkey::GlobalHotkey(QObject* parent)
    : QObject(parent)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterAll();
}

bool GlobalHotkey::registerShortcut(int id, UINT mods, UINT vk)
{
#ifdef Q_OS_WIN
    // 通过 parent() 取得 QWidget 句柄，用于 RegisterHotKey
    HWND hwnd = nullptr;
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    if (parentWidget != nullptr)
    {
        hwnd = reinterpret_cast<HWND>(parentWidget->winId());
    }
    if (hwnd == nullptr)
    {
        SK_LOG_WARN() << "GlobalHotkey: 父窗口无效，无法注册。";
        return false;
    }

    if (RegisterHotKey(hwnd, id, mods, vk) != 0)
    {
        m_registeredIds.append(id);
        return true;
    }

    SK_LOG_WARN() << "RegisterHotKey 失败，错误码:" << GetLastError();
    return false;
#else
    Q_UNUSED(id);
    Q_UNUSED(mods);
    Q_UNUSED(vk);
    return false;
#endif
}

bool GlobalHotkey::unregisterShortcut(int id)
{
#ifdef Q_OS_WIN
    HWND hwnd = nullptr;
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    if (parentWidget != nullptr)
    {
        hwnd = reinterpret_cast<HWND>(parentWidget->winId());
    }
    if (hwnd == nullptr)
    {
        return false;
    }

    if (UnregisterHotKey(hwnd, id) != 0)
    {
        m_registeredIds.removeAll(id);
        return true;
    }
    return false;
#else
    Q_UNUSED(id);
    return false;
#endif
}

void GlobalHotkey::unregisterAll()
{
#ifdef Q_OS_WIN
    if (!m_registeredIds.isEmpty())
    {
        for (int id : m_registeredIds)
        {
            unregisterShortcut(id);
        }
        m_registeredIds.clear();
    }
#endif
}
