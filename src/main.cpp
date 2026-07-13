/**
 * \file main.cpp
 * \brief ScreenKiller 程序入口
 *
 * 启动流程：
 *   1. 创建 QApplication，启用高 DPI 支持（Qt6 默认开启）
 *   2. 加载 Windows11 浅色 QSS 样式
 *   3. 构造 MainWindow（初始仅显示截屏按钮 / 最小化 / 关闭）
 *   4. 注册全局快捷键 Ctrl+Alt+A
 *   5. 进入 Qt 事件循环
 *
 * 注：通过 Qt6::EntryPoint 实现 GUI 程序入口（WIN32 子系统 + main()）。
 */
#include <QApplication>
#include <QFile>
#include <QObject>
#include <QSettings>

#include "app/MainWindow.h"
#include "ui/Style.h"
#include "utils/Logger.h"

namespace {

/// \brief 应用名称
const QString G_APP_NAME = QStringLiteral("ScreenKiller");
/// \brief 组织名称
const QString G_ORG_NAME = QStringLiteral("Sammer");
/// \brief 应用版本号
const QString G_APP_VERSION = QStringLiteral("0.1.0");
/// \brief QSS 样式表资源路径
const QString G_QSS_PATH = QStringLiteral(":/styles/windows11_light.qss");

} // namespace

/**
 * @brief 程序入口函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char* argv[])
{
    // Qt6 默认已开启高 DPI，无需手动调用
    QApplication app(argc, argv);
    QApplication::setApplicationName(G_APP_NAME);
    QApplication::setOrganizationName(G_ORG_NAME);
    QApplication::setApplicationVersion(G_APP_VERSION);
    // 配置 QSettings 默认使用 INI 文件（而非 Windows 注册表），全局唯一配置源
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // portable.txt 双模式：exe 同目录有标记文件则配置写 exe 同目录，否则走 %APPDATA%
    const QString appDir = QCoreApplication::applicationDirPath();
    if (QFile::exists(appDir + QStringLiteral("/portable.txt")))
    {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, appDir);
        SK_LOG_INFO() << "Portable mode: config redirected to" << appDir;
    }
    // 关闭主窗口后仍驻留托盘
    QApplication::setQuitOnLastWindowClosed(false);

    // 加载样式
    SK::Style::loadAppStyleSheet(G_QSS_PATH);

    SK::MainWindow window;
    QObject::connect(&window, &SK::MainWindow::requestQuit, &app, &QApplication::quit);
    window.show();

    return app.exec();
}
