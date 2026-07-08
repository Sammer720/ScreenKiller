/**
 * \file TextItem.cpp
 * \brief 文字标注图元实现
 */
#include "TextItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QGraphicsScene>

namespace SK {

namespace {
/// \brief 文字外扩边距（避免紧贴边框）
constexpr qreal G_TEXT_MARGIN = 4.0;
/// \brief 文字默认背景色（半透明黄色高亮）
const QColor G_TEXT_BG_COLOR(255, 255, 0, 80);
/// \brief 文字对话框标题
const QString G_EDIT_DIALOG_TITLE = QStringLiteral("编辑文字");
/// \brief 文字对话框输入提示
const QString G_EDIT_DIALOG_LABEL = QStringLiteral("文字内容：");
}

TextItem::TextItem(QGraphicsItem* parent)
    : BaseAnnotationItem(parent)
{
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setPenColor(Qt::black);
    setBrushColor(G_TEXT_BG_COLOR);
}

QRectF TextItem::boundingRect() const
{
    QFontMetricsF fm(m_font);

    // 空文本时用一个空格占位，否则 boundingRect 返回空
    QString measureText = m_text.isEmpty() ? QStringLiteral(" ") : m_text;
    QRectF textRect = fm.boundingRect(measureText);
    return textRect.adjusted(-G_TEXT_MARGIN, -G_TEXT_MARGIN,
                              G_TEXT_MARGIN,  G_TEXT_MARGIN);
}

void TextItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setFont(m_font);

    QRectF itemBoundingRect = boundingRect();

    // 可选背景
    if (m_brush.style() != Qt::NoBrush)
    {
        painter->setBrush(m_brush);
        painter->setPen(Qt::NoPen);
        painter->drawRect(itemBoundingRect);
    }
    painter->setPen(m_pen.color());
    painter->drawText(itemBoundingRect, Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

void TextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        BaseAnnotationItem::mouseDoubleClickEvent(event);
        return;
    }

    bool ok = false;
    QString newText = QInputDialog::getText(
        nullptr, G_EDIT_DIALOG_TITLE, G_EDIT_DIALOG_LABEL,
        QLineEdit::Normal, m_text, &ok);

    if (ok)
    {
        setText(newText);
    }
    event->accept();
}

} // namespace SK
