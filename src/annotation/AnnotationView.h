/**
 * \file AnnotationView.h
 * \brief 标注画布视图
 *
 * 职责：
 *   - 自适应缩放（fit-in）
 *   - 滚轮缩放：Ctrl + 滚轮缩放，普通滚轮垂直滚动
 *   - 支持 Ctrl+Z / Ctrl+Y 转发到 Scene 的 UndoStack
 *   - Delete 键删除选中图元
 */
#pragma once

#include <QGraphicsView>

namespace SK { class AnnotationScene; }

/**
 * @brief 标注画布视图
 *
 * 基于 QGraphicsView，提供缩放、撤销重做快捷键、删除等交互能力。
 * 视图本身不持有图元数据，所有图元由关联的 AnnotationScene 管理。
 */
class AnnotationView : public QGraphicsView
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param scene 关联的标注场景
     * @param parent Qt 父窗口
     */
    explicit AnnotationView(SK::AnnotationScene* scene, QWidget* parent = nullptr);

    /**
     * @brief 自适应缩放至场景尺寸
     */
    void fitToView();

protected:
    /**
     * @brief 滚轮事件：Ctrl+滚轮缩放，否则垂直滚动
     * @param event 滚轮事件
     */
    void wheelEvent(QWheelEvent* event) override;

    /**
     * @brief 按键事件：处理 Undo/Redo/Delete 快捷键
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief 窗口尺寸变化事件
     * @param event 尺寸事件
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    SK::AnnotationScene* m_scene;  ///< 关联的标注场景（不持有所有权）
};
