/**
 * \file MouseWheelHook.cpp
 * \brief MouseWheelHook 实现
 */
#include "MouseWheelHook.h"

#include "utils/Logger.h"

MouseWheelHook* MouseWheelHook::s_instance = nullptr;

MouseWheelHook::MouseWheelHook(QObject* parent)
    : QObject(parent)
{
}

MouseWheelHook::~MouseWheelHook()
{
    uninstall();
}

bool MouseWheelHook::install()
{
#ifdef Q_OS_WIN
    if (m_hook != nullptr)
    {
        return true;
    }

    s_instance = this;
    m_hook = SetWindowsHookExW(WH_MOUSE_LL, &MouseWheelHook::hookProc,
                               nullptr, 0);

    if (m_hook == nullptr)
    {
        SK_LOG_WARN() << "MouseWheelHook: SetWindowsHookExW 失败，错误码:"
                      << GetLastError();
        s_instance = nullptr;
        return false;
    }

    return true;
#else
    return false;
#endif
}

void MouseWheelHook::uninstall()
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

bool MouseWheelHook::isInstalled() const
{
#ifdef Q_OS_WIN
    return m_hook != nullptr;
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
LRESULT CALLBACK MouseWheelHook::hookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if ((nCode == HC_ACTION) && (wParam == WM_MOUSEWHEEL) && (s_instance != nullptr))
    {
        auto* mhs = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (mhs != nullptr)
        {
            int delta = static_cast<int>(static_cast<SHORT>(HIWORD(mhs->mouseData)));
            QPoint pos(mhs->pt.x, mhs->pt.y);
            Q_EMIT s_instance->wheelScrolled(delta, pos);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif
