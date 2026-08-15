/**
 * \file UpdateInfo.h
 * \brief 更新发布信息数据结构
 *
 * 描述 GitHub Releases 最新正式版的元数据。由 UpdateChecker 解析
 * GitHub API 响应后填充，再通过信号传递给 MainWindow，由 UpdateDialog
 * 展示给用户，并根据用户选择决定「前往下载 / 稍后 / 跳过此版本」。
 *
 * 本结构体为纯数据载体，不继承 QObject，也不参与 AUTOMOC，因此
 * 无需写入 APP_HEADERS 的 MOC 扫描（但按项目惯例仍登记其中便于 IDE 展示）。
 */
#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace SK::update {

/**
 * @brief 一次更新发布的关键信息
 *
 * 所有字段均由 UpdateChecker 从 GitHub Releases API 的 JSON 响应中提取。
 * 字段语义与 GitHub API 字段一一对应，详见各成员注释。
 */
struct ReleaseInfo
{
    QString versionString;       ///< 去除 v 前缀后的语义版本号，如 "1.2.3"（用于比对与跳过判断）
    QString tagName;             ///< 原始 tag 名，如 "v1.2.3"
    QString title;               ///< 发布标题（对应 GitHub 的 name 字段）
    QString changelogMarkdown;   ///< 更新说明正文（对应 GitHub 的 body 字段，Markdown 格式）
    QString releasePageUrl;      ///< 发布页地址（对应 GitHub 的 html_url 字段）
    QString downloadUrl;         ///< 与当前发行版匹配的资产下载地址；为空时回退到发布页
    QDateTime publishedAt;       ///< 发布时间（对应 GitHub 的 published_at 字段，ISO 8601）
};

} // namespace SK::update

/// \brief 声明 ReleaseInfo 的 Qt 元类型，为潜在的跨线程/排队连接保留能力
Q_DECLARE_METATYPE(SK::update::ReleaseInfo)
