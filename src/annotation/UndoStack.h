/**
 * \file UndoStack.h
 * \brief 轻量级 Undo/Redo 命令栈
 *
 * 职责：
 *   - 维护一个 ICommand 子类构成的撤销栈与重做栈
 *   - 提供 push / undo / redo / clear 接口
 *   - 当栈状态变化时发出 changed 信号
 *
 * 设计说明：
 *   - Qt 自带 QUndoStack + QUndoCommand，但为了减少耦合与教学清晰，
 *     这里自行实现一份简洁版本；如需更复杂的合并（如画笔点合并），
 *     可后续替换为 QUndoStack。
 *   - 命令以 std::unique_ptr 持有，ICommand 是 move-only 类型；
 *     因此内部使用 std::vector 而非 QStack（QList 在 Qt6 中要求元素可拷贝）。
 */
#pragma once

#include <QObject>
#include <memory>
#include <qtmetamacros.h>
#include <vector>

namespace SK {

/**
 * @brief 撤销命令抽象接口
 *
 * 所有可撤销操作（如添加图元、删除图元）均实现该接口，
 * 由 UndoStack 统一管理生命周期与执行顺序。
 */
class ICommand
{
public:
    virtual ~ICommand() = default;

    /**
     * @brief 执行（或重做）命令
     */
    virtual void redo() = 0;

    /**
     * @brief 撤销命令
     */
    virtual void undo() = 0;

    /**
     * @brief 返回命令的可读描述（用于调试或菜单显示）
     * @return 命令描述字符串，默认为空
     */
    virtual QString description() const
    {
        return {};
    }
};

/**
 * @brief 轻量级 Undo/Redo 命令栈
 *
 * 维护两个 std::vector：
 *   - m_undo：已执行但可撤销的命令
 *   - m_redo：已撤销但可重做的命令
 *
 * 栈容量上限为 m_limit，超过后丢弃最旧的命令。
 */
class UndoStack : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     * @param limit 栈容量上限，默认 100
     */
    explicit UndoStack(QObject* parent = nullptr, int limit = 100);

    /**
     * @brief 压入并执行一条命令
     * @param cmd 命令的所有权通过 unique_ptr 转移给栈
     */
    void push(std::unique_ptr<ICommand> cmd);

    /**
     * @brief 当前是否可撤销
     * @return 撤销栈非空时返回 true
     */
    bool canUndo() const { return !m_undo.empty(); }

    /**
     * @brief 当前是否可重做
     * @return 重做栈非空时返回 true
     */
    bool canRedo() const { return !m_redo.empty(); }

    /**
     * @brief 撤销最近一条命令
     */
    void undo();

    /**
     * @brief 重做最近一条被撤销的命令
     */
    void redo();

    /**
     * @brief 清空撤销栈与重做栈
     */
    void clear();

Q_SIGNALS:
    /**
     * @brief 栈状态变化信号
     * @param canUndo 当前是否可撤销
     * @param canRedo 当前是否可重做
     */
    void changed(bool canUndo, bool canRedo);

private:
    std::vector<std::unique_ptr<ICommand>> m_undo;  ///< 撤销栈（已执行命令）
    std::vector<std::unique_ptr<ICommand>> m_redo;  ///< 重做栈（已撤销命令）
    int m_limit;                                    ///< 栈容量上限
};

} // namespace SK
