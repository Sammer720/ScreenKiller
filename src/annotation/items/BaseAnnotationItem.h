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
#include <QRectF>
#include <QPointF>

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

/// @brief 缩放手柄位置枚举
enum class ResizeHandle {
    None,          ///< 无（未在手柄上）
    TopLeft,       ///< 左上角
    TopRight,      ///< 右上角
    BottomLeft,    ///< 左下角
    BottomRight    ///< 右下角
};

/**
 * @brief 标注图元抽象基类
 *
 * 采用 NVI（Non-Virtual Interface）设计：
 *   - 基类的 paint() 为 final，统一负责绘制内容并叠加选中态手柄；
 *   - 子类只需实现 paintContent() / resizeRect() / setResizeRect() 三个纯虚函数；
 *   - 通过 setPenColor/setPenWidth 等方法调整外观。
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

    /// @brief 最终 paint：调用 paintContent() + 选中时绘制手柄
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) final;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    /// @brief 获取图元逻辑几何（不含画笔边距），用于手柄定位与缩放
    virtual QRectF resizeRect() const = 0;

    /// @brief 根据新的缩放矩形更新图元几何
    /// @param newRect 新的逻辑几何（局部坐标系）
    virtual void setResizeRect(const QRectF& newRect) = 0;

protected:
    /// @brief 子类实现：绘制图元内容（原有 paint() 逻辑搬入）
    virtual void paintContent(QPainter* painter,
                              const QStyleOptionGraphicsItem* option,
                              QWidget* widget = nullptr) = 0;

    /// @brief 最小缩放尺寸（像素），子类缩放实现中用于保护
    static constexpr qreal G_MIN_RESIZE_SIZE = 5.0;

    QPen   m_pen   { Qt::red, 2 };   ///< 画笔（默认红色，线宽 2）
    QBrush m_brush { Qt::NoBrush };  ///< 画刷（默认无填充）

private:
    /// @brief 绘制四角缩放手柄
    void drawHandles(QPainter* painter) const;
    /// @brief 命中测试：返回指定位置（item 坐标系）所在的手柄
    ResizeHandle handleAt(const QPointF& pos) const;

    ResizeHandle m_activeHandle = ResizeHandle::None;  ///< 当前拖拽的手柄
    QRectF m_startResizeRect;                          ///< 缩放起始的 resizeRect 快照
    QPointF m_startPos;                                ///< 缩放起始鼠标位置（item 坐标系）
};

} // namespace SK

Q_DECLARE_METATYPE(SK::AnnotationType)
