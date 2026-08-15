/**
 * \file VersionUtils.h
 * \brief 语义版本号解析与比较工具
 *
 * 提供 GitHub tag（如 "v1.2.3"、"v1.2.3-beta"）到规范语义版本号的归一化，
 * 以及当前版本与最新 tag 之间的「是否有更新」判断。
 *
 * 纯函数、无状态、无 Qt 事件依赖，便于后续以 QtTest 单独验证。
 */
#pragma once

#include <QString>

namespace SK::update {

/**
 * @brief 将 GitHub tag 归一化为纯数字语义版本号
 *
 * 处理流程：去除首尾空白 → 去掉前导的 v/V 前缀 → 提取前导的数字段
 * （形如 X 或 X.Y 或 X.Y.Z），丢弃 -beta、-rc1 等后缀。
 *
 * @param tag 原始 tag 字符串，如 "v1.2.3" 或 "  v1.2.3-beta  "
 * @return 归一化后的纯数字版本号，如 "1.2.3"；若无法提取任何数字段则返回空字符串
 */
QString normalizeTag(const QString& tag);

/**
 * @brief 判断最新 tag 是否比当前运行版本更新
 *
 * 两侧均用 QVersionNumber 逐段数值比较（正确处理 1.2.9 < 1.2.10），
 * QVersionNumber 会忽略尾随零段（1.2 与 1.2.0 视为相等），符合语义版本习惯。
 *
 * @param currentVersion 当前运行版本号（来自 QCoreApplication::applicationVersion()）
 * @param latestTag      最新发布的原始 tag（如 "v1.2.3"）
 * @return 最新 tag 比当前版本高则返回 true，否则返回 false
 */
bool isNewerVersion(const QString& currentVersion, const QString& latestTag);

} // namespace SK::update
