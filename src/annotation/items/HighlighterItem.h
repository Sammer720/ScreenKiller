/**
 * \file HighlighterItem.h
 * \brief 荧光笔图元
 *
 * 与画笔类似，但默认半透明（Alpha ~ 100/255）+ 较粗线宽 +
 * Qt::SquareCap + QPainter::CompositionMode_Darken / SourceOver
 * 以模拟真实荧光笔涂抹效果。
 */
#pragma once

#include "PenItem.h"

namespace SK {

/**
 * @brief 荧光笔图元
 *
 * 继承自 PenItem，复用路径数据，重写 paintContent() 实现半透明叠加效果。
 */
class HighlighterItem : public PenItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit HighlighterItem(QGraphicsItem* parent = nullptr);

    /// @brief 绘制（覆盖父类，使用半透明叠加，NVI 内容绘制入口）
    void paintContent(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget = nullptr) override;

    /// @brief 设置透明度
    /// @param a 0..255
    void setAlpha(int a);
    /// @brief 获取透明度
    int  alpha() const;

private:
    int m_alpha = 100;   ///< 透明度（0..255）
};

} // namespace SK
