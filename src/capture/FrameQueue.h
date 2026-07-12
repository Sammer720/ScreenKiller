/**
 * \file FrameQueue.h
 * \brief 线程安全帧队列，用于主线程（抓帧）与工作线程（处理帧）之间通信
 *
 * 设计要点：
 *   - 主线程 grabWindow 后 enqueue（非阻塞，满则丢弃最旧帧）
 *   - 工作线程 dequeue（阻塞等待，可超时）
 *   - 析构前调用 stop() 唤醒阻塞的 dequeue 使工作线程退出
 *   - 队列上限防止内存无界增长
 */
#pragma once

#include <QImage>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

class FrameQueue
{
public:
    /**
     * @brief 构造函数
     * @param maxSize 队列最大深度（防止内存无界增长，默认 5）
     */
    explicit FrameQueue(int maxSize = 5);

    /**
     * @brief 入队（非阻塞，满则丢弃最旧帧）
     * @param frame 待入队的帧（必须是 .copy() 深拷贝，脱离隐式共享）
     */
    void enqueue(QImage frame);

    /**
     * @brief 出队（阻塞等待，可超时）
     * @param out 输出帧
     * @param timeoutMs 超时毫秒数（0 = 无限等待）
     * @return true 成功取出帧；false 超时或已停止
     */
    bool dequeue(QImage& out, int timeoutMs = 0);

    /**
     * @brief 等待队列排空（阻塞）
     *
     * 用于 Flushing 状态：主线程等工作线程处理完所有帧。
     * 必须先调用 stop() 停止入队，再调用此函数。
     */
    void waitDrained();

    /**
     * @brief 停止队列（唤醒所有阻塞的 dequeue / waitDrained）
     *
     * 调用后 dequeue 不再阻塞，返回 false。
     * 析构前必须调用，否则工作线程可能永久阻塞。
     */
    void stop();

    /**
     * @brief 清空队列
     */
    void clear();

    /**
     * @brief 当前队列深度
     */
    int size() const;

    /**
     * @brief 队列是否已停止
     */
    bool isStopped() const;

private:
    std::queue<QImage> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cvNotEmpty;
    std::condition_variable m_cvDrained;
    int m_maxSize;
    std::atomic<bool> m_stopped{false};
};
