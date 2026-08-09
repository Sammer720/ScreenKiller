/**
 * \file MessageBox.cpp
 * \brief 中文文案的标准消息提示框实现
 */
#include "MessageBox.h"

#include <QMessageBox>
#include <QPushButton>

namespace SK::utils {

void showWarning(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
    auto* okBtn = box.button(QMessageBox::Ok);
    if (okBtn != nullptr)
    {
        okBtn->setText(QStringLiteral("确认"));
    }
    box.exec();
}

void showCritical(QWidget* parent, const QString& title, const QString& text)
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