/**
 * \file UpdateDialog.cpp
 * \brief 更新提示对话框实现
 */
#include "UpdateDialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace SK::update {

namespace {

/// \brief 对话框默认宽度（像素）
constexpr int G_DIALOG_WIDTH = 520;
/// \brief 对话框默认高度（像素）
constexpr int G_DIALOG_HEIGHT = 420;
/// \brief 变更日志为空时的占位文案
const QString G_EMPTY_CHANGELOG_TEXT = QStringLiteral("本次更新未提供详细说明。");

} // namespace

UpdateDialog::UpdateDialog(const ReleaseInfo& releaseInfo, QWidget* parent)
    : QDialog(parent)
    , m_releaseInfo(releaseInfo)
{
    setupUi();
    resize(G_DIALOG_WIDTH, G_DIALOG_HEIGHT);
}

UpdateDialog::Action UpdateDialog::selectedAction() const
{
    return m_selectedAction;
}

void UpdateDialog::setupUi()
{
    setWindowTitle(tr("软件更新"));

    auto* rootLayout = new QVBoxLayout(this);

    // 标题与版本号
    auto* titleLabel = new QLabel(tr("发现新版本"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* versionLabel = new QLabel(tr("ScreenKiller v%1").arg(m_releaseInfo.versionString), this);

    // 变更日志浏览器：以 Markdown 渲染发布说明正文，只读
    auto* changelogBrowser = new QTextBrowser(this);
    const QString changelogText = m_releaseInfo.changelogMarkdown.isEmpty()
                                      ? G_EMPTY_CHANGELOG_TEXT
                                      : m_releaseInfo.changelogMarkdown;
    changelogBrowser->setMarkdown(changelogText);
    changelogBrowser->setOpenExternalLinks(false);

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(versionLabel);
    rootLayout->addWidget(changelogBrowser, 1);

    // 动作按钮区
    auto* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    auto* downloadButton = buttonBox->addButton(tr("前往下载"), QDialogButtonBox::AcceptRole);
    auto* laterButton = buttonBox->addButton(tr("稍后"), QDialogButtonBox::RejectRole);
    auto* skipButton = buttonBox->addButton(tr("跳过此版本"), QDialogButtonBox::ActionRole);
    rootLayout->addWidget(buttonBox);

    // 连接各按钮：记录选择并关闭对话框
    connect(downloadButton, &QPushButton::clicked, this, [this]()
    {
        m_selectedAction = Action::Download;
        accept();
    });
    connect(laterButton, &QPushButton::clicked, this, [this]()
    {
        m_selectedAction = Action::Later;
        reject();
    });
    connect(skipButton, &QPushButton::clicked, this, [this]()
    {
        m_selectedAction = Action::Skip;
        reject();
    });
}

} // namespace SK::update
