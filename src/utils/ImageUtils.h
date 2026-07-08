/**
 * \file ImageUtils.h
 * \brief QImage 与 cv::Mat 互转工具
 *
 * 设计说明：
 *   ImageStitcher 内部已有私有版本，这里抽出公共版本便于其他模块复用。
 *   所有转换均采用深拷贝，避免源对象析构后悬空指针。
 */
#pragma once

#include <QImage>

#include <opencv2/core.hpp>

namespace SK {
namespace ImageUtils {

/**
 * @brief 将 QImage 转换为 cv::Mat（BGRA 4 通道）
 * @param img 输入的 QImage，可以为任意格式（内部统一转 RGBA8888 再转 BGRA）
 * @return 转换后的 cv::Mat（深拷贝，与原图无共享内存）
 */
cv::Mat qImageToMat(const QImage& img);

/**
 * @brief 将 cv::Mat 转换为 QImage（保留通道数）
 * @param mat 输入的 cv::Mat，支持 1/3/4 通道
 * @return 转换后的 QImage（深拷贝，与原 Mat 无共享内存）
 */
QImage  matToQImage(const cv::Mat& mat);

} // namespace ImageUtils
} // namespace SK
