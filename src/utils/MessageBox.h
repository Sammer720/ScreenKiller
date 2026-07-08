/**
 * \file MessageBox.h
 * \brief 中文文案的标准消息提示框
 *
 * Qt 内置 QMessageBox 默认按钮文案为英文 "OK"/"Cancel" 等，
 * 本工具统一将其本地化为中文，避免每个调用点重复设置。
 */
#pragma once

#include <QMessageBox>
#include <QPushButton>
#include <QString>

namespace SK::utils {

/// \brief 显示警告框（按钮文案为"确认"）
inline void showWarning(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
    auto* okBtn = box.button(QMessageBox::Ok);
    if (okBtn != nullptr)
    {
        okBtn->setText(QStringLiteral("确认"));
    }
    box.exec();
}

/// \brief 显示严重错误框（按钮文案为"确认"）
inline void showCritical(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(QMessageBox::Critical, title, text, QMessageBox::Ok, parent);
    auto* okBtn = box.button(QMessageBox::Ok);
    if (okBtn != nullptr)
    {
        okBtn->setText(QStringLiteral("确认"));
    }
    box.exec();
}

} // namespace SK::utils
