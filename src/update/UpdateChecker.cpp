/**
 * \file UpdateChecker.cpp
 * \brief GitHub / Gitee 双源并发更新检测器实现
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
const QString G_GITHUB_API_BASE_URL = QStringLiteral("https://api.github.com/repos/");
/// \brief Gitee API 基址（国内网络访问更稳定，作为回落源）
const QString G_GITEE_API_BASE_URL = QStringLiteral("https://gitee.com/api/v5/repos/");
/// \brief releases/latest 端点后缀（两个源共用）
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

/// \brief 构建 API 要求的 User-Agent（GitHub 强制要求非空 UA）
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
/// @param assets     API 返回的 assets 数组（GitHub 与 Gitee 结构一致）
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

QNetworkRequest UpdateChecker::buildUpdateRequest(const QUrl& url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, buildUserAgent());
    request.setRawHeader(G_HEADER_ACCEPT, G_HEADER_ACCEPT_VALUE);
    request.setRawHeader(G_HEADER_API_VERSION, G_HEADER_API_VERSION_VALUE);
    request.setTransferTimeout(G_REQUEST_TIMEOUT_MS);
    return request;
}

void UpdateChecker::checkForUpdates()
{
    // 1. 先中止上一次可能仍在途的请求，防止新旧回包相互干扰
    //    （旧回包的 finished 回调会因指针不匹配而被忽略）
    m_isCheckDone = true;
    if ((m_githubReply != nullptr) && (!m_githubReply->isFinished()))
    {
        m_githubReply->abort();
    }
    if ((m_giteeReply != nullptr) && (!m_giteeReply->isFinished()))
    {
        m_giteeReply->abort();
    }

    // 2. 重置本轮竞速状态
    m_isCheckDone = false;
    m_failureReasons.clear();

    // 3. 并发发起 GitHub 与 Gitee 两个 latest 请求
    const QUrl githubUrl(G_GITHUB_API_BASE_URL + QStringLiteral(SK_GITHUB_REPO) + G_LATEST_ENDPOINT);
    const QUrl giteeUrl(G_GITEE_API_BASE_URL + QStringLiteral(SK_GITEE_REPO) + G_LATEST_ENDPOINT);

    SK_LOG_UPD() << "并发检查更新: GitHub=" << githubUrl.toString()
                 << " Gitee=" << giteeUrl.toString();
    m_githubReply = m_networkManager->get(buildUpdateRequest(githubUrl));
    m_giteeReply = m_networkManager->get(buildUpdateRequest(giteeUrl));
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    // reply 归 QNetworkAccessManager 管理，无论是否使用都排程释放
    reply->deleteLater();

    // 过期回包（上一次检查被中止后迟到的）或已定案：忽略
    if ((reply != m_githubReply) && (reply != m_giteeReply))
    {
        return;
    }
    if (m_isCheckDone)
    {
        return;
    }

    // 1. 解析当前源的回包；解析失败则记录原因并等另一个源
    ReleaseInfo releaseInfo;
    QString failureReason;
    const bool isParseSuccess = parseReleaseResponse(reply, releaseInfo, failureReason);
    if (!isParseSuccess)
    {
        m_failureReasons.append(failureReason);

        // 双源均已失败：定案失败
        const QNetworkReply* otherReply = (reply == m_githubReply) ? m_giteeReply : m_githubReply;
        if ((otherReply != nullptr) && (otherReply->isFinished()))
        {
            m_isCheckDone = true;
            const QString combinedReason = m_failureReasons.join(QStringLiteral("；"));
            SK_LOG_UPD() << "更新检查失败（双源均不可用）:" << combinedReason;
            Q_EMIT checkFailed(combinedReason);
        }
        return;
    }

    // 2. 竞速取先：首个成功回包立即定案，并中止另一源的在途请求
    m_isCheckDone = true;
    abortOtherReply(reply);

    // 3. 预发布 / 无版本号：视为无正式新版
    if (releaseInfo.versionString.isEmpty())
    {
        SK_LOG_UPD() << "该源最新为预发布或无版本号，视为无正式新版。";
        Q_EMIT upToDate();
        return;
    }

    // 4. 版本比对：不高于当前版本则视为最新
    const QString currentVersion = QCoreApplication::applicationVersion();
    if (!isNewerVersion(currentVersion, releaseInfo.tagName))
    {
        SK_LOG_UPD() << "当前版本" << currentVersion << "已是最新（tag:" << releaseInfo.tagName << "）。";
        Q_EMIT upToDate();
        return;
    }

    // 5. 有新版：上报（下载地址与发布页均指向先回包的源）
    SK_LOG_INFO() << "发现新版本:" << releaseInfo.tagName;
    SK_LOG_UPD() << "下载地址:" << releaseInfo.downloadUrl;
    Q_EMIT updateAvailable(releaseInfo);
}

bool UpdateChecker::parseReleaseResponse(QNetworkReply* reply, ReleaseInfo& outInfo, QString& outFailureReason)
{
    // 1. 网络层错误直接上报（含离线 / 超时 / TLS 失败等）
    const QNetworkReply::NetworkError networkError = reply->error();
    if (networkError != QNetworkReply::NoError)
    {
        outFailureReason = reply->errorString();
        return false;
    }

    // 2. 依据 HTTP 状态码分流
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus == 404)
    {
        outFailureReason = QStringLiteral("HTTP 404（该源暂无 Release）");
        return false;
    }
    if ((httpStatus == 403) || (httpStatus == 429))
    {
        outFailureReason = QStringLiteral("HTTP %1（限流）").arg(httpStatus);
        return false;
    }
    if (httpStatus != 200)
    {
        outFailureReason = QStringLiteral("HTTP %1").arg(httpStatus);
        return false;
    }

    // 3. 解析响应 JSON
    const QByteArray payload = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        outFailureReason = QStringLiteral("JSON 解析失败: %1").arg(parseError.errorString());
        return false;
    }
    if (!document.isObject())
    {
        outFailureReason = QStringLiteral("响应不是 JSON 对象");
        return false;
    }

    const QJsonObject rootObject = document.object();
    const QString tagName = rootObject.value(G_JSON_TAG_NAME).toString();

    // 4. 组装发布信息并按发行版类型挑选下载资产
    outInfo.tagName = tagName;
    outInfo.versionString = normalizeTag(tagName);
    outInfo.title = rootObject.value(G_JSON_TITLE).toString();
    outInfo.changelogMarkdown = rootObject.value(G_JSON_BODY).toString();
    outInfo.releasePageUrl = rootObject.value(G_JSON_HTML_URL).toString();
    outInfo.publishedAt =
        QDateTime::fromString(rootObject.value(G_JSON_PUBLISHED_AT).toString(), Qt::ISODate);
    outInfo.downloadUrl =
        pickDownloadUrl(rootObject.value(G_JSON_ASSETS).toArray(), isPortableDistribution());

    // 5. 草稿 / 预发布：置空版本号，表示「该源无正式新版」
    const bool isDraft = rootObject.value(G_JSON_DRAFT).toBool();
    const bool isPrerelease = rootObject.value(G_JSON_PRERELEASE).toBool();
    if ((isDraft) || (isPrerelease))
    {
        outInfo.versionString.clear();
    }

    return true;
}

void UpdateChecker::abortOtherReply(QNetworkReply* finishedReply)
{
    QNetworkReply* otherReply = (finishedReply == m_githubReply) ? m_giteeReply : m_githubReply;
    if ((otherReply != nullptr) && (!otherReply->isFinished()))
    {
        otherReply->abort();
    }
}

} // namespace SK::update
