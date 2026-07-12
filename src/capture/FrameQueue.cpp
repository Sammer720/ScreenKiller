/**
 * \file FrameQueue.cpp
 * \brief 线程安全帧队列实现
 */
#include "FrameQueue.h"

FrameQueue::FrameQueue(int maxSize)
    : m_maxSize(maxSize)
{
}

void FrameQueue::enqueue(QImage frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 队列已满：丢弃最旧帧
    if (static_cast<int>(m_queue.size()) >= m_maxSize)
    {
        m_queue.pop();
    }

    m_queue.push(std::move(frame));
    m_cvNotEmpty.notify_one();
}

bool FrameQueue::dequeue(QImage& out, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (timeoutMs <= 0)
    {
        // 无限等待
        m_cvNotEmpty.wait(lock, [this]()
        {
            return !m_queue.empty() || m_stopped.load();
        });
    }
    else
    {
        // 超时等待
        if (!m_cvNotEmpty.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this]() { return !m_queue.empty() || m_stopped.load(); }))
        {
            return false; // 超时
        }
    }

    // 已停止且队列为空
    if (m_stopped.load() && m_queue.empty())
    {
        return false;
    }

    // 取出帧
    out = std::move(m_queue.front());
    m_queue.pop();

    // 队列排空时通知 waitDrained
    if (m_queue.empty())
    {
        m_cvDrained.notify_all();
    }

    return true;
}

void FrameQueue::waitDrained()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cvDrained.wait(lock, [this]()
    {
        return m_queue.empty() || m_stopped.load();
    });
}

void FrameQueue::stop()
{
    m_stopped.store(true);
    m_cvNotEmpty.notify_all();
    m_cvDrained.notify_all();
}

void FrameQueue::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<QImage> empty;
    m_queue.swap(empty);
    m_cvDrained.notify_all();
}

int FrameQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_queue.size());
}

bool FrameQueue::isStopped() const
{
    return m_stopped.load();
}
