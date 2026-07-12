/**
 * \file StitchWorker.h
 * \brief 拼接工作线程，将 stitchVertical 移出主线程避免 UI 冻结
 *
 * 设计要点：
 *   - 通过 QtConcurrent 在工作线程执行拼接
 *   - 每帧对循环顶检查取消标志，帧对粒度退出
 *   - 取消后丢弃所有结果，emit cancelled（不 emit 部分 result）
 *   - 进度/完成/取消均通过信号通知主线程（QueuedConnection）
 */
#pragma once

#include <QImage>
#include <QVector>
#include <QObject>

#include <atomic>

class ImageStitcher;

class StitchWorker : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit StitchWorker(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~StitchWorker() override;

    /**
     * @brief 启动拼接（在工作线程执行）
     * @param frames 帧序列（所有权转移，worker 独占）
     *
     * 调用后 stitchVertical 在工作线程执行，
     * 完成后 emit finished 或 cancelled。
     */
    void run(QVector<QImage> frames);

    /**
     * @brief 请求取消拼接（线程安全）
     *
     * 下一个帧对循环顶部检查此标志并退出。
     * 取消后 emit cancelled()，不保留任何结果。
     */
    void cancel();

Q_SIGNALS:
    /**
     * @brief 拼接进度信号
     * @param frameIndex 当前处理到第几帧对
     * @param total 总帧对数
     */
    void stitchProgress(int frameIndex, int total);

    /**
     * @brief 拼接完成信号
     * @param result 拼接结果长图
     */
    void stitchFinished(const QImage& result);

    /**
     * @brief 拼接取消信号（丢弃所有结果）
     */
    void stitchCancelled();

private:
    std::atomic<bool> m_cancelFlag{false};
};
