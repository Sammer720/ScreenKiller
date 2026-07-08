/**
 * \file BaseAnnotationItem.h
 * \brief 标注图元基类
 *
 * 设计说明：
 *   所有标注图形都继承自此基类，统一管理：
 *     - 边框颜色 / 填充颜色 / 线宽
 *     - 选中态视觉
 *     - 序列化接口（用于保存/恢复）
 *
 *   提供 setPen/setBrush 的统一接口，方便属性面板修改。
 */
#pragma once

#include <QGraphicsItem>
#include <QPen>
#include <QBrush>
#include <QColor>

namespace SK {

/// @brief 标注图元类型枚举
enum class AnnotationType {
    Rectangle,     ///< 矩形
    Ellipse,       ///< 椭圆
    Arrow,         ///< 箭头
    Line,          ///< 直线
    Pen,           ///< 自由画笔
    Highlighter,   ///< 荧光笔
    Text           ///< 文本
};

/**
 * @brief 标注图元抽象基类
 *
 * 子类只需实现 boundingRect() 与 paint()，
 * 通过 setPenColor/setPenWidth 等方法调整外观。
 */
class BaseAnnotationItem : public QGraphicsItem
{
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父图元；为 nullptr 时为顶层图元
     */
    explicit BaseAnnotationItem(QGraphicsItem* parent = nullptr)
        : QGraphicsItem(parent) {}

    /// @brief 析构
    virtual ~BaseAnnotationItem() = default;

    /// @brief 设置画笔颜色
    /// @param c 颜色
    void setPenColor(const QColor& c)   { m_pen.setColor(c); update(); }
    /// @brief 设置画笔线宽
    /// @param w 线宽（像素）
    void setPenWidth(qreal w)           { m_pen.setWidthF(w); update(); }
    /// @brief 设置画笔风格
    /// @param s Qt::PenStyle 枚举
    void setPenStyle(Qt::PenStyle s)     { m_pen.setStyle(s); update(); }
    /// @brief 设置画刷颜色
    /// @param c 颜色
    void setBrushColor(const QColor& c) { m_brush.setColor(c); update(); }
    /// @brief 设置画刷风格
    /// @param s Qt::BrushStyle 枚举
    void setBrushStyle(Qt::BrushStyle s){ m_brush.setStyle(s); update(); }

    /// @brief 获取画笔颜色
    QColor    penColor()    const { return m_pen.color();    }
    /// @brief 获取画笔线宽
    qreal     penWidth()    const { return m_pen.widthF();   }
    /// @brief 获取画笔风格
    Qt::PenStyle   penStyle()  const { return m_pen.style();    }
    /// @brief 获取画刷颜色
    QColor    brushColor()  const { return m_brush.color();  }
    /// @brief 获取画刷风格
    Qt::BrushStyle brushStyle() const { return m_brush.style(); }

protected:
    QPen   m_pen   { Qt::red, 2 };   ///< 画笔（默认红色，线宽 2）
    QBrush m_brush { Qt::NoBrush };  ///< 画刷（默认无填充）
};

} // namespace SK

Q_DECLARE_METATYPE(SK::AnnotationType)
