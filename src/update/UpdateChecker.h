/**
 * \file UpdateChecker.h
 * \brief GitHub Releases 更新检测器
 *
 * 负责异步查询 GitHub 仓库的最新正式 Release，解析响应 JSON，比对当前
 * 运行版本，并按当前发行版类型（安装版/便携版）挑选匹配的下载资产。
 *
 * 设计要点：
 *   - 全部在主线程通过 QNetworkAccessManager 异步完成，不阻塞 UI；
 *   - 网络/解析/限流等异常统一通过 checkFailed 信号上报，由上层决定提示策略；
 *   - 只认正式版：跳过 draft 与 prerelease 的 Release。
 */
#pragma once

#include <QObject>

#include "update/UpdateInfo.h"

class QNetworkAccessManager;
class QNetworkReply;

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
     * @brief 异步发起一次更新检查
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
    /// @param reason 失败原因（网络错误 / HTTP 状态码 / JSON 解析错误等）
    void checkFailed(const QString& reason);

private Q_SLOTS:
    /// @brief 网络请求完成回调
    /// @param reply 已完成请求的响应对象；读取后由本方法负责释放
    void onReplyFinished(QNetworkReply* reply);

private:
    /// @brief 判断当前是否为便携版发行（exe 同目录存在 portable.txt 标记）
    ///
    /// 与 main.cpp 中的便携模式判定同源，用于挑选匹配的下载资产。
    /// @return 便携版返回 true，安装版返回 false
    static bool isPortableDistribution();

    QNetworkAccessManager* m_networkManager = nullptr;  ///< 网络访问管理器（异步请求载体）
};

} // namespace SK::update
