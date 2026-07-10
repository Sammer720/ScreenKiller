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

namespace {
/// \brief 需要过滤的系统背景/任务栏窗口类名列表
const wchar_t* const G_SYSTEM_BACKGROUND_CLASSES[] = {
    L"Progman",
    L"WorkerW",
    L"Shell_TrayWnd",
    L"Shell_SecondaryTrayWnd"
};
/// \brief 系统背景类名数量
constexpr int G_SYSTEM_BACKGROUND_CLASS_COUNT =
    sizeof(G_SYSTEM_BACKGROUND_CLASSES) / sizeof(G_SYSTEM_BACKGROUND_CLASSES[0]);
/// \brief 类名缓冲区最大字符数（256 足以容纳所有标准窗口类名）
constexpr int G_CLASS_NAME_BUFFER_LEN = 256;

/**
 * @brief 判断窗口是否为系统背景/任务栏窗口（需过滤掉）
 * @param hwnd 目标窗口句柄
 * @return 是系统背景窗口返回 true，否则返回 false
 */
bool isSystemBackgroundWindow(HWND hwnd)
{
    // Fail-Fast：空句柄直接返回
    if (hwnd == nullptr)
    {
        return false;
    }

    wchar_t className[G_CLASS_NAME_BUFFER_LEN] = {0};
    int nameLen = GetClassNameW(hwnd, className, G_CLASS_NAME_BUFFER_LEN);
    if (nameLen == 0)
    {
        return false;
    }

    for (int i = 0; i < G_SYSTEM_BACKGROUND_CLASS_COUNT; ++i)
    {
        if (wcscmp(className, G_SYSTEM_BACKGROUND_CLASSES[i]) == 0)
        {
            return true;
        }
    }
    return false;
}
} // namespace anonymous

HWND findTopLevelWindowAtPoint(int x, int y, HWND skipHwnd)
{
    // 按 Z-Order 从最顶层窗口开始遍历
    HWND hwnd = GetTopWindow(nullptr);
    while (hwnd != nullptr)
    {
        bool isSkipped = false;

        // 跳过遮罩自身
        if (hwnd == skipHwnd)
        {
            isSkipped = true;
        }
        // 跳过不可见窗口
        else if (IsWindowVisible(hwnd) == 0)
        {
            isSkipped = true;
        }
        // 跳过最小化窗口
        else if (IsIconic(hwnd) != 0)
        {
            isSkipped = true;
        }
        // 跳过桌面窗口
        else if (hwnd == GetDesktopWindow())
        {
            isSkipped = true;
        }
        // 跳过系统背景/任务栏窗口（Progman/WorkerW/Shell_TrayWnd 等）
        else if (isSystemBackgroundWindow(hwnd))
        {
            isSkipped = true;
        }

        if (isSkipped)
        {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }

        // 检查点是否在窗口矩形内
        QRect rect = getWindowFrameRect(hwnd);
        if (rect.contains(x, y))
        {
            return hwnd;
        }
        hwnd = GetWindow(hwnd, GW_HWNDNEXT);
    }
    return nullptr;
}

namespace {
/// \brief 主任务栏窗口类名（Shell_TrayWnd）
const wchar_t* const G_PRIMARY_TASKBAR_CLASS   = L"Shell_TrayWnd";
/// \brief 副屏任务栏窗口类名（Shell_SecondaryTrayWnd）
const wchar_t* const G_SECONDARY_TASKBAR_CLASS = L"Shell_SecondaryTrayWnd";
} // namespace anonymous

QVector<QRect> getTaskbarRects()
{
    QVector<QRect> taskbarRects;

    // 主任务栏：Shell_TrayWnd 全局唯一
    HWND primaryHwnd = FindWindowW(G_PRIMARY_TASKBAR_CLASS, nullptr);
    if (primaryHwnd != nullptr)
    {
        RECT primaryRc{};
        if (GetWindowRect(primaryHwnd, &primaryRc) != 0)
        {
            taskbarRects.append(QRect(primaryRc.left, primaryRc.top,
                                      primaryRc.right - primaryRc.left,
                                      primaryRc.bottom - primaryRc.top));
        }
    }

    // 副屏任务栏：枚举所有 Shell_SecondaryTrayWnd
    HWND secondaryHwnd = FindWindowExW(nullptr, nullptr,
                                       G_SECONDARY_TASKBAR_CLASS, nullptr);
    while (secondaryHwnd != nullptr)
    {
        RECT secondaryRc{};
        if (GetWindowRect(secondaryHwnd, &secondaryRc) != 0)
        {
            // 仅追加非空矩形，过滤异常副屏状态
            if (IsRectEmpty(&secondaryRc) == 0)
            {
                taskbarRects.append(QRect(secondaryRc.left, secondaryRc.top,
                                          secondaryRc.right - secondaryRc.left,
                                          secondaryRc.bottom - secondaryRc.top));
            }
        }
        // 继续查找下一个副屏任务栏
        secondaryHwnd = FindWindowExW(nullptr, secondaryHwnd,
                                       G_SECONDARY_TASKBAR_CLASS, nullptr);
    }

    return taskbarRects;
}

#endif // Q_OS_WIN

} // namespace WinApi
} // namespace SK
