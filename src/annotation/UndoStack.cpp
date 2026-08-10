/**
 * \file UndoStack.cpp
 * \brief 轻量级 Undo/Redo 命令栈实现
 */
#include "UndoStack.h"

namespace SK {

ResizeItemCommand::ResizeItemCommand(SK::BaseAnnotationItem* item,
                                     const QRectF& oldRect,
                                     const QRectF& newRect)
    : m_item(item), m_oldRect(oldRect), m_newRect(newRect)
{
}

void ResizeItemCommand::undo()
{
    // Fail-Fast：图元指针为空时直接返回，避免空指针解引用
    if (m_item != nullptr)
    {
        m_item->setResizeRect(m_oldRect);
    }
}

void ResizeItemCommand::redo()
{
    // Fail-Fast：图元指针为空时直接返回，避免空指针解引用
    if (m_item != nullptr)
    {
        m_item->setResizeRect(m_newRect);
    }
}

UndoStack::UndoStack(QObject* parent, int limit)
    : QObject(parent), m_limit(limit)
{
}

void UndoStack::push(std::unique_ptr<ICommand> cmd)
{
    // Fail-Fast：空命令直接丢弃
    if (cmd == nullptr)
    {
        return;
    }
    cmd->redo();
    m_undo.push_back(std::move(cmd));

    // 新动作清空重做栈
    m_redo.clear();

    // 限制栈大小，超出上限丢弃最旧命令
    while (static_cast<int>(m_undo.size()) > m_limit)
    {
        m_undo.erase(m_undo.begin());
    }

    Q_EMIT changed(canUndo(), canRedo());
}

void UndoStack::undo()
{
    // 撤销栈为空直接返回
    if (m_undo.empty())
    {
        return;
    }
    auto cmd = std::move(m_undo.back());
    m_undo.pop_back();
    cmd->undo();
    m_redo.push_back(std::move(cmd));

    Q_EMIT changed(canUndo(), canRedo());
}

void UndoStack::redo()
{
    // 重做栈为空直接返回
    if (m_redo.empty())
    {
        return;
    }
    auto cmd = std::move(m_redo.back());
    m_redo.pop_back();
    cmd->redo();
    m_undo.push_back(std::move(cmd));

    Q_EMIT changed(canUndo(), canRedo());
}

void UndoStack::clear()
{
    m_undo.clear();
    m_redo.clear();

    Q_EMIT changed(false, false);
}

} // namespace SK
