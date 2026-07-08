/**
 * \file ImageUtils.cpp
 * \brief QImage 与 cv::Mat 互转实现
 */
#include "ImageUtils.h"

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

namespace SK {
namespace ImageUtils {

/**
 * @brief 内部常量：QImage 转 Mat 时统一使用的中间格式通道数
 */
namespace {
constexpr int G_RGBA_CHANNELS = 4;  ///< RGBA 四通道
constexpr int G_RGB_CHANNELS  = 3;  ///< RGB 三通道
}

cv::Mat qImageToMat(const QImage& img)
{
    // Fail-Fast：空图像直接返回空 Mat
    if (img.isNull())
    {
        return {};
    }

    // 统一转为 RGBA8888，保证通道顺序一致
    QImage src = img.convertToFormat(QImage::Format_RGBA8888);

    // 借用 QImage 内存构造 Mat（不拷贝），随后 clone 出独立内存
    cv::Mat mat(src.height(), src.width(), CV_8UC4,
                const_cast<uchar*>(src.constBits()),
                static_cast<size_t>(src.bytesPerLine()));
    cv::Mat cloned = mat.clone();

    // RGBA -> BGRA，符合 OpenCV 默认通道顺序
    cv::cvtColor(cloned, cloned, cv::COLOR_RGBA2BGRA);
    return cloned;
}

QImage matToQImage(const cv::Mat& mat)
{
    // Fail-Fast：空 Mat 直接返回空 QImage
    if (mat.empty())
    {
        return {};
    }

    cv::Mat rgba;
    if (mat.channels() == G_RGBA_CHANNELS)
    {
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
    }
    else if (mat.channels() == G_RGB_CHANNELS)
    {
        cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGB);
    }
    else
    {
        rgba = mat.clone();
    }

    QImage::Format fmt = (rgba.channels() == G_RGBA_CHANNELS)
                         ? QImage::Format_RGBA8888
                         : QImage::Format_RGB888;

    QImage img(rgba.data, rgba.cols, rgba.rows,
               static_cast<int>(rgba.step), fmt);
    return img.copy();   // 深拷贝，脱离 cv::Mat 内存
}

} // namespace ImageUtils
} // namespace SK
