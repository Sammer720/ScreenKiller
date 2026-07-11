/**
 * \file ScrollOverlay.cpp
 * \brief ScrollOverlay 实现
 */
#include "ScrollOverlay.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>

namespace {

/// \brief 浮窗默认宽度
constexpr int G_OVERLAY_WIDTH = 440;
/// \brief 浮窗默认高度
constexpr int G_OVERLAY_HEIGHT = 56;
/// \brief 浮窗距离屏幕顶部偏移
constexpr int G_OVERLAY_TOP_OFFSET = 16;

} // namespace

ScrollOverlay::ScrollOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(G_OVERLAY_WIDTH, G_OVERLAY_HEIGHT);
    setObjectName(QStringLiteral("scrollOverlay"));

    // 标题：提示用户操作
    m_titleLabel = new QLabel(tr("鼠标滚动目标窗口截屏"), this);
    m_titleLabel->setObjectName(QStringLiteral("scrollOverlayTitle"));

    // 帧数
    m_countLabel = new QLabel(tr("已捕获 0 帧"), this);
    m_countLabel->setObjectName(QStringLiteral("scrollOverlayCount"));

    // 完成按钮
    m_finishButton = new QPushButton(tr("完成"), this);
    m_finishButton->setObjectName(QStringLiteral("scrollOverlayFinish"));
    m_finishButton->setDefault(true);

    // 取消按钮
    m_cancelButton = new QPushButton(tr("取消"), this);
    m_cancelButton->setObjectName(QStringLiteral("scrollOverlayCancel"));

    // 布局：标题 + 帧数 + 弹性间距 + 完成 + 取消
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 12, 0);
    layout->setSpacing(12);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_countLabel);
    layout->addStretch(1);
    layout->addWidget(m_finishButton);
    layout->addWidget(m_cancelButton);

    // 连接信号
    connect(m_finishButton, &QPushButton::clicked,
            this, &ScrollOverlay::finishRequested);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &ScrollOverlay::cancelRequested);

    // 设置样式（与项目全局 Lavender & Cream 主题一致）
    setStyleSheet(QStringLiteral(R"(
        #scrollOverlay {
            background: rgba(251, 247, 239, 0.75);
            border: none;
            border-radius: 10px;
        }
        #scrollOverlayTitle {
            color: #3A3357;
            font-size: 13px;
            font-weight: 500;
            background: transparent;
            border: none;
        }
        #scrollOverlayCount {
            color: #726597;
            font-size: 12px;
            background: transparent;
            border: none;
        }
        #scrollOverlayFinish {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #9B8AC8, stop:1 #6B5B95);
            color: #FFFFFF;
            border: 1px solid #5A4B85;
            border-radius: 6px;
            padding: 6px 18px;
            font-weight: 600;
            font-size: 13px;
            min-width: 60px;
        }
        #scrollOverlayFinish:hover {
            background: #7B6BA5;
            border: 1px solid #5A4B85;
        }
        #scrollOverlayFinish:pressed {
            background: #4A3F6E;
        }
        #scrollOverlayCancel {
            background: transparent;
            color: #3A3357;
            border: 1px solid #D9CFC1;
            border-radius: 6px;
            padding: 6px 18px;
            font-size: 13px;
            min-width: 60px;
        }
        #scrollOverlayCancel:hover {
            background: #F0E6E6;
            border: 1px solid #C89898;
            color: #6A1F1F;
        }
        #scrollOverlayCancel:pressed {
            background: #C89898;
            color: #FFFFFF;
            border: 1px solid #A87878;
        }
    )"));

    positionAtTopCenter();
}

ScrollOverlay::~ScrollOverlay() = default;

void ScrollOverlay::setFrameCount(int count)
{
    m_countLabel->setText(tr("已捕获 %1 帧").arg(count));
}

void ScrollOverlay::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
        Q_EMIT cancelRequested();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        Q_EMIT finishRequested();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void ScrollOverlay::positionAtTopCenter()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr)
    {
        return;
    }

    QRect screenGeo = screen->geometry();
    int x = screenGeo.center().x() - (width() / 2);
    int y = screenGeo.top() + G_OVERLAY_TOP_OFFSET;
    move(x, y);
}