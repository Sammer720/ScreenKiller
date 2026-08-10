/**
 * \file MosaicItem.h
 * \brief 马赛克涂抹图元
 *
 * 继承自 PenItem，复用其点序列数据（points() / appendPoint()）；
 * paintContent 对背景图像做像素化（马赛克）处理，
 * 涂抹区域为画笔轨迹按线宽外扩后的笔画区域。
 */
#pragma once

#include "PenItem.h"

namespace SK {

/**
 * @brief 马赛克涂抹图元
 *
 * 用户以画笔笔迹划定需要打码的区域，图元绘制时
 * 对背景图在该区域内的像素按固定块大小做平均色采样并回填，
 * 形成马赛克遮掩效果。移动图元时 pos() 变化，绘制期间会
 * 重新采样新位置下方的背景区域，行为符合预期。
 */
class MosaicItem : public PenItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元
     */
    explicit MosaicItem(QGraphicsItem* parent = nullptr);

protected:
    /// @brief 绘制马赛克内容（NVI 内容绘制入口）
    void paintContent(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget = nullptr) override;
};

} // namespace SK