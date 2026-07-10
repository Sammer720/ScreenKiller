/**
 * \file WinApi.h
 * \brief Windows API 封装工具集
 *
 * 设计目的：
 *   集中放置所有 Windows API 的薄封装，便于上层模块统一调用，
 *   也方便后续替换实现或单元测试时 mock。
 *
 * 涵盖能力：
 *   - 窗口枚举 / 查找
 *   - 真实窗口矩形（含 DWM 扩展帧）
 *   - 鼠标滚轮模拟
 *   - 前台窗口设置
 *   - 屏幕坐标转换
 */
#pragma once

#include <QRect>
#include <QString>
#include <QVector>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace SK {
namespace WinApi {

#ifdef Q_OS_WIN

/**
 * @brief 获取窗口的真实可见矩形（DWMWA_EXTENDED_FRAME_BOUNDS，去除隐形阴影区）
 * @param hwnd 目标窗口句柄
 * @return 真实窗口矩形；若获取失败返回空 QRect
 */
QRect getWindowFrameRect(HWND hwnd);

/**
 * @brief 获取窗口标题
 * @param hwnd 目标窗口句柄
 * @return 窗口标题文本；失败返回空 QString
 */
QString getWindowTitle(HWND hwnd);

/**
 * @brief 获取窗口所属进程的可执行文件路径
 * @param hwnd 目标窗口句柄
 * @return 进程可执行文件绝对路径；失败返回空 QString
 */
QString getWindowProcessPath(HWND hwnd);

/**
 * @brief 将目标窗口设为前台
 * @param hwnd 目标窗口句柄
 * @return 是否成功设置为前台窗口
 */
bool setForegroundWindow(HWND hwnd);

/**
 * @brief 模拟鼠标滚轮滚动
 * @param delta 滚动量，正值向上，负值向下（WHEEL_DELTA = 120）
 * @return 是否成功发送滚轮事件
 */
bool sendMouseWheel(int delta);

/**
 * @brief 将光标移动到指定屏幕坐标
 * @param x 屏幕 X 坐标
 * @param y 屏幕 Y 坐标
 * @return 是否成功移动光标
 */
bool moveCursorTo(int x, int y);

/**
 * @brief 枚举所有可见顶层窗口
 * @return 顶层窗口句柄列表
 */
QVector<HWND> enumerateTopLevelWindows();

/**
 * @brief 判断窗口是否可见且非最小化
 * @param hwnd 目标窗口句柄
 * @return 可见且非最小化返回 true，否则返回 false
 */
bool isVisibleWindow(HWND hwnd);

/**
 * @brief 查找鼠标点下最顶层的可见窗口（排除遮罩自身与系统背景窗口）
 * @param x 屏幕 X 坐标
 * @param y 屏幕 Y 坐标
 * @param skipHwnd 需要跳过的窗口句柄（通常为遮罩窗口自身），可为 nullptr
 * @return 命中窗口的 HWND；未命中返回 nullptr
 */
HWND findTopLevelWindowAtPoint(int x, int y, HWND skipHwnd);

/**
 * @brief 获取主任务栏和所有副屏任务栏的屏幕矩形
 * @return 任务栏矩形列表
 */
QVector<QRect> getTaskbarRects();

#endif // Q_OS_WIN

} // namespace WinApi
} // namespace SK
