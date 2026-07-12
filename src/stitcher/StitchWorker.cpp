/**
 * \file StitchWorker.cpp
 * \brief 拼接工作线程实现
 */
#include "StitchWorker.h"

#include "ImageStitcher.h"
#include "utils/Logger.h"

#include <QtConcurrent>

StitchWorker::StitchWorker(QObject* parent)
    : QObject(parent)
{
}

StitchWorker::~StitchWorker() = default;

void StitchWorker::cancel()
{
    m_cancelFlag.store(true);
}

void StitchWorker::run(QVector<QImage> frames)
{
    m_cancelFlag.store(false);

    int totalFrames = frames.size();

    // 捕获 this 和 frames（所有权已转移给 lambda）
    QtConcurrent::run([this, frames = std::move(frames), totalFrames]() mutable
    {
        // 单帧直接返回
        if (frames.isEmpty() || m_cancelFlag.load())
        {
            Q_EMIT stitchCancelled();
            return;
        }

        if (frames.size() == 1)
        {
            Q_EMIT stitchFinished(frames.first());
            return;
        }

        // 创建独立拼接器（工作线程独占，不与主线程共享）
        ImageStitcher stitcher;

        SK_LOG_STI() << "工作线程拼接开始，帧数:" << totalFrames;

        // 取消前置检查
        if (m_cancelFlag.load())
        {
            SK_LOG_STI() << "拼接取消（调用前）";
            Q_EMIT stitchCancelled();
            return;
        }

        // 执行拼接（stitchVertical 内部会在每帧对循环顶检查 cancel flag）
        QImage result = stitcher.stitchVertical(frames, &m_cancelFlag);

        // 取消后置检查
        if (m_cancelFlag.load())
        {
            SK_LOG_STI() << "拼接取消（丢弃结果）";
            Q_EMIT stitchCancelled();
        }
        else if (result.isNull())
        {
            SK_LOG_WARN() << "拼接结果为空";
            Q_EMIT stitchCancelled();
        }
        else
        {
            SK_LOG_STI() << "工作线程拼接完成，结果高度:" << result.height();
            Q_EMIT stitchFinished(result);
        }
    });
}
