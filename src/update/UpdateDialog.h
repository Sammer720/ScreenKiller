/**
 * \file UpdateDialog.h
 * \brief 更新提示对话框
 *
 * 展示一次可用的更新发布信息（版本号 + Markdown 变更日志），并提供三个
 * 用户可选的后续动作：前往下载 / 稍后 / 跳过此版本。
 *
 * 通过 exec() 模态展示后，调用 selectedAction() 读取用户最终选择。
 */
#pragma once

#include <QDialog>

#include "update/UpdateInfo.h"

class QLabel;
class QTextBrowser;

namespace SK::update {

class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief 用户在更新对话框中选择的后续动作
     */
    enum class Action
    {
        Download,  ///< 前往下载
        Later,     ///< 稍后再说
        Skip       ///< 跳过此版本
    };

    /**
     * @brief 构造函数
     * @param releaseInfo 待展示的发布信息
     * @param parent 父窗口；可为 nullptr
     */
    explicit UpdateDialog(const ReleaseInfo& releaseInfo, QWidget* parent = nullptr);

    /**
     * @brief 获取用户最终选择的动作
     * @return 用户点击的按钮对应的 Action；未选择时为 Later
     */
    Action selectedAction() const;

private:
    /// @brief 构建界面控件与布局，并连接三个动作按钮的点击逻辑
    void setupUi();

    ReleaseInfo m_releaseInfo;                  ///< 待展示的发布信息副本
    Action      m_selectedAction = Action::Later;  ///< 用户最终选择，默认「稍后」
};

} // namespace SK::update
