/**
 * \file GuidePanel.cpp
 * \brief 标注页悬浮引导面板实现
 *
 * 实现要点：
 *   1. 面板背景不依赖 QSS，直接在 paintEvent 中自绘半透明圆角矩形（WA_TranslucentBackground）。
 *   2. 内容区用 QLabel 富文本展示“鼠标按键图标 + 五行操作说明”（拖动平移 / 滚动缩放 / 点击复位 / 点击复制 / Ctrl+S保存），
 *      表格用 HTML attribute 形式指定列宽（Qt 富文本对 <td> CSS width 支持有限），第三列右侧留白用 &nbsp; 补位压到最小；
 *      缩放比例独立一个 QLabel。
 *   3. 左键点击整块面板在 折叠 / 展开 两种形态间切换，折叠时隐藏全部内容并收缩为小方块。
 */
#include "GuidePanel.h"

#include <QColor>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QtGlobal>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace SK {

namespace {
/// \brief 面板背景色（#FFE4B5 柔和黄 + 70% 不透明度）
constexpr int G_BG_R = 255;   // #FFE4B5
constexpr int G_BG_G = 228;
constexpr int G_BG_B = 181;
constexpr int G_BG_A = 180;   // ~70% 不透明度
/// \brief 圆角半径
constexpr qreal G_CORNER_RADIUS = 10.0;
/// \brief 面板内边距
constexpr int G_PADDING = 5;
/// \brief 展开尺寸（容纳 5 行操作提示：拖动平移 / 滚动缩放 / 点击复位 / 右键复制 / Ctrl+S保存 + 底部信息行）
constexpr int G_EXPANDED_W = 160;
constexpr int G_EXPANDED_H = 220;
/// \brief 折叠尺寸
constexpr int G_COLLAPSED_SIZE = 40;
/// \brief 折叠状态下图标占面板边长的比例（缩放到 70% 居中显示）
constexpr qreal G_COLLAPSED_ICON_RATIO = 0.7;
/// \brief 初始缩放显示文本
const QString G_ZOOM_TEXT_INITIAL = QStringLiteral("缩放: 100%");
/// \brief 缩放比例标签样式（深橙棕文字 + 中粗字重）
const QString G_ZOOM_STYLE = QStringLiteral(
    "color: #8B5A2B; font-size: 12px; font-weight: 600;");
/// \brief 左下角「点击隐藏」提示文本
const QString G_HINT_TEXT = QStringLiteral("点击隐藏");
/// \brief 「点击隐藏」小字样式（正文同色 + 半透明弱化，字号小于缩放标签）
const QString G_HINT_STYLE = QStringLiteral(
    "color: rgb(90, 62, 27); font-size: 12px;");
/// \brief 操作提示富文本（鼠标按键图标 + 四行说明 + Ctrl+S 快捷键行）
/// QTextDocument 对 <td> 的 CSS width 支持有限，改用 HTML attribute 形式
/// 让 Qt HtmlParser 正确解析列宽约束（绝对值 40/65 固定前两列，第三列吃剩余）；
/// 第三列 text-align:right 让右侧文字紧贴 content 区右缘；
/// Ctrl+S 行用 colspan 合并前两列写快捷键文本，与鼠标操作行风格一致
const QString G_CONTENT_HTML = QStringLiteral(
    "<div style='color: #5A3E1B; font-size: 15px;'>"
    "<table border='0' cellspacing='0' cellpadding='0' style='vertical-align: middle;' width='100%'>"
    "<tr height='32'>"
    "<td width='40' style='padding-right: 8px; vertical-align: middle; text-align: left;'>"
    "<img src=':/icons/mouse_mid.png' width='32' height='32' style='vertical-align: middle;'/>"
    "</td>"
    "<td width='65' style='padding-right: 8px; vertical-align: middle; text-align: left; white-space: nowrap;'>+ 拖动</td>"
    "<td style='vertical-align: middle; text-align: right; white-space: nowrap;'>&nbsp;平移</td>"
    "</tr>"
    "<tr height='32'>"
    "<td width='40' style='padding-right: 8px; vertical-align: middle; text-align: left;'>"
    "<img src=':/icons/mouse_mid.png' width='32' height='32' style='vertical-align: middle;'/>"
    "</td>"
    "<td width='65' style='padding-right: 8px; vertical-align: middle; text-align: left; white-space: nowrap;'>+ 滚动</td>"
    "<td style='vertical-align: middle; text-align: right; white-space: nowrap;'>&nbsp;缩放</td>"
    "</tr>"
    "<tr height='32'>"
    "<td width='40' style='padding-right: 8px; vertical-align: middle; text-align: left;'>"
    "<img src=':/icons/mouse_mid.png' width='32' height='32' style='vertical-align: middle;'/>"
    "</td>"
    "<td width='65' style='padding-right: 8px; vertical-align: middle; text-align: left; white-space: nowrap;'>+ 点击</td>"
    "<td style='vertical-align: middle; text-align: right; white-space: nowrap;'>&nbsp;复位</td>"
    "</tr>"
    "<tr height='32'>"
    "<td width='40' style='padding-right: 8px; vertical-align: middle; text-align: left;'>"
    "<img src=':/icons/mouse_right.png' width='32' height='32' style='vertical-align: middle;'/>"
    "</td>"
    "<td width='65' style='padding-right: 8px; vertical-align: middle; text-align: left; white-space: nowrap;'>+ 点击</td>"
    "<td style='vertical-align: middle; text-align: right; white-space: nowrap;'>&nbsp;复制</td>"
    "</tr>"
    "<tr height='32'>"
    "<td colspan='2' width='105' style='padding-right: 8px; vertical-align: middle; text-align: left; white-space: nowrap; line-height: 32px;'>"
    "Ctrl + S"
    "</td>"
    "<td style='vertical-align: middle; text-align: right; white-space: nowrap; line-height: 32px;'>保存</td>"
    "</tr>"
    "</table>"
    "</div>");
}

GuidePanel::GuidePanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setObjectName(QStringLiteral("guidePanel"));

    // 1. 初始化操作提示标签：富文本展示鼠标中键图标 + 文字
    m_contentLabel = new QLabel(this);
    m_contentLabel->setObjectName(QStringLiteral("guideContent"));
    m_contentLabel->setTextFormat(Qt::RichText);
    m_contentLabel->setText(G_CONTENT_HTML);

    // 2. 初始化「点击隐藏」提示标签：左下角小字，弱化显示
    m_hintLabel = new QLabel(G_HINT_TEXT, this);
    m_hintLabel->setObjectName(QStringLiteral("guideHint"));
    m_hintLabel->setAlignment(Qt::AlignLeft);
    m_hintLabel->setStyleSheet(G_HINT_STYLE);

    // 3. 初始化缩放比例标签：默认 100%，右下角对齐
    m_zoomLabel = new QLabel(G_ZOOM_TEXT_INITIAL, this);
    m_zoomLabel->setObjectName(QStringLiteral("guideZoom"));
    m_zoomLabel->setAlignment(Qt::AlignRight);
    m_zoomLabel->setStyleSheet(G_ZOOM_STYLE);

    // 4. 底部一行：左侧「点击隐藏」提示 + 右侧缩放比例（同一行，不增加面板行数）
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(2);
    bottomLayout->addWidget(m_hintLabel, 0, Qt::AlignLeft);
    bottomLayout->addWidget(m_zoomLabel, 0, Qt::AlignRight);

    // 5. 垂直布局：提示内容在上、底部一行在下
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(G_PADDING, G_PADDING, G_PADDING, G_PADDING);
    layout->setSpacing(4);
    layout->addWidget(m_contentLabel);
    layout->addStretch();
    layout->addLayout(bottomLayout);

    setFixedSize(G_EXPANDED_W, G_EXPANDED_H);
}

void GuidePanel::setZoomScale(qreal scale)
{
    // 缩放因子转百分比并四舍五入到整数，例如 1.25 显示为 "缩放: 125%"
    const int percent = qRound(scale * 100.0);
    m_zoomLabel->setText(QStringLiteral("缩放: %1%").arg(percent));
}

void GuidePanel::mousePressEvent(QMouseEvent* event)
{
    // 仅响应左键：整块面板作为开关按钮，其余按键交回基类默认处理
    if (event->button() == Qt::LeftButton)
    {
        toggleCollapsed();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void GuidePanel::toggleCollapsed()
{
    m_collapsed = !m_collapsed;
    if (m_collapsed)
    {
        // 折叠：隐藏全部内容，缩小为小浮动控件
        m_contentLabel->setVisible(false);
        m_hintLabel->setVisible(false);
        m_zoomLabel->setVisible(false);
        setFixedSize(G_COLLAPSED_SIZE, G_COLLAPSED_SIZE);
    }
    else
    {
        // 展开：恢复完整面板尺寸与内容
        m_contentLabel->setVisible(true);
        m_hintLabel->setVisible(true);
        m_zoomLabel->setVisible(true);
        setFixedSize(G_EXPANDED_W, G_EXPANDED_H);
    }
}

void GuidePanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 自绘半透明圆角背景：填充色从具名常量组合，保证抗锯齿平滑
    QPainterPath path;
    path.addRoundedRect(rect(), G_CORNER_RADIUS, G_CORNER_RADIUS);
    painter.fillPath(path, QColor(G_BG_R, G_BG_G, G_BG_B, G_BG_A));

    // 折叠状态：绘制 help.png 图标居中，避免收缩后只剩一个空方块
    if (m_collapsed)
    {
        QPixmap helpIcon(QStringLiteral(":/icons/help.png"));
        if (!helpIcon.isNull())
        {
            // 图标按折叠尺寸的 70% 缩放，居中绘制
            int iconSize = static_cast<int>(G_COLLAPSED_SIZE * G_COLLAPSED_ICON_RATIO);
            int iconX    = (width() - iconSize) / 2;
            int iconY    = (height() - iconSize) / 2;
            painter.drawPixmap(iconX, iconY, helpIcon.scaled(iconSize, iconSize,
                                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

} // namespace SK