/**
 * \file WinApi.cpp
 * \brief Windows API 封装实现
 *
 * 注意：
 *   所有原生句柄（HWND、HANDLE）入参均采用显式判空（== nullptr / != nullptr），
 *   所有返回 BOOL 的 Win32 API 调用结果均显式与 0 比较，
 *   严格遵循防御性编程规范，杜绝隐式真值判断。
 */
#include "WinApi.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <psapi.h>
#  include <dwmapi.h>
#  pragma comment(lib, "psapi.lib")
#  pragma comment(lib, "dwmapi.lib")
#endif

#include "utils/Logger.h"

namespace SK {
namespace WinApi {

#ifdef Q_OS_WIN

namespace {
/// \brief 窗口标题缓冲区最大字符数（512 字符，足以容纳绝大多数标题）
constexpr int G_TITLE_BUFFER_LEN = 512;
/// \brief 进程可执行文件路径缓冲区最大字符数
constexpr int G_PATH_BUFFER_LEN   = MAX_PATH;
/// \brief 键盘状态数组固定长度
constexpr int G_KEYBOARD_STATE_LEN = 256;
/// \brief Alt 键按下标志位（高位 0x80）
constexpr BYTE G_KEY_PRESSED_MASK = 0x80;
}

QRect getWindowFrameRect(HWND hwnd)
{
    // Fail-Fast：空句柄直接返回
    if (hwnd == nullptr)
    {
        return {};
    }

    RECT rc{};
    // 优先使用 DWM 扩展帧边界（去除隐形阴影区）
    if (DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                              &rc, sizeof(rc)) == S_OK)
    {
        return QRect(rc.left, rc.top,
                     rc.right - rc.left, rc.bottom - rc.top);
    }

    // 回退到 GetWindowRect
    if (::GetWindowRect(hwnd, &rc) != 0)
    {
        return QRect(rc.left, rc.top,
                     rc.right - rc.left, rc.bottom - rc.top);
    }
    return {};
}

QString getWindowTitle(HWND hwnd)
{
    // Fail-Fast：空句柄直接返回
    if (hwnd == nullptr)
    {
        return {};
    }

    wchar_t buf[G_TITLE_BUFFER_LEN] = {0};
    int len = GetWindowTextW(hwnd, buf, G_TITLE_BUFFER_LEN);
    return QString::fromWCharArray(buf, len);
}

QString getWindowProcessPath(HWND hwnd)
{
    // Fail-Fast：空句柄直接返回
    if (hwnd == nullptr)
    {
        return {};
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
    {
        return {};
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc == nullptr)
    {
        return {};
    }

    wchar_t path[G_PATH_BUFFER_LEN] = {0};
    DWORD sz = G_PATH_BUFFER_LEN;
    BOOL ok = QueryFullProcessImageNameW(hProc, 0, path, &sz);
    CloseHandle(hProc);

    return (ok != 0) ? QString::fromWCharArray(path) : QString{};
}

bool setForegroundWindow(HWND hwnd)
{
    // Fail-Fast：空句柄直接返回
    if (hwnd == nullptr)
    {
        return false;
    }

    // 标准做法：先发 Alt 键释放焦点锁，再 SetForegroundWindow
    BYTE state[G_KEYBOARD_STATE_LEN] = {0};
    GetKeyboardState(state);
    if ((state[VK_MENU] & G_KEY_PRESSED_MASK) == 0)
    {
        keybd_event(VK_MENU, 0, 0, 0);
        keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    }
    return SetForegroundWindow(hwnd) != 0;
}

bool sendMouseWheel(int delta)
{
    INPUT input{};
    input.type         = INPUT_MOUSE;
    input.mi.dwFlags   = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool moveCursorTo(int x, int y)
{
    return SetCursorPos(x, y) != 0;
}

namespace {
/**
 * @brief EnumWindows 回调：将可见顶层窗口句柄加入列表
 * @param hwnd 当前枚举到的窗口句柄
 * @param lParam 用户参数，指向 QVector<HWND>
 * @return TRUE 继续枚举
 */
BOOL CALLBACK enumWndProc(HWND hwnd, LPARAM lParam)
{
    auto* list = reinterpret_cast<QVector<HWND>*>(lParam);
    if (IsWindowVisible(hwnd) == 0)
    {
        return TRUE;   // 不可见，跳过
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr)
    {
        return TRUE;   // 跳过子窗口（如对话框、工具窗）
    }
    list->append(hwnd);
    return TRUE;
}
} // namespace anonymous

QVector<HWND> enumerateTopLevelWindows()
{
    QVector<HWND> list;
    EnumWindows(enumWndProc, reinterpret_cast<LPARAM>(&list));
    return list;
}

bool isVisibleWindow(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return false;
    }
    return (IsWindowVisible(hwnd) != 0) && (IsIconic(hwnd) == 0);
}

#endif // Q_OS_WIN

} // namespace WinApi
} // namespace SK
