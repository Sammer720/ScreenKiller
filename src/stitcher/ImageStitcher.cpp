/**
 * \file ImageStitcher.cpp
 * \brief 基于 OpenCV 的垂直图像拼接器实现
 */
#include "ImageStitcher.h"

#include <QImage>
#include <QDebug>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/features2d.hpp>

#include "utils/Logger.h"

namespace {

/// \brief 4 通道图像的通道数
constexpr int G_CHANNELS_4 = 4;
/// \brief 3 通道图像的通道数
constexpr int G_CHANNELS_3 = 3;
/// \brief 模板最小高度阈值
constexpr int G_MIN_STRIP_HEIGHT = 4;
/// \brief strip 高度的行数除数（取 prev 行数的 1/3 作为上限）
constexpr int G_STRIP_DIVISOR = 3;
/// \brief 画布初始容量倍数（预估总高度的 2 倍）
constexpr int G_CANVAS_MULTIPLIER = 2;
/// \brief 单帧场景标志
constexpr int G_SINGLE_FRAME = 1;
/// \brief 空帧索引标志
constexpr int G_INVALID_OVERLAP = 0;
/// \brief 画布扩展时的额外预留行数
constexpr int G_CANVAS_RESERVE = 0;

} // namespace

ImageStitcher::ImageStitcher() = default;
ImageStitcher::~ImageStitcher() = default;

// -----------------------------------------------------------------------------
// QImage <-> cv::Mat
// -----------------------------------------------------------------------------
cv::Mat ImageStitcher::qImageToMat(const QImage& img)
{
    // Fail-Fast：空图像返回空 Mat
    if (img.isNull())
    {
        return {};
    }
    // 统一转成 Format_RGBA8888
    QImage src = img.convertToFormat(QImage::Format_RGBA8888);
    // Format_RGBA8888 在 little-endian（x86 Windows）上内存字节序为 B,G,R,A，
    // 即 cv::Mat 的 BGRA 格式，无需额外转换，COLOR_BGRA2GRAY 可直接处理。
    cv::Mat mat(src.height(), src.width(), CV_8UC4,
                const_cast<uchar*>(src.constBits()),
                static_cast<size_t>(src.bytesPerLine()));
    // 复制一份独立内存（QImage 析构后 cv::Mat 引用会失效）
    cv::Mat cloned = mat.clone();
    return cloned;
}

QImage ImageStitcher::matToQImage(const cv::Mat& mat)
{
    // Fail-Fast：空 Mat 返回空图像
    if (mat.empty())
    {
        return {};
    }
    cv::Mat rgba;
    // 根据通道数转换
    if (mat.channels() == G_CHANNELS_4)
    {
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
    }
    else if (mat.channels() == G_CHANNELS_3)
    {
        cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGB);
    }
    else
    {
        rgba = mat.clone();
    }
    QImage img(rgba.data, rgba.cols, rgba.rows,
               static_cast<int>(rgba.step),
               (rgba.channels() == G_CHANNELS_4) ? QImage::Format_RGBA8888
                                                  : QImage::Format_RGB888);
    // 深拷贝脱离 cv::Mat 生命周期
    return img.copy();
}

// -----------------------------------------------------------------------------
// 计算重叠行数
// -----------------------------------------------------------------------------
int ImageStitcher::computeOverlap(const QImage& prev, const QImage& next, double* confidence)
{
    // Fail-Fast：任一帧为空返回 0
    if (prev.isNull() || next.isNull())
    {
        return G_INVALID_OVERLAP;
    }
    // Fail-Fast：宽度不一致返回 0
    if (prev.width() != next.width())
    {
        SK_LOG_WARN() << "两帧宽度不一致，无法拼接：" << prev.width() << "vs" << next.width();
        return G_INVALID_OVERLAP;
    }

    cv::Mat prevMat = qImageToMat(prev);
    cv::Mat nextMat = qImageToMat(next);

    // 转 8 位灰度用于匹配
    cv::Mat prevGray, nextGray;
    cv::cvtColor(prevMat, prevGray, cv::COLOR_BGRA2GRAY);
    cv::cvtColor(nextMat, nextGray, cv::COLOR_BGRA2GRAY);

    int stripH = qMin(m_stripHeight, prevGray.rows / G_STRIP_DIVISOR);
    // Fail-Fast：strip 过小返回 0
    if (stripH < G_MIN_STRIP_HEIGHT)
    {
        return G_INVALID_OVERLAP;
    }

    // 取上一帧底部 strip 作为模板
    cv::Rect tmplRect(0, prevGray.rows - stripH, prevGray.cols, stripH);
    cv::Mat tmpl = prevGray(tmplRect);

    // 在下一帧的上半部分搜索
    int searchH = qMax(stripH + 1, static_cast<int>(nextGray.rows * m_searchRatio));
    searchH = qMin(searchH, nextGray.rows);
    cv::Rect searchRect(0, 0, nextGray.cols, searchH);
    cv::Mat searchArea = nextGray(searchRect);

    // 模板匹配
    cv::Mat result;
    cv::matchTemplate(searchArea, tmpl, result, cv::TM_CCOEFF_NORMED);

    // 找最大响应位置
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    if (confidence != nullptr)
    {
        *confidence = maxVal;
    }

    // 置信度过低时返回 0
    if (maxVal < m_minConfidence)
    {
        SK_LOG_WARN() << "模板匹配置信度" << maxVal << "低于阈值" << m_minConfidence << "，跳过重叠检测";
        // 此处可回退到 ORB 特征匹配，骨架版本暂略
        return G_INVALID_OVERLAP;
    }

    // maxLoc.y 是 strip 顶部在下一帧中的 y 坐标
    // 重叠行数 = prevGray.rows - maxLoc.y
    int overlap = prevGray.rows - maxLoc.y;
    // 异常重叠返回 0
    if ((overlap < 0) || (overlap >= prevGray.rows))
    {
        SK_LOG_WARN() << "重叠行数异常：" << overlap;
        return G_INVALID_OVERLAP;
    }
    return overlap;
}

// -----------------------------------------------------------------------------
// 垂直拼接
// -----------------------------------------------------------------------------
QImage ImageStitcher::stitchVertical(const QVector<QImage>& frames)
{
    // Fail-Fast：空帧列表返回空图像
    if (frames.isEmpty())
    {
        return {};
    }
    // 单帧直接返回
    if (frames.size() == G_SINGLE_FRAME)
    {
        return frames.first();
    }

    // 第一步：将第一帧转 Mat 作为画布
    cv::Mat base = qImageToMat(frames.first());
    // Fail-Fast：转换失败返回空图像
    if (base.empty())
    {
        return {};
    }

    int width  = base.cols;
    int totalH = base.rows;

    // 预估总高度上限（所有帧之和的 2 倍），后续裁剪
    cv::Mat canvas(totalH * G_CANVAS_MULTIPLIER, width, base.type(),
                   cv::Scalar(0, 0, 0, 0));
    cv::Mat roi = canvas(cv::Rect(0, 0, width, totalH));
    base.copyTo(roi);
    int cursorY = totalH;

    // 逐帧拼接
    for (int i = 1; i < frames.size(); ++i)
    {
        const QImage& next = frames[i];
        cv::Mat nextMat = qImageToMat(next);
        // Fail-Fast：无效帧或宽度不一致时跳过
        if (nextMat.empty() || (nextMat.cols != width))
        {
            SK_LOG_WARN() << "第" << i << "帧无效，跳过。";
            continue;
        }

        double conf = 0.0;
        int overlap = computeOverlap(frames[i - 1], next, &conf);
        SK_LOG_INFO() << "帧" << (i - 1) << "->" << i
                      << "重叠:" << overlap << "置信度:" << conf;

        int appendH = nextMat.rows - overlap;
        // 无新增内容时跳过
        if (appendH <= G_INVALID_OVERLAP)
        {
            SK_LOG_WARN() << "appendH<=0，跳过帧" << i;
            continue;
        }

        // 画布不足时扩展
        if (cursorY + appendH > canvas.rows)
        {
            expandCanvas(canvas, width, cursorY, appendH, totalH, base.type());
        }

        // 将下一帧的非重叠部分复制到画布
        cv::Rect srcRect(0, overlap, width, appendH);
        cv::Mat dst = canvas(cv::Rect(0, cursorY, width, appendH));
        nextMat(srcRect).copyTo(dst);
        cursorY += appendH;
    }

    // 裁剪到实际高度
    cv::Mat finalMat = canvas(cv::Rect(0, 0, width, cursorY)).clone();
    return matToQImage(finalMat);
}

void ImageStitcher::expandCanvas(cv::Mat& canvas, int width, int cursorY,
                                  int appendH, int totalH, int type)
{
    cv::Mat bigger(canvas.rows + appendH + totalH + G_CANVAS_RESERVE, width,
                   type, cv::Scalar(0, 0, 0, 0));
    cv::Mat sub = bigger(cv::Rect(0, 0, width, cursorY));
    canvas.copyTo(sub);
    canvas = bigger;
}
