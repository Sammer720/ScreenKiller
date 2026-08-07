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
/// \brief 缩放时的最小字号保护（pt），避免文字小到不可读
constexpr qreal G_MIN_FONT_SIZE = 6.0;
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

void TextItem::paintContent(QPainter* painter,
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

QRectF TextItem::resizeRect() const
{
    return boundingRect();
}

void TextItem::setResizeRect(const QRectF& newRect)
{
    // 1. 取旧包围盒高度作为缩放基准
    QRectF oldRect = boundingRect();
    qreal oldHeight = oldRect.height();

    // 2. 旧高度过小说明文本几乎不可见，无法可靠缩放，直接放弃
    if (oldHeight < G_MIN_RESIZE_SIZE)
    {
        return;
    }

    // 3. 按高度比例计算新字号，与拖拽手柄的纵向尺寸变化保持一致
    qreal scaleFactor = newRect.height() / oldHeight;
    QFont newFont = m_font;
    qreal newSize = m_font.pointSizeF() * scaleFactor;

    // 4. 最小字号保护，避免文字小到不可读
    if (newSize < G_MIN_FONT_SIZE)
    {
        newSize = G_MIN_FONT_SIZE;
    }
    newFont.setPointSizeF(newSize);
    setFont(newFont);
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
