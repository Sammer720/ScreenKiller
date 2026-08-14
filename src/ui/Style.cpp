/**
 * \file Style.cpp
 * \brief Style 实现：QSS 样式加载
 */
#include "Style.h"

#include <QApplication>
#include <QFile>
#include <QDebug>

namespace SK {

void Style::loadAppStyleSheet(const QString& qrcPath)
{
    QFile styleFile(qrcPath);
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString qss = QString::fromUtf8(styleFile.readAll());
        qApp->setStyleSheet(qss);
    }
    else
    {
        qWarning() << "无法加载样式:" << qrcPath << "，使用内联样式。";
        qApp->setStyleSheet(windows11LightInline());
    }
}

void Style::loadAppStyleSheetFromFile(const QString& filePath)
{
    QFile styleFile(filePath);
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
}

QString Style::windows11LightInline()
{
    return QStringLiteral(R"(
        /* Lavender & Cream - Inline Fallback */
        QWidget {
            background: #FBF7EF;
            color: #3A3357;
            font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
            font-size: 13px;
        }
        /* 引导面板及其子控件透明背景：让 GuidePanel 的半透明自绘背景透出 */
        QWidget#guidePanel,
        QWidget#guidePanel QLabel,
        QWidget#guideContent {
            background: transparent;
        }
        QMainWindow { background: #FBF7EF; }
        QToolBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #FDFAF3, stop:1 #F5EDDF);
            border: none;
            border-bottom: 1px solid #D9CFC1;
            padding: 4px;
            spacing: 6px;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 6px 12px;
            color: #3A3357;
        }
        QToolButton:hover {
            background: #E4D9F0;
            border: 1px solid #B5A5D1;
        }
        QToolButton:pressed, QToolButton:checked {
            background: #B5A5D1;
            border: 1px solid #8B7AB8;
            color: #FFFFFF;
        }
        QToolButton#captureButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #9B8AC8, stop:1 #6B5B95);
            color: #FFFFFF;
            border: 1px solid #5A4B85;
            font-weight: 600;
        }
    )");
}

} // namespace SK
