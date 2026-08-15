/**
 * \file UpdateChecker.cpp
 * \brief GitHub Releases 更新检测器实现
 */
#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "utils/Logger.h"
#include "update/AppInfo.h"
#include "VersionUtils.h"

namespace SK::update {

namespace {

/// \brief GitHub API 基址
const QString G_API_BASE_URL = QStringLiteral("https://api.github.com/repos/");
/// \brief releases/latest 端点后缀
const QString G_LATEST_ENDPOINT = QStringLiteral("/releases/latest");
/// \brief 网络请求超时毫秒数
constexpr int G_REQUEST_TIMEOUT_MS = 10000;
/// \brief 安装版资产后缀
const QString G_ASSET_INSTALLER_SUFFIX = QStringLiteral("-win64.exe");
/// \brief 便携版资产后缀
const QString G_ASSET_PORTABLE_SUFFIX = QStringLiteral("-win64-portable.zip");
/// \brief 便携包通用后缀（资产兜底匹配用）
const QString G_ASSET_ZIP_SUFFIX = QStringLiteral(".zip");
/// \brief 安装包通用后缀（资产兜底匹配用）
const QString G_ASSET_EXE_SUFFIX = QStringLiteral(".exe");
/// \brief 便携模式标记文件名
const QString G_PORTABLE_MARKER_NAME = QStringLiteral("/portable.txt");

/// \brief GitHub API 请求头：Accept
const QByteArray G_HEADER_ACCEPT = QByteArrayLiteral("Accept");
/// \brief GitHub API 请求头：Accept 取值
const QByteArray G_HEADER_ACCEPT_VALUE = QByteArrayLiteral("application/vnd.github+json");
/// \brief GitHub API 请求头：API 版本
const QByteArray G_HEADER_API_VERSION = QByteArrayLiteral("X-GitHub-Api-Version");
/// \brief GitHub API 请求头：API 版本取值
const QByteArray G_HEADER_API_VERSION_VALUE = QByteArrayLiteral("2022-11-28");

/// \brief JSON 字段名：tag_name
const QString G_JSON_TAG_NAME = QStringLiteral("tag_name");
/// \brief JSON 字段名：name（发布标题）
const QString G_JSON_TITLE = QStringLiteral("name");
/// \brief JSON 字段名：body（变更日志）
const QString G_JSON_BODY = QStringLiteral("body");
/// \brief JSON 字段名：html_url（发布页地址）
const QString G_JSON_HTML_URL = QStringLiteral("html_url");
/// \brief JSON 字段名：published_at（发布时间）
const QString G_JSON_PUBLISHED_AT = QStringLiteral("published_at");
/// \brief JSON 字段名：draft（草稿标记）
const QString G_JSON_DRAFT = QStringLiteral("draft");
/// \brief JSON 字段名：prerelease（预发布标记）
const QString G_JSON_PRERELEASE = QStringLiteral("prerelease");
/// \brief JSON 字段名：assets（资产数组）
const QString G_JSON_ASSETS = QStringLiteral("assets");
/// \brief 资产对象字段名：name
const QString G_JSON_ASSET_NAME = QStringLiteral("name");
/// \brief 资产对象字段名：browser_download_url
const QString G_JSON_ASSET_URL = QStringLiteral("browser_download_url");

/// \brief 构建 GitHub API 要求的 User-Agent（GitHub 强制要求非空 UA）
/// @return 形如 "ScreenKiller/1.2.3" 的标识串
QString buildUserAgent()
{
    const QString appVersion = QCoreApplication::applicationVersion();
    return QStringLiteral("ScreenKiller/%1").arg(appVersion);
}

/// \brief 从资产数组中按发行版类型挑选下载地址
///
/// 优先精确匹配与发行版对应的后缀；未命中时回退到任意 .zip/.exe；
/// 仍无结果则返回空字符串（由上层回退到发布页）。
///
/// @param assets     GitHub API 返回的 assets 数组
/// @param isPortable 是否为便携版发行
/// @return 匹配资产的 browser_download_url；未匹配返回空字符串
QString pickDownloadUrl(const QJsonArray& assets, bool isPortable)
{
    QString fallbackExeUrl;
    QString fallbackZipUrl;

    for (const QJsonValue& assetValue : assets)
    {
        const QJsonObject asset = assetValue.toObject();
        const QString assetName = asset.value(G_JSON_ASSET_NAME).toString();
        const QString assetUrl = asset.value(G_JSON_ASSET_URL).toString();

        // 跳过缺失名称或地址的无效资产
        if ((assetName.isEmpty()) || (assetUrl.isEmpty()))
        {
            continue;
        }

        // 便携版：精确匹配 -win64-portable.zip
        if ((isPortable) && (assetName.endsWith(G_ASSET_PORTABLE_SUFFIX)))
        {
            return assetUrl;
        }

        // 安装版：精确匹配 -win64.exe
        if ((!isPortable) && (assetName.endsWith(G_ASSET_INSTALLER_SUFFIX)))
        {
            return assetUrl;
        }

        // 兜底记录：任意 .zip 与 .exe（仅在精确匹配未命中时使用）
        if ((fallbackZipUrl.isEmpty()) && (assetName.endsWith(G_ASSET_ZIP_SUFFIX)))
        {
            fallbackZipUrl = assetUrl;
        }
        if ((fallbackExeUrl.isEmpty()) && (assetName.endsWith(G_ASSET_EXE_SUFFIX)))
        {
            fallbackExeUrl = assetUrl;
        }
    }

    if (isPortable)
    {
        return fallbackZipUrl;
    }
    return fallbackExeUrl;
}

} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

bool UpdateChecker::isPortableDistribution()
{
    const QString portableMarkerPath =
        QCoreApplication::applicationDirPath() + G_PORTABLE_MARKER_NAME;
    return QFile::exists(portableMarkerPath);
}

void UpdateChecker::checkForUpdates()
{
    // 1. 拼出 releases/latest 端点地址
    const QString repoFullName = QStringLiteral(SK_GITHUB_REPO);
    const QUrl apiUrl(G_API_BASE_URL + repoFullName + G_LATEST_ENDPOINT);

    // 2. 构造请求并设置必要请求头（GitHub 强制要求 User-Agent）
    QNetworkRequest request(apiUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, buildUserAgent());
    request.setRawHeader(G_HEADER_ACCEPT, G_HEADER_ACCEPT_VALUE);
    request.setRawHeader(G_HEADER_API_VERSION, G_HEADER_API_VERSION_VALUE);
    request.setTransferTimeout(G_REQUEST_TIMEOUT_MS);

    // 3. 发起异步请求，结果经 finished 信号回到 onReplyFinished
    SK_LOG_UPD() << "开始检查更新:" << apiUrl.toString();
    m_networkManager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    // reply 归 QNetworkAccessManager 管理，读取完立即排程释放，避免内存泄漏
    reply->deleteLater();

    // 1. 网络层错误直接上报（含离线 / 超时 / TLS 失败等）
    const QNetworkReply::NetworkError networkError = reply->error();
    if (networkError != QNetworkReply::NoError)
    {
        const QString reason = reply->errorString();
        SK_LOG_UPD() << "更新检查网络失败:" << reason;
        Q_EMIT checkFailed(reason);
        return;
    }

    // 2. 依据 HTTP 状态码分流
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus == 404)
    {
        // 仓库尚无任何 Release：视为「无新版」
        SK_LOG_UPD() << "仓库尚无 Release，视为无新版。";
        Q_EMIT upToDate();
        return;
    }
    if ((httpStatus == 403) || (httpStatus == 429))
    {
        // GitHub API 限流：作为失败处理，不立即重试
        const QString reason = QStringLiteral("GitHub API 限流 (HTTP %1)").arg(httpStatus);
        SK_LOG_UPD() << reason;
        Q_EMIT checkFailed(reason);
        return;
    }
    if (httpStatus != 200)
    {
        const QString reason = QStringLiteral("HTTP %1").arg(httpStatus);
        SK_LOG_UPD() << "更新检查 HTTP 错误:" << reason;
        Q_EMIT checkFailed(reason);
        return;
    }

    // 3. 解析响应 JSON
    const QByteArray payload = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        const QString reason = QStringLiteral("JSON 解析失败: %1").arg(parseError.errorString());
        SK_LOG_UPD() << reason;
        Q_EMIT checkFailed(reason);
        return;
    }
    if (!document.isObject())
    {
        SK_LOG_UPD() << "更新检查响应不是 JSON 对象。";
        Q_EMIT checkFailed(QStringLiteral("响应格式异常。"));
        return;
    }

    const QJsonObject rootObject = document.object();

    // 4. 跳过草稿与预发布：MVP 只认正式版
    const bool isDraft = rootObject.value(G_JSON_DRAFT).toBool();
    const bool isPrerelease = rootObject.value(G_JSON_PRERELEASE).toBool();
    if ((isDraft) || (isPrerelease))
    {
        SK_LOG_UPD() << "最新 Release 为 draft/prerelease，MVP 视为无正式新版。";
        Q_EMIT upToDate();
        return;
    }

    // 5. 版本比对：不高于当前版本则视为最新
    const QString tagName = rootObject.value(G_JSON_TAG_NAME).toString();
    const QString currentVersion = QCoreApplication::applicationVersion();
    if (!isNewerVersion(currentVersion, tagName))
    {
        SK_LOG_UPD() << "当前版本" << currentVersion << "已是最新（tag:" << tagName << "）。";
        Q_EMIT upToDate();
        return;
    }

    // 6. 组装发布信息并按发行版类型挑选下载资产
    ReleaseInfo releaseInfo;
    releaseInfo.versionString = normalizeTag(tagName);
    releaseInfo.tagName = tagName;
    releaseInfo.title = rootObject.value(G_JSON_TITLE).toString();
    releaseInfo.changelogMarkdown = rootObject.value(G_JSON_BODY).toString();
    releaseInfo.releasePageUrl = rootObject.value(G_JSON_HTML_URL).toString();
    releaseInfo.publishedAt =
        QDateTime::fromString(rootObject.value(G_JSON_PUBLISHED_AT).toString(), Qt::ISODate);
    releaseInfo.downloadUrl =
        pickDownloadUrl(rootObject.value(G_JSON_ASSETS).toArray(), isPortableDistribution());

    SK_LOG_INFO() << "发现新版本:" << tagName;
    SK_LOG_UPD() << "下载地址:" << releaseInfo.downloadUrl;
    Q_EMIT updateAvailable(releaseInfo);
}

} // namespace SK::update
