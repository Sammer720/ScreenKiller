/**
 * \file AnnotationScene.cpp
 * \brief 标注场景实现
 */
#include "AnnotationScene.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QFont>
#include <QtGlobal>
#include <QLineF>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QVector>

#include "items/ArrowItem.h"
#include "items/BaseAnnotationItem.h"
#include "items/EllipseItem.h"
#include "items/HighlighterItem.h"
#include "items/MosaicItem.h"
#include "items/PenItem.h"
#include "items/RectangleItem.h"
#include "items/TextItem.h"

namespace {

/// \brief 背景图元 Z 值（置于所有标注图元之下）
constexpr qreal G_BG_ZVALUE = -1000.0;
/// \brief 撤销栈容量上限
constexpr int G_UNDO_LIMIT = 200;
/// \brief 图元最小有效尺寸（宽度或高度阈值）
constexpr qreal G_MIN_VALID_SIZE = 1.0;
/// \brief 导出图像白色背景的 RGB 分量
const int G_EXPORT_BG_WHITE = 255;

/**
 * @brief 添加图元命令
 *
 * redo 时将图元加入场景，undo 时从场景移除。
 * 图元所有权在场景与命令之间转移。
 */
class AddItemCommand : public SK::ICommand
{
public:
    /**
     * @brief 构造函数
     * @param scene 目标场景
     * @param item 待添加的图元
     */
    AddItemCommand(QGraphicsScene* scene, SK::BaseAnnotationItem* item)
        : m_scene(scene), m_item(item) {}

    /// @brief 执行：将图元加入场景
    void redo() override { m_scene->addItem(m_item); }

    /// @brief 撤销：将图元从场景移除
    void undo() override { m_scene->removeItem(m_item); }

    /// @brief 命令描述
    /// @return 描述字符串
    QString description() const override { return QStringLiteral("AddItem"); }

    /// @brief 析构：命令被撤销栈淘汰或场景销毁时释放图元所有权
    ///
    /// 图元所有权在场景与命令之间转移：命令最后一次执行（redo 后或从未撤销）
    /// 时图元仍在场景中，需先 removeItem 再由本命令释放；
    /// 若最后一次执行的是 undo，图元已被移出场景，直接释放即可。
    ~AddItemCommand() override
    {
        // 图元仍被场景持有（未撤销或重做后）时先移除，再释放内存
        if ((m_item != nullptr) && (m_item->scene() != nullptr))
        {
            m_scene->removeItem(m_item);
        }
        delete m_item;
    }

private:
    QGraphicsScene* m_scene;                 ///< 目标场景
    SK::BaseAnnotationItem* m_item;           ///< 待添加图元（所有权在 scene 与本命令间转移）
};

/**
 * @brief 移除图元命令
 *
 * redo 时将图元从场景移除，undo 时重新加入。
 */
class RemoveItemCommand : public SK::ICommand
{
public:
    /**
     * @brief 构造函数
     * @param scene 目标场景
     * @param item 待移除的图元
     */
    RemoveItemCommand(QGraphicsScene* scene, SK::BaseAnnotationItem* item)
        : m_scene(scene), m_item(item) {}

    /// @brief 执行：将图元从场景移除
    void redo() override { m_scene->removeItem(m_item); }

    /// @brief 撤销：将图元重新加入场景
    void undo() override { m_scene->addItem(m_item); }

private:
    QGraphicsScene* m_scene;                 ///< 目标场景
    SK::BaseAnnotationItem* m_item;          ///< 待移除图元
};

} // namespace

namespace SK {

const QImage& AnnotationScene::backgroundImage() const
{
    return m_bgImage;
}

void AnnotationScene::setTool(Tool t)
{
    m_tool = t;
}

Tool AnnotationScene::tool() const
{
    return m_tool;
}

void AnnotationScene::setPenColor(const QColor& c)
{
    m_penColor = c;
}

void AnnotationScene::setPenWidth(qreal w)
{
    m_penWidth = qBound(G_MIN_PEN_WIDTH, w, G_MAX_PEN_WIDTH);
}

void AnnotationScene::setBrushColor(const QColor& c)
{
    m_brushColor = c;
}

void AnnotationScene::setBrushStyle(Qt::BrushStyle s)
{
    m_brushStyle = s;
}

QColor AnnotationScene::penColor() const
{
    return m_penColor;
}

qreal AnnotationScene::penWidth() const
{
    return m_penWidth;
}

QColor AnnotationScene::brushColor() const
{
    return m_brushColor;
}

Qt::BrushStyle AnnotationScene::brushStyle() const
{
    return m_brushStyle;
}

void AnnotationScene::setFontSize(qreal s)
{
    m_fontSize = qBound(G_MIN_FONT_SIZE, s, G_MAX_FONT_SIZE);
}

void AnnotationScene::setFontFamily(const QString& f)
{
    m_fontFamily = f;
}

qreal AnnotationScene::fontSize() const
{
    return m_fontSize;
}

QString AnnotationScene::fontFamily() const
{
    return m_fontFamily;
}

UndoStack* AnnotationScene::undoStack()
{
    return m_undoStack;
}

AnnotationScene::AnnotationScene(QObject* parent) : QGraphicsScene(parent)
{
    m_undoStack = new UndoStack(this, G_UNDO_LIMIT);
    connect(m_undoStack, &UndoStack::changed, this,
            &AnnotationScene::historyChanged);
}

AnnotationScene::~AnnotationScene()
{
    // 先清空撤销栈：命令析构会在场景图元仍存活时完成 removeItem + delete，
    // 避免 QGraphicsScene 基类析构删除场景图元后，命令再访问悬垂指针
    m_undoStack->clear();
}

void AnnotationScene::loadImage(const QImage& image)
{
    m_bgImage = image;
    // 首次加载时创建背景图元
    if (m_bgItem == nullptr)
    {
        m_bgItem = new QGraphicsPixmapItem();
        m_bgItem->setZValue(G_BG_ZVALUE);
        addItem(m_bgItem);
    }
    m_bgItem->setPixmap(QPixmap::fromImage(image));
    setSceneRect(image.rect());
}

QImage AnnotationScene::exportImage()
{
    // Fail-Fast：背景为空时返回空图像
    if (m_bgImage.isNull())
    {
        return {};
    }
    QSize exportSize = sceneRect().size().toSize();
    QImage outputImage(exportSize, QImage::Format_ARGB32_Premultiplied);
    outputImage.fill(QColor(G_EXPORT_BG_WHITE, G_EXPORT_BG_WHITE, G_EXPORT_BG_WHITE));

    QPainter painter(&outputImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    render(&painter, QRectF(), sceneRect());
    painter.end();
    return outputImage;
}

void AnnotationScene::deleteSelected()
{
    auto selectedItemsList = selectedItems();
    // 没有选中项时直接返回
    if (selectedItemsList.isEmpty())
    {
        return;
    }
    for (QGraphicsItem* item : selectedItemsList)
    {
        auto* annotationItem = dynamic_cast<SK::BaseAnnotationItem*>(item);
        if (annotationItem != nullptr)
        {
            // 标注图元走撤销栈
            m_undoStack->push(std::make_unique<RemoveItemCommand>(this, annotationItem));
        }
        else
        {
            // 非标注图元直接删除
            removeItem(item);
            delete item;
        }
    }
}

// -----------------------------------------------------------------------------
// 鼠标事件 - 工具分发
// -----------------------------------------------------------------------------
void AnnotationScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    // 非左键交给基类处理
    if (event->button() != Qt::LeftButton)
    {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    // 选择模式交给基类处理
    if (m_tool == Tool::Select)
    {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    // 用户开始标注：通知外部（如工具栏收起二三级展开）
    Q_EMIT annotationStarted();

    m_startPos = event->scenePos();
    beginCreateItem(m_tool, m_startPos);
}

void AnnotationScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_currentItem != nullptr)
    {
        updateCreateItem(event->scenePos());
    }
    else
    {
        QGraphicsScene::mouseMoveEvent(event);
    }
}

void AnnotationScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_currentItem != nullptr)
    {
        finalizeCreateItem();
    }
    else
    {
        QGraphicsScene::mouseReleaseEvent(event);
    }
}

// -----------------------------------------------------------------------------
// 创建图元
// -----------------------------------------------------------------------------
void AnnotationScene::beginCreateItem(Tool t, const QPointF& pos)
{
    using namespace SK;
    SK::BaseAnnotationItem* item = nullptr;

    switch (t)
    {
    case Tool::Rectangle:
        item = new RectangleItem();
        break;
    case Tool::Ellipse:
        item = new EllipseItem();
        break;
    case Tool::Arrow:
    case Tool::Line:
    {
        auto* arrowItem = new ArrowItem();
        arrowItem->setDrawArrow(t == Tool::Arrow);
        item = arrowItem;
        break;
    }
    case Tool::Pen:
        item = new PenItem();
        break;
    case Tool::Highlighter:
        item = new HighlighterItem();
        break;
    case Tool::Mosaic:
        item = new MosaicItem();
        break;
    case Tool::Text:
    {
        auto* textItem = new TextItem();
        QFont textFont(m_fontFamily, qRound(m_fontSize));
        textItem->setFont(textFont);
        textItem->setPos(pos);
        addItem(textItem);
        // 不在这里入栈：等视图编辑器提交后由 commitTextItem 入栈
        Q_EMIT textEditRequested(textItem);
        return;
    }
    default:
        return;
    }

    // Fail-Fast：图元创建失败时直接返回
    if (item == nullptr)
    {
        return;
    }
    item->setPenColor(m_penColor);
    item->setPenWidth(m_penWidth);
    item->setBrushColor(m_brushColor);
    item->setBrushStyle(m_brushStyle);
    item->setPos(pos);

    // 立即添加到场景，鼠标移动时持续更新几何
    addItem(item);
    m_currentItem = item;

    // 画笔类工具立即追加第一个点
    if ((t == Tool::Pen) || (t == Tool::Highlighter) || (t == Tool::Mosaic))
    {
        auto* pen = dynamic_cast<PenItem*>(item);
        if (pen != nullptr)
        {
            pen->appendPoint(QPointF(0, 0));
        }
    }
}

void AnnotationScene::updateCreateItem(const QPointF& pos)
{
    // Fail-Fast：没有正在创建的图元时直接返回
    if (m_currentItem == nullptr)
    {
        return;
    }

    using namespace SK;
    QPointF local = pos - m_currentItem->pos();

    // 根据图元类型分发更新逻辑
    if (auto* rectItem = dynamic_cast<RectangleItem*>(m_currentItem))
    {
        rectItem->setRect(QRectF(QPointF(0, 0), local).normalized());
    }
    else if (auto* ellipseItem = dynamic_cast<EllipseItem*>(m_currentItem))
    {
        ellipseItem->setRect(QRectF(QPointF(0, 0), local).normalized());
    }
    else if (auto* arrowItem = dynamic_cast<ArrowItem*>(m_currentItem))
    {
        arrowItem->setLine(QLineF(QPointF(0, 0), local));
    }
    else if (auto* penItem = dynamic_cast<PenItem*>(m_currentItem))
    {
        penItem->appendPoint(local);
    }
}

void AnnotationScene::finalizeCreateItem()
{
    // Fail-Fast：没有正在创建的图元时直接返回
    if (m_currentItem == nullptr)
    {
        return;
    }
    // 将"直接添加"撤销为可撤销的命令：先移除，再通过命令重新加入
    SK::BaseAnnotationItem* item = m_currentItem;
    m_currentItem = nullptr;
    removeItem(item);

    // 仅当图元有非零尺寸时才提交
    QRectF boundingRect = item->boundingRect();
    bool isValid = !boundingRect.isNull()
                   && ((boundingRect.width() > G_MIN_VALID_SIZE)
                       || (boundingRect.height() > G_MIN_VALID_SIZE));
    if (isValid)
    {
        pushAddCommand(item);
    }
    else
    {
        // 尺寸过小直接丢弃
        delete item;
    }
}

void AnnotationScene::pushAddCommand(SK::BaseAnnotationItem* item)
{
    m_undoStack->push(std::make_unique<AddItemCommand>(this, item));
}

void AnnotationScene::commitTextItem(SK::TextItem* item, const QString& text)
{
    if (item == nullptr)
    {
        return;
    }
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        discardTextItem(item);
        return;
    }
    item->setText(trimmed);
    pushAddCommand(item);
}

void AnnotationScene::discardTextItem(SK::TextItem* item)
{
    if (item == nullptr)
    {
        return;
    }
    removeItem(item);
    delete item;
}

} // namespace SK
