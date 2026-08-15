/**
 * \file UpdateChecker.h
 * \brief GitHub / Gitee 双源并发更新检测器
 *
 * 负责异步查询 GitHub 与 Gitee 两个仓库的最新正式 Release（并发竞速），
 * 解析响应 JSON，比对当前运行版本，并按当前发行版类型（安装版/便携版）
 * 挑选匹配的下载资产。哪个源先成功回包，就用哪个源的版本与下载地址——
 * 在国内网络下 GitHub 不可达时自动落到 Gitee。
 *
 * 设计要点：
 *   - 全部在主线程通过 QNetworkAccessManager 异步完成，不阻塞 UI；
 *   - 竞速取先：首个成功回包立即定案，另一源的在途请求被中止；
 *   - 单源失败（网络/HTTP/解析）不立即判失败，等另一源；
 *   - 双源均失败才通过 checkFailed 信号上报，由上层决定提示策略；
 *   - 只认正式版：跳过 draft 与 prerelease 的 Release。
 */
#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

#include "update/UpdateInfo.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QUrl;

namespace SK::update {

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象；创建后归 Qt 对象树管理
     */
    explicit UpdateChecker(QObject* parent = nullptr);

    /**
     * @brief 异步并发发起一次双源更新检查
     *
     * 立即返回；结果通过 updateAvailable / upToDate / checkFailed 之一异步发出。
     */
    void checkForUpdates();

Q_SIGNALS:
    /// @brief 检测到比当前版本更新的正式 Release
    /// @param releaseInfo 解析后的发布信息（含版本号、变更日志、下载地址等）
    void updateAvailable(const SK::update::ReleaseInfo& releaseInfo);

    /// @brief 当前已是最新版本（或无正式版 / 无 Release）
    void upToDate();

    /// @brief 检查失败
    /// @param reason 失败原因（双源均不可用时的汇总说明）
    void checkFailed(const QString& reason);

private Q_SLOTS:
    /// @brief 网络请求完成回调（两个源共用）
    /// @param reply 已完成请求的响应对象；读取后由本方法负责释放
    void onReplyFinished(QNetworkReply* reply);

private:
    /// @brief 判断当前是否为便携版发行（exe 同目录存在 portable.txt 标记）
    ///
    /// 与 main.cpp 中的便携模式判定同源，用于挑选匹配的下载资产。
    /// @return 便携版返回 true，安装版返回 false
    static bool isPortableDistribution();

    /// @brief 构造带标准请求头的更新请求
    /// @param url 目标 API 地址
    /// @return 已设置 User-Agent / Accept / 超时的请求对象
    QNetworkRequest buildUpdateRequest(const QUrl& url) const;

    /// @brief 解析单个源的 Release 响应
    ///
    /// 仅当响应为 HTTP 200 且 JSON 合法时返回 true；网络错误、非 200、
    /// JSON 解析失败等一律返回 false 并给出 outFailureReason。
    ///
    /// @param reply            已完成的响应对象
    /// @param outInfo          [out] 解析出的发布信息；若最新为预发布，versionString 置空
    /// @param outFailureReason [out] 解析失败原因（仅返回 false 时有意义）
    /// @return 是否解析成功
    bool parseReleaseResponse(QNetworkReply* reply, ReleaseInfo& outInfo, QString& outFailureReason);

    /// @brief 中止尚未完成的另一个源请求
    /// @param finishedReply 已经完成回包的请求（另一个即需要中止的目标）
    void abortOtherReply(QNetworkReply* finishedReply);

    QNetworkAccessManager* m_networkManager = nullptr;  ///< 网络访问管理器（异步请求载体）
    QPointer<QNetworkReply> m_githubReply;               ///< GitHub 源的在途回包（销毁后自动置空，防悬垂）
    QPointer<QNetworkReply> m_giteeReply;                ///< Gitee 源的在途回包（销毁后自动置空，防悬垂）
    bool m_isCheckDone = false;                         ///< 是否已定案（竞速取先，先到先得）
    QStringList m_failureReasons;                       ///< 各源失败原因收集（双源失败时汇总）
};

} // namespace SK::update
