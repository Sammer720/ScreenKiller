/**
 * \file Style.h
 * \brief 应用样式管理
 *
 * 提供 Windows11 浅色 QSS 加载能力，
 * 支持从 Qt 资源路径或文件路径加载，并提供内联备用样式。
 */
#pragma once

#include <QString>

namespace SK {

class Style
{
public:
    /// @brief 从 Qt 资源路径加载 QSS 并应用到 qApp
    /// @param qrcPath 资源路径，如 ":/styles/windows11_light.qss"
    static void loadAppStyleSheet(const QString& qrcPath);

    /// @brief 从文件系统路径加载 QSS 并应用到 qApp
    /// @param filePath 文件绝对路径或相对路径
    static void loadAppStyleSheetFromFile(const QString& filePath);

    /// @brief 返回内联的 Windows11 浅色 QSS（备用，无需文件 IO）
    /// @return QSS 字符串
    static QString windows11LightInline();
};

} // namespace SK
