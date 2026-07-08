/**
 * \file TextItem.h
 * \brief 文字标注图元
 *
 * 点击画布弹出输入框（QInputDialog），确认后转为画布文本对象。
 * 双击可再次进入编辑模式。
 */
#pragma once

#include "BaseAnnotationItem.h"

#include <QFont>
#include <QString>

namespace SK {

/**
 * @brief 文字标注图元
 *
 * 内部维护文本与字体，paint() 时绘制可选背景矩形+文字。
 */
class TextItem : public BaseAnnotationItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit TextItem(QGraphicsItem* parent = nullptr);

    /// @brief 返回包围盒（含边距）
    QRectF boundingRect() const override;
    /// @brief 绘制背景与文字
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    /// @brief 设置文字内容
    /// @param t 文字
    void setText(const QString& t) { m_text = t; prepareGeometryChange(); update(); }
    /// @brief 获取文字内容
    QString text() const { return m_text; }

    /// @brief 设置字体
    /// @param f 字体
    void setFont(const QFont& f) { m_font = f; prepareGeometryChange(); update(); }
    /// @brief 获取字体
    QFont font() const { return m_font; }

protected:
    /// @brief 双击进入文字编辑对话框
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QString m_text;   ///< 文字内容
    QFont   m_font { "Microsoft YaHei UI", 14 };   ///< 字体（默认微软雅黑 14pt）
};

} // namespace SK
