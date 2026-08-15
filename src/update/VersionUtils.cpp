/**
 * \file VersionUtils.cpp
 * \brief 语义版本号解析与比较工具实现
 */
#include "VersionUtils.h"

#include <QRegularExpression>
#include <QVersionNumber>

namespace SK::update {

namespace {

/// \brief 匹配 tag 前导数字段（X 或 X.Y 或 X.Y.Z），忽略前缀与 -suffix
///
/// 锚定在字符串开头，捕获前导的「数字(.数字)*」段；对 "v1.2.3-beta" 可捕获 "1.2.3"。
const QRegularExpression G_VERSION_SEGMENT_PATTERN(QStringLiteral("^(\\d+(?:\\.\\d+)*)"));

/// \brief 前导版本前缀字符 v（小写）
const QChar G_TAG_PREFIX_LOWER = QLatin1Char('v');
/// \brief 前导版本前缀字符 V（大写）
const QChar G_TAG_PREFIX_UPPER = QLatin1Char('V');

} // namespace

QString normalizeTag(const QString& tag)
{
    // 1. 去除首尾空白，避免 " v1.2.3 " 这类 tag 干扰后续处理
    QString normalizedTag = tag.trimmed();

    // 2. 去掉前导的 v / V 前缀（仅当存在且长度足够时）
    if (normalizedTag.size() >= 1)
    {
        const QChar firstChar = normalizedTag.at(0);
        if ((firstChar == G_TAG_PREFIX_LOWER) || (firstChar == G_TAG_PREFIX_UPPER))
        {
            normalizedTag.remove(0, 1);
        }
    }

    // 3. 提取前导数字段，丢弃 -beta、-rc1 等后缀
    const QRegularExpressionMatch match = G_VERSION_SEGMENT_PATTERN.match(normalizedTag);
    if (match.hasMatch())
    {
        return match.captured(1);
    }

    // 无法提取任何数字段：返回空字符串，由调用方据此判定为「无法比对」
    return QString();
}

bool isNewerVersion(const QString& currentVersion, const QString& latestTag)
{
    // 1. 归一化最新 tag，得到纯数字版本号
    const QString normalizedLatest = normalizeTag(latestTag);

    // 2. 解析两侧版本号
    const QVersionNumber currentVersionNumber = QVersionNumber::fromString(currentVersion);
    const QVersionNumber latestVersionNumber = QVersionNumber::fromString(normalizedLatest);

    // 3. 防御性校验：最新 tag 无法解析为有效版本号时，视为「无更新」
    if (latestVersionNumber.isNull())
    {
        return false;
    }

    // 4. 逐段数值比较：仅当最新版本严格高于当前版本才返回 true
    return currentVersionNumber < latestVersionNumber;
}

} // namespace SK::update
