/**
 * \file MessageBox.h
 * \brief 中文文案的标准消息提示框
 *
 * Qt 内置 QMessageBox 默认按钮文案为英文 "OK"/"Cancel" 等，
 * 本工具统一将其本地化为中文，避免每个调用点重复设置。
 *
 * 注意：本头文件仅包含声明与接口注释，实现位于 MessageBox.cpp。
 */
#pragma once

#include <QString>

class QWidget;

namespace SK::utils {

/// \brief 显示警告框（按钮文案为"确认"）
/// \param parent 父窗口
/// \param title 标题
/// \param text 正文内容
void showWarning(QWidget* parent, const QString& title, const QString& text);

/// \brief 显示严重错误框（按钮文案为"确认"）
/// \param parent 父窗口
/// \param title 标题
/// \param text 正文内容
void showCritical(QWidget* parent, const QString& title, const QString& text);

} // namespace SK::utils