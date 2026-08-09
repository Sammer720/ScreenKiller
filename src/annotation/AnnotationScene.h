/**
 * \file AnnotationScene.h
 * \brief 标注场景
 *
 * 职责：
 *   - 持有背景长图（QGraphicsPixmapItem）
 *   - 根据当前工具，处理鼠标事件创建对应 Item
 *   - 管理 Undo/Redo
 *   - 提供 setTool / setPenColor / setPenWidth / setBrushColor 等接口
 */
#pragma once

#include <QGraphicsScene>
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QString>

#include "AnnotationConstants.h"
#include "UndoStack.h"

class QGraphicsPixmapItem;

namespace SK {

// 前向声明（与 items/BaseAnnotationItem.h 中的 SK::BaseAnnotationItem 对齐）
class BaseAnnotationItem;
class TextItem;

/**
 * @brief 标注工具类型枚举
 */
enum class Tool
{
    Select,       ///< 选择/移动模式
    Rectangle,    ///< 矩形标注
    Ellipse,      ///< 椭圆标注
    Arrow,        ///< 箭头标注
    Line,         ///< 直线标注
    Pen,          ///< 自由画笔
    Highlighter, ///< 高亮画笔
    Mosaic,       ///< 马赛克涂抹
    Text          ///< 文字标注
};

/**
 * @brief 标注场景
 *
 * 负责管理背景图像与所有标注图元，根据当前工具分发鼠标事件，
 * 创建对应类型的 BaseAnnotationItem，并通过 UndoStack 提供可撤销操作。
 */
class AnnotationScene : public QGraphicsScene
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit AnnotationScene(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     *
     * 先清空撤销栈，让 Add/Remove 命令在 QGraphicsScene 基类析构删除场景图元之前
     * 完成图元释放，避免命令析构访问已删除图元导致的悬垂指针与双重释放。
     */
    ~AnnotationScene() override;

    /**
     * @brief 加载背景图像
     * @param image 背景图像
     */
    void loadImage(const QImage& image);

    /**
     * @brief 获取背景图像（const 引用，避免绘制时复制大图）
     * @return 背景图像引用
     */
    const QImage& backgroundImage() const;

    /**
     * @brief 导出当前画布（背景 + 所有标注）为单张 QImage
     * @return 导出后的图像
     */
    QImage exportImage();

    /**
     * @brief 切换当前工具
     * @param t 工具类型
     */
    void setTool(Tool t);

    /**
     * @brief 获取当前工具
     * @return 工具类型
     */
    Tool  tool() const;

    /**
     * @brief 设置画笔颜色
     * @param c 颜色
     */
    void setPenColor(const QColor& c);

    /**
     * @brief 设置画笔线宽（按边界常量 clamp 到合法范围）
     * @param w 线宽
     */
    void setPenWidth(qreal w);

    /**
     * @brief 设置填充颜色
     * @param c 颜色
     */
    void setBrushColor(const QColor& c);

    /**
     * @brief 设置填充样式
     * @param s 填充样式
     */
    void setBrushStyle(Qt::BrushStyle s);

    /**
     * @brief 获取画笔颜色
     * @return 颜色
     */
    QColor    penColor()    const;

    /**
     * @brief 获取画笔线宽
     * @return 线宽
     */
    qreal     penWidth()    const;

    /**
     * @brief 获取填充颜色
     * @return 颜色
     */
    QColor    brushColor()  const;

    /**
     * @brief 获取填充样式
     * @return 填充样式
     */
    Qt::BrushStyle brushStyle() const;

    /**
     * @brief 设置文字字号（按边界常量 clamp 到合法范围）
     * @param s 字号（pt）
     */
    void setFontSize(qreal s);

    /**
     * @brief 设置文字字体族
     * @param f 字体族名称
     */
    void setFontFamily(const QString& f);

    /**
     * @brief 获取文字字号
     * @return 字号（pt）
     */
    qreal fontSize() const;

    /**
     * @brief 获取文字字体族
     * @return 字体族名称
     */
    QString fontFamily() const;

    /**
     * @brief 获取撤销栈
     * @return 撤销栈指针
     */
    UndoStack* undoStack();

    /**
     * @brief 删除当前选中的图元
     */
    void deleteSelected();

    /**
     * @brief 提交文字图元：非空则设文字并入撤销栈，空则删除图元
     * @param item 待提交的文字图元
     * @param text 输入的原始文字内容
     */
    void commitTextItem(SK::TextItem* item, const QString& text);

    /**
     * @brief 丢弃文字图元（从场景移除并删除）
     * @param item 待丢弃的文字图元
     */
    void discardTextItem(SK::TextItem* item);

Q_SIGNALS:
    /**
     * @brief 历史记录变化信号
     * @param canUndo 当前是否可撤销
     * @param canRedo 当前是否可重做
     */
    void historyChanged(bool canUndo, bool canRedo);

    /**
     * @brief 文字编辑请求信号（文字工具点击后发射，由视图弹出内联编辑器）
     * @param item 待编辑的空文字图元
     */
    void textEditRequested(SK::TextItem* item);

    /// @brief 用户开始在场景上创建标注图元（用于工具栏自动收回展开的参数区）
    void annotationStarted();

protected:
    /**
     * @brief 鼠标按下事件：根据工具开始创建图元
     * @param event 鼠标事件
     */
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

    /**
     * @brief 鼠标移动事件：更新正在创建的图元几何
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

    /**
     * @brief 鼠标释放事件：完成图元创建并提交到撤销栈
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    /**
     * @brief 根据工具类型开始创建对应图元
     * @param t 工具类型
     * @param pos 起始位置（场景坐标）
     */
    void beginCreateItem(Tool t, const QPointF& pos);

    /**
     * @brief 更新正在创建的图元几何
     * @param pos 当前鼠标位置（场景坐标）
     */
    void updateCreateItem(const QPointF& pos);

    /**
     * @brief 完成图元创建，提交到撤销栈或丢弃
     */
    void finalizeCreateItem();

    /**
     * @brief 将图元通过 AddItem 命令压入撤销栈
     * @param item 待添加的图元（所有权转移）
     */
    void pushAddCommand(BaseAnnotationItem* item);

private:
    QGraphicsPixmapItem* m_bgItem    = nullptr;  ///< 背景图像图元
    QImage               m_bgImage;              ///< 背景图像数据

    Tool      m_tool      = Tool::Select;         ///< 当前工具
    QColor    m_penColor  { Qt::red };            ///< 画笔颜色
    qreal     m_penWidth  = 2.0;                  ///< 画笔线宽
    QColor    m_brushColor{ Qt::transparent };    ///< 填充颜色
    Qt::BrushStyle m_brushStyle = Qt::NoBrush;    ///< 填充样式
    qreal     m_fontSize  = 12.0;                 ///< 文字字号（pt）
    QString   m_fontFamily = QStringLiteral("微软雅黑");  ///< 文字字体族

    BaseAnnotationItem* m_currentItem = nullptr;  ///< 正在创建的图元
    QPointF   m_startPos;                          ///< 创建起点

    UndoStack* m_undoStack = nullptr;             ///< 撤销栈
};

} // namespace SK
