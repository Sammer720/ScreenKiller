/**
 * \file TextItem.h
 * \brief 文字标注图元
 *
 * 点击画布后由 AnnotationView 在点击位置弹出内联编辑器，
 * 确认后转为画布文本对象。
 */
#pragma once

#include "BaseAnnotationItem.h"

#include <QFont>
#include <QString>

namespace SK {

/**
 * @brief 文字标注图元
 *
 * 内部维护文本与字体，paintContent() 时绘制可选背景矩形+文字。
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
    /// @brief 绘制背景与文字（由基类 NVI paint() 回调）
    void paintContent(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget = nullptr) override;

    /// @brief 获取缩放参考矩形（文字包围盒）
    QRectF resizeRect() const override;
    /// @brief 按新的缩放矩形按比例调整字号
    /// @param newRect 新的文字包围盒（局部坐标系）
    void setResizeRect(const QRectF& newRect) override;

    /// @brief 设置文字内容
    /// @param t 文字
    void setText(const QString& t);
    /// @brief 获取文字内容
    QString text() const;

    /// @brief 设置字体
    /// @param f 字体
    void setFont(const QFont& f);
    /// @brief 获取字体
    QFont font() const;

private:
    QString m_text;   ///< 文字内容
    QFont   m_font { "Microsoft YaHei UI", 14 };   ///< 字体（默认微软雅黑 14pt）
};

} // namespace SK
