/**
 * \file KeyboardHook.cpp
 * \brief KeyboardHook 实现
 */
#include "KeyboardHook.h"

#include "utils/Logger.h"

KeyboardHook* KeyboardHook::s_instance = nullptr;

KeyboardHook::KeyboardHook(QObject* parent)
    : QObject(parent)
{
}

KeyboardHook::~KeyboardHook()
{
    uninstall();
}

bool KeyboardHook::install()
{
#ifdef Q_OS_WIN
    if (m_hook != nullptr)
    {
        return true;
    }

    s_instance = this;
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardHook::hookProc,
                               nullptr, 0);

    if (m_hook == nullptr)
    {
        SK_LOG_WARN() << "KeyboardHook: SetWindowsHookExW 失败，错误码:"
                      << GetLastError();
        s_instance = nullptr;
        return false;
    }

    return true;
#else
    return false;
#endif
}

void KeyboardHook::uninstall()
{
#ifdef Q_OS_WIN
    if (m_hook != nullptr)
    {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    s_instance = nullptr;
#endif
}

bool KeyboardHook::isInstalled() const
{
#ifdef Q_OS_WIN
    return m_hook != nullptr;
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
LRESULT CALLBACK KeyboardHook::hookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // nCode < 0 时必须调用 CallNextHookEx
    if (nCode < 0)
    {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if ((nCode == HC_ACTION) && (wParam == WM_KEYDOWN) && (s_instance != nullptr))
    {
        auto* khs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (khs != nullptr)
        {
            switch (khs->vkCode)
            {
            case VK_ESCAPE:
                Q_EMIT s_instance->cancelTriggered();
                // 消耗该按键，不继续传递
                return 1;

            case VK_RETURN:
                Q_EMIT s_instance->finishTriggered();
                // 消耗该按键，不继续传递
                return 1;

            default:
                break;
            }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif
