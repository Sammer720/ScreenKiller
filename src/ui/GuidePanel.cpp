/**
 * \file GuidePanel.cpp
 * \brief 标注页悬浮引导面板实现
 *
 * 实现要点：
 *   1. 面板背景不依赖 QSS，直接在 paintEvent 中自绘半透明圆角矩形（WA_TranslucentBackground）。
 *   2. 内容区用 QGridLayout 逐行摆放提示：
 *        - 图标行：32px 图标（列0）+ 操作描述（列1）+ 含义（列2，右对齐）
 *        - 快捷键行：按键文本跨列0/列1 左对齐（与图标行左缘对齐），含义在列2 右对齐
 *      改用网格布局而非富文本表格的原因：Qt 富文本表格会把跨列单元格的内容宽度
 *      均摊到被跨的各列上，「Del + Del + Del」等较长文本会把图标列撑宽，
 *      网格布局用固定图标列 + 拉伸文本列精确控制，图标列恒为 32px。
 *   3. 左键点击整块面板在 折叠 / 展开 两种形态间切换，折叠时隐藏全部内容并收缩为小方块。
 */
#include "GuidePanel.h"

#include <QColor>
#include <QGridLayout>
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
/// \brief 展开尺寸：宽度需容纳 图标列32 + 快捷键列（"Del + Del + Del"）+ 含义列（"清空全部"）
constexpr int G_EXPANDED_W = 240;
/// \brief 展开高度：9 行提示 x 32 + 底部信息行 + 边距
constexpr int G_EXPANDED_H = 320;
/// \brief 折叠尺寸
constexpr int G_COLLAPSED_SIZE = 40;
/// \brief 折叠状态下图标占面板边长的比例（缩放到 70% 居中显示）
constexpr qreal G_COLLAPSED_ICON_RATIO = 0.7;
/// \brief 图标列宽度（与 32px 图标一致）
constexpr int G_ICON_COLUMN_WIDTH = 32;
/// \brief 网格列间距（像素，替代原富文本单元格的 padding-right）
constexpr int G_GRID_SPACING = 8;
/// \brief 提示行最小高度（与 32px 图标匹配，保证行内垂直居中空间）
constexpr int G_ROW_HEIGHT = 32;
/// \brief 内容字号（与原富文本 div 的 font-size 一致）
constexpr int G_CONTENT_FONT_SIZE = 15;
/// \brief 内容文字颜色（与原富文本 div 的 color 一致）
const QString G_CONTENT_COLOR = QStringLiteral("#5A3E1B");
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
/// \brief 内容行文字样式（字号 + 颜色，背景由全局 QSS 保证透明）
const QString G_CONTENT_STYLE = QStringLiteral(
    "font-size: %1px; color: %2;")
        .arg(G_CONTENT_FONT_SIZE)
        .arg(G_CONTENT_COLOR);
} // namespace

GuidePanel::GuidePanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setObjectName(QStringLiteral("guidePanel"));

    // 1. 初始化操作提示区：网格布局承载图标行与快捷键行
    m_contentWidget = new QWidget(this);
    m_contentWidget->setObjectName(QStringLiteral("guideContent"));
    auto* guideGrid = new QGridLayout(m_contentWidget);
    guideGrid->setContentsMargins(0, 0, 0, 0);
    guideGrid->setHorizontalSpacing(G_GRID_SPACING);
    guideGrid->setVerticalSpacing(0);
    guideGrid->setColumnMinimumWidth(0, G_ICON_COLUMN_WIDTH);
    guideGrid->setColumnStretch(1, 1);

    buildGuideRows(guideGrid);

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
    layout->addWidget(m_contentWidget);
    layout->addStretch();
    layout->addLayout(bottomLayout);

    setFixedSize(G_EXPANDED_W, G_EXPANDED_H);
}

void GuidePanel::buildGuideRows(QGridLayout* guideGrid)
{
    // 图标行数据：图标资源 + 操作描述 + 含义
    struct IconRowSpec
    {
        QString iconPath;
        QString operation;
        QString meaning;
    };
    const IconRowSpec iconRows[] = {
        { QStringLiteral(":/icons/mouse_mid.png"),   QStringLiteral("+ 拖动"), QStringLiteral("平移") },
        { QStringLiteral(":/icons/mouse_mid.png"),   QStringLiteral("+ 滚动"), QStringLiteral("缩放") },
        { QStringLiteral(":/icons/mouse_mid.png"),   QStringLiteral("+ 点击"), QStringLiteral("复位") },
        { QStringLiteral(":/icons/mouse_right.png"), QStringLiteral("+ 点击"), QStringLiteral("复制") },
    };

    // 快捷键行数据：按键组合 + 含义
    struct KeysRowSpec
    {
        QString keys;
        QString meaning;
    };
    const KeysRowSpec keysRows[] = {
        { QStringLiteral("Ctrl + S"),        QStringLiteral("保存") },
        { QStringLiteral("Del + Del"),       QStringLiteral("清空编辑") },
        { QStringLiteral("Del + Del + Del"), QStringLiteral("清空全部") },
        { QStringLiteral("Ctrl + Z"),        QStringLiteral("撤销") },
        { QStringLiteral("Ctrl + Y"),        QStringLiteral("重做") },
    };

    int rowIndex = 0;

    // 1. 图标行：图标(列0) + 操作描述(列1) + 含义(列2，右对齐)
    for (const IconRowSpec& rowSpec : iconRows)
    {
        auto* iconLabel = new QLabel(m_contentWidget);
        iconLabel->setPixmap(QPixmap(rowSpec.iconPath));
        iconLabel->setFixedSize(G_ICON_COLUMN_WIDTH, G_ICON_COLUMN_WIDTH);
        iconLabel->setScaledContents(true);

        auto* opLabel = new QLabel(rowSpec.operation, m_contentWidget);
        opLabel->setStyleSheet(G_CONTENT_STYLE);
        opLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto* meaningLabel = new QLabel(rowSpec.meaning, m_contentWidget);
        meaningLabel->setStyleSheet(G_CONTENT_STYLE);
        meaningLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        guideGrid->addWidget(iconLabel, rowIndex, 0, Qt::AlignLeft | Qt::AlignVCenter);
        guideGrid->addWidget(opLabel, rowIndex, 1, Qt::AlignLeft | Qt::AlignVCenter);
        guideGrid->addWidget(meaningLabel, rowIndex, 2, Qt::AlignRight | Qt::AlignVCenter);
        guideGrid->setRowMinimumHeight(rowIndex, G_ROW_HEIGHT);
        ++rowIndex;
    }

    // 2. 快捷键行：按键文本跨列0/列1 左对齐（与图标行左缘对齐），含义在列2 右对齐
    for (const KeysRowSpec& rowSpec : keysRows)
    {
        auto* keysLabel = new QLabel(rowSpec.keys, m_contentWidget);
        keysLabel->setStyleSheet(G_CONTENT_STYLE);
        keysLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto* meaningLabel = new QLabel(rowSpec.meaning, m_contentWidget);
        meaningLabel->setStyleSheet(G_CONTENT_STYLE);
        meaningLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        guideGrid->addWidget(keysLabel, rowIndex, 0, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
        guideGrid->addWidget(meaningLabel, rowIndex, 2, Qt::AlignRight | Qt::AlignVCenter);
        guideGrid->setRowMinimumHeight(rowIndex, G_ROW_HEIGHT);
        ++rowIndex;
    }
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
        m_contentWidget->setVisible(false);
        m_hintLabel->setVisible(false);
        m_zoomLabel->setVisible(false);
        setFixedSize(G_COLLAPSED_SIZE, G_COLLAPSED_SIZE);
    }
    else
    {
        // 展开：恢复完整面板尺寸与内容
        m_contentWidget->setVisible(true);
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
