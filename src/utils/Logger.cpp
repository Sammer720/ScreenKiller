/**
 * \file Logger.cpp
 * \brief 日志分类定义
 *
 * 说明：
 *   Q_DECLARE_LOGGING_CATEGORY 仅声明 category 函数原型，
 *   必须在且仅在一个翻译单元中用 Q_LOGGING_CATEGORY 提供定义，
 *   否则链接器会报"无法解析的外部符号 ?skLog@@..."。
 *
 * 命名约定：
 *   "sk.<module>" 形式，可通过 QT_LOGGING_RULES="sk.capture=true" 开启。
 */
#include "Logger.h"

/// \brief 主日志分类实例（默认开启）
Q_LOGGING_CATEGORY(skLog,        "sk.log")
/// \brief 截屏模块日志分类实例
Q_LOGGING_CATEGORY(skCapture,    "sk.capture")
/// \brief 拼接模块日志分类实例
Q_LOGGING_CATEGORY(skStitcher,   "sk.stitcher")
/// \brief 标注模块日志分类实例
Q_LOGGING_CATEGORY(skAnnotation,  "sk.annotation")
/// \brief 平台层日志分类实例
Q_LOGGING_CATEGORY(skPlatform,    "sk.platform")
/// \brief 自动更新模块日志分类实例
Q_LOGGING_CATEGORY(skUpdater,     "sk.updater")
