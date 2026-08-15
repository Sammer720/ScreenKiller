/**
 * \file Logger.h
 * \brief 轻量日志宏，封装 qDebug / qInfo / qWarning / qCritical
 *
 * 用法示例：
 *   SK_LOG_INFO() << "message" << value;
 *   SK_LOG_WARN() << "warning";
 *   SK_LOG_ERR()  << "error";
 *
 * 设计说明：
 *   - 借助 Qt 的 QLoggingCategory 机制实现分类日志输出，
 *     可通过 QT_LOGGING_RULES 或代码动态开启/关闭各分类。
 *   - 宏本身不做格式化，仅做流式 << 输出，性能开销最小。
 */
#pragma once

#include <QDebug>
#include <QLoggingCategory>

/// \brief 主日志分类（通用信息）
Q_DECLARE_LOGGING_CATEGORY(skLog)
/// \brief 截屏模块日志分类
Q_DECLARE_LOGGING_CATEGORY(skCapture)
/// \brief 图像拼接模块日志分类
Q_DECLARE_LOGGING_CATEGORY(skStitcher)
/// \brief 标注画布模块日志分类
Q_DECLARE_LOGGING_CATEGORY(skAnnotation)
/// \brief 平台层（WinApi）模块日志分类
Q_DECLARE_LOGGING_CATEGORY(skPlatform)
/// \brief 自动更新模块日志分类
Q_DECLARE_LOGGING_CATEGORY(skUpdater)

/// \brief 输出 Debug 级别主日志
#define SK_LOG_DEBUG()   qCDebug(skLog)
/// \brief 输出 Info 级别主日志
#define SK_LOG_INFO()    qCInfo(skLog)
/// \brief 输出 Warning 级别主日志
#define SK_LOG_WARN()    qCWarning(skLog)
/// \brief 输出 Critical 级别主日志
#define SK_LOG_ERR()     qCCritical(skLog)

/// \brief 输出截屏模块 Debug 日志
#define SK_LOG_CAP()     qCDebug(skCapture)
/// \brief 输出拼接模块 Debug 日志
#define SK_LOG_STI()     qCDebug(skStitcher)
/// \brief 输出标注模块 Debug 日志
#define SK_LOG_ANN()     qCDebug(skAnnotation)
/// \brief 输出平台层 Debug 日志
#define SK_LOG_PLT()     qCDebug(skPlatform)
/// \brief 输出自动更新模块 Debug 日志
#define SK_LOG_UPD()     qCDebug(skUpdater)
