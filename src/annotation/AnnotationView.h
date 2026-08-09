/**
 * \file AnnotationView.h
 * \brief 标注画布视图
 *
 * 职责：
 *   - 自适应缩放（fit-in）
 *   - 滚轮缩放：滚轮以光标为中心缩放，中键拖动平移
 *   - 中键点击恢复默认视图（自适应缩放 + 居中）
 *   - 右键点击将当前标注成品图复制到系统剪贴板
 *   - 平移边界：滚动范围按图片尺寸比例扩展，允许图片边缘移出视口但受边界约束
 *   - 滚动条隐藏（平移仅靠中键拖动）
 *   - 支持 Ctrl+Z / Ctrl+Y 转发到 Scene 的 UndoStack
 *   - Delete 键删除选中图元
 */
#pragma once

#include <QGraphicsView>
#include <QPointer>
#include <QElapsedTimer>

class QLineEdit;
class QEvent;

namespace SK { class AnnotationScene; class TextItem; }

/**
 * @brief 标注画布视图
 *
 * 基于 QGraphicsView，提供缩放、中键平移/点击复位、右键复制、
 * 撤销重做快捷键、删除等交互能力。
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

    /**
     * @brief 复位到默认视图：100% 缩放并居中到场景中心
     *
     * 新截图加载后或中键点击（未拖动）时调用，使视图回到最自然的浏览状态。
     * 复位成功后发射 zoomChanged（缩放因子回到 1.0）。
     */
    void resetToDefault();

Q_SIGNALS:
    /// @brief 视图缩放比例变化
    /// @param scale 当前缩放因子（1.0 = 100%）
    void zoomChanged(qreal scale);

    /// @brief 当前标注成品图已成功复制到系统剪贴板
    void imageCopied();

private Q_SLOTS:
    /**
     * @brief 文字编辑请求槽：在点击位置弹出视图内联编辑器
     * @param item 待编辑的空文字图元
     */
    void onTextEditRequested(SK::TextItem* item);

protected:
    /**
     * @brief 滚轮事件：以光标为中心缩放视图
     * @param event 滚轮事件
     */
    void wheelEvent(QWheelEvent* event) override;

    /**
     * @brief 鼠标按下事件：中键按下进入平移模式；右键复制成品图到剪贴板
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标移动事件：平移模式下按位移反向滚动滚动条，并检测是否发生实际拖动
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标释放事件：中键释放退出平移模式；未拖动时视为点击并恢复默认视图
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

    /**
     * @brief 内容滚动事件：基类完成实际滚动后，重新按图片尺寸比例扩展滚动范围，
     *        保证平移边界（允许超出图片边缘）在滚动条刷新后依然生效
     * @param dx 水平滚动增量
     * @param dy 垂直滚动增量
     */
    void scrollContentsBy(int dx, int dy) override;

private:
    /**
     * @brief 将当前标注成品图（背景 + 全部标注图元）复制到系统剪贴板
     */
    void copyImageToClipboard();

    /**
     * @brief 按图片尺寸比例扩展水平/垂直滚动条范围，形成平移边界
     */
    void applyPanMargin();

    /**
     * @brief 关闭文字内联编辑器并按提交/丢弃语义处理图元
     * @param commit true 表示提交（非空文字压入撤销栈），false 表示丢弃图元
     */
    void closeTextEditor(bool commit);

    /**
     * @brief 事件过滤器：捕获文字编辑器的 Esc 键用于取消输入
     * @param watched 被监视对象
     * @param event 事件
     * @return 事件被处理时返回 true
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

    SK::AnnotationScene* m_scene;  ///< 关联的标注场景（不持有所有权）
    bool m_panning = false;        ///< 是否处于中键平移状态
    bool m_panMoved = false;       ///< 平移过程中是否发生了实际拖动（用于区分中键点击）
    QPoint m_panStart;             ///< 平移开始时的鼠标位置
    int m_panStartHVal = 0;        ///< 平移开始时水平滚动条值
    int m_panStartVVal = 0;        ///< 平移开始时垂直滚动条值

    QPointer<QLineEdit> m_textEditor;             ///< 文字内联编辑器（viewport 子控件，不进场景）
    SK::TextItem* m_editingTextItem = nullptr;    ///< 正在编辑的文字图元（生命周期由场景/关闭流程保证）
    bool m_editorClosing = false;                 ///< 编辑器关闭守卫（防 editingFinished 重入）
    QElapsedTimer m_deleteTimer;                  ///< Delete 双击计时器（间隔 ≤400ms 判定双击清空）
};
