/**
 * \file AnnotationView.h
 * \brief 标注画布视图
 *
 * 职责：
 *   - 自适应缩放（fit-in）
 *   - 滚轮缩放：滚轮以光标为中心缩放，中键拖动平移
 *   - 滚动条隐藏（平移仅靠中键拖动）
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

Q_SIGNALS:
    /// @brief 视图缩放比例变化
    /// @param scale 当前缩放因子（1.0 = 100%）
    void zoomChanged(qreal scale);

protected:
    /**
     * @brief 滚轮事件：以光标为中心缩放视图
     * @param event 滚轮事件
     */
    void wheelEvent(QWheelEvent* event) override;

    /**
     * @brief 鼠标按下事件：中键按下进入平移模式
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标移动事件：平移模式下按位移反向滚动滚动条
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标释放事件：中键释放退出平移模式
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

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
    bool m_panning = false;        ///< 是否处于中键平移状态
    QPoint m_panStart;             ///< 平移开始时的鼠标位置
    int m_panStartHVal = 0;        ///< 平移开始时水平滚动条值
    int m_panStartVVal = 0;        ///< 平移开始时垂直滚动条值
};
