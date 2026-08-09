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
/// \brief 探测固定区所需最少帧数
constexpr int G_MIN_FRAMES_FOR_DETECT = 3;
/// \brief 行级视为固定的像素一致占比
constexpr double G_FIXED_ROW_MATCH_RATIO = 0.95;
/// \brief 像素一致的灰度差容忍
constexpr int G_FIXED_PIXEL_TOL = 8;
/// \brief 固定区高度上限 = rows / 此值
constexpr int G_FIXED_MAX_RATIO_DIVISOR = 3;
/// \brief 固定区内允许的间断行数（容忍滑块等微小变化，典型滑块高 5px）
constexpr int G_FIXED_GAP_TOLERANCE = 5;

} // namespace

ImageStitcher::ImageStitcher() = default;
ImageStitcher::~ImageStitcher() = default;

void ImageStitcher::setStripHeight(int h)
{
    m_stripHeight = h;
}

void ImageStitcher::setMinConfidence(double c)
{
    m_minConfidence = c;
}

void ImageStitcher::setSearchRatio(double r)
{
    m_searchRatio = r;
}

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
    // Format_RGBA8888 是 byte-ordered 格式，内存恒为 [R,G,B,A]（与 CPU 字节序无关）
    QImage src = img.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat mat(src.height(), src.width(), CV_8UC4,
                const_cast<uchar*>(src.constBits()),
                static_cast<size_t>(src.bytesPerLine()));
    // 复制一份独立内存（QImage 析构后 cv::Mat 引用会失效）
    cv::Mat cloned = mat.clone();
    // RGBA→BGRA：交换 R/B 通道以符合 OpenCV 约定
    cv::cvtColor(cloned, cloned, cv::COLOR_RGBA2BGRA);
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
int ImageStitcher::computeOverlap(const QImage& prev, const QImage& next,
                                  double* confidence)
{
    // 向后兼容：转发到固定区感知版本，无固定区时等价旧行为
    return computeOverlap(prev, next, {0, 0}, confidence);
}

int ImageStitcher::computeOverlap(const QImage& prev, const QImage& next,
                                  const FixedRegion& fixed, double* confidence)
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

    int rows = prevGray.rows;
    int topH = fixed.topH;
    int botH = fixed.botH;
    int movingH = rows - topH - botH;

    // Fail-Fast：固定区太大挤没可动内容
    if (movingH <= 0)
    {
        SK_LOG_WARN() << "固定区过大，无可动内容 topH=" << topH << " botH=" << botH;
        return G_INVALID_OVERLAP;
    }

    int stripH = qMin(m_stripHeight, movingH / G_STRIP_DIVISOR);
    // Fail-Fast：strip 过小返回 0
    if (stripH < G_MIN_STRIP_HEIGHT)
    {
        return G_INVALID_OVERLAP;
    }

    // 取上一帧可动区底部 strip 作为模板（避开底部固定区）
    int tmplY = rows - botH - stripH;
    cv::Rect tmplRect(0, tmplY, prevGray.cols, stripH);
    cv::Mat tmpl = prevGray(tmplRect);

    // 在下一帧可动区内搜索（排除顶/底固定区）
    int searchH = qMax(stripH + 1, static_cast<int>(movingH * m_searchRatio));
    searchH = qMin(searchH, movingH);
    cv::Rect searchRect(0, topH, nextGray.cols, searchH);
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
        return G_INVALID_OVERLAP;
    }

    // maxLoc.y 是模板顶部在搜索区域中的 y 坐标（相对于 searchArea）
    // 可动内容重叠行数 = stripH + maxLoc.y
    // 推导：模板在 prev 的全局 y = rows - botH - stripH
    //       匹配到 next 的全局 y = maxLoc.y + topH
    //       overlapMoving = (rows - botH) - (maxLoc.y + topH) ... 不对
    //       正确公式：overlapMoving = stripH + maxLoc.y
    //       验证：无固定区时退化为旧公式 stripH + maxLoc.y ✓
    int overlap = stripH + maxLoc.y;

    // 异常重叠返回 0（overlap 不应超过可动区高度）
    if ((overlap < 0) || (overlap >= movingH))
    {
        SK_LOG_WARN() << "重叠行数异常：" << overlap << " movingH=" << movingH;
        return G_INVALID_OVERLAP;
    }
    return overlap;
}

// -----------------------------------------------------------------------------
// 固定区探测
// -----------------------------------------------------------------------------
FixedRegion ImageStitcher::detectFixedRegion(const QVector<QImage>& frames)
{
    // Fail-Fast：样本不足不探测，退化为无固定区
    if (frames.size() < G_MIN_FRAMES_FOR_DETECT)
    {
        return {};
    }

    // 取前 3 帧转灰度
    cv::Mat gray[3];
    for (int i = 0; i < G_MIN_FRAMES_FOR_DETECT; ++i)
    {
        if (frames[i].isNull())
        {
            return {};
        }
        cv::Mat mat = qImageToMat(frames[i]);
        if (mat.empty())
        {
            return {};
        }
        cv::cvtColor(mat, gray[i], cv::COLOR_BGRA2GRAY);
    }

    // 宽高一致性检查
    for (int i = 1; i < G_MIN_FRAMES_FOR_DETECT; ++i)
    {
        if ((gray[i].cols != gray[0].cols) || (gray[i].rows != gray[0].rows))
        {
            SK_LOG_WARN() << "固定区探测：帧尺寸不一致，跳过";
            return {};
        }
    }

    int rows = gray[0].rows;
    int cols = gray[0].cols;
    int maxFixed = rows / G_FIXED_MAX_RATIO_DIVISOR; // 固定区上限

    /// @brief 判断第 y 行在两帧间是否稳定（≥95% 像素差 ≤8）
    auto rowStable = [&](const cv::Mat& a, const cv::Mat& b, int y) -> bool
    {
        int matchCount = 0;
        const uchar* rowA = a.ptr<uchar>(y);
        const uchar* rowB = b.ptr<uchar>(y);
        for (int x = 0; x < cols; ++x)
        {
            if (std::abs(static_cast<int>(rowA[x]) - static_cast<int>(rowB[x]))
                <= G_FIXED_PIXEL_TOL)
            {
                ++matchCount;
            }
        }
        return (static_cast<double>(matchCount) / cols) >= G_FIXED_ROW_MATCH_RATIO;
    };

    // 探测 topH：从 y=0 往下，两对帧 (0,1) 和 (1,2) 都 stable 才计入
    // 允许固定区内有 ≤G_FIXED_GAP_TOLERANCE 行间断（容忍滑块等微小变化）
    int topH = 0;
    int gapCount = 0;
    for (int y = 0; y < maxFixed; ++y)
    {
        if (rowStable(gray[0], gray[1], y) && rowStable(gray[1], gray[2], y))
        {
            ++topH;
            gapCount = 0;
        }
        else
        {
            ++gapCount;
            if (gapCount > G_FIXED_GAP_TOLERANCE)
            {
                // 回退：去掉间断行（只保留连续稳定行）
                topH -= (gapCount - 1);
                break;
            }
            // 间断行暂计入，若后续恢复稳定则保留
            ++topH;
        }
    }

    // 探测 botH：从 y=rows-1 往上，同样容忍间断
    int botH = 0;
    gapCount = 0;
    for (int y = rows - 1; y >= rows - maxFixed; --y)
    {
        if (rowStable(gray[0], gray[1], y) && rowStable(gray[1], gray[2], y))
        {
            ++botH;
            gapCount = 0;
        }
        else
        {
            ++gapCount;
            if (gapCount > G_FIXED_GAP_TOLERANCE)
            {
                botH -= (gapCount - 1);
                break;
            }
            ++botH;
        }
    }

    SK_LOG_STI() << "固定区探测 topH=" << topH << " botH=" << botH;
    return {topH, botH};
}

// -----------------------------------------------------------------------------
// 垂直拼接
// -----------------------------------------------------------------------------
QImage ImageStitcher::stitchVertical(const QVector<QImage>& frames,
                                      std::atomic<bool>* cancelFlag)
{
    // Fail-Fast：空帧列表返回空图像
    if (frames.isEmpty())
    {
        return {};
    }
    // 取消前置检查
    if (cancelFlag && cancelFlag->load())
    {
        return {};
    }
    // 单帧直接返回
    if (frames.size() == G_SINGLE_FRAME)
    {
        return frames.first();
    }

    // 第一步：将第一帧转 Mat
    cv::Mat base = qImageToMat(frames.first());
    // Fail-Fast：转换失败返回空图像
    if (base.empty())
    {
        return {};
    }

    int width  = base.cols;
    int totalH = base.rows;

    // 第二步：探测固定区
    FixedRegion fx = detectFixedRegion(frames);
    int topH = fx.topH;
    int botH = fx.botH;
    int movingH = totalH - topH - botH;

    SK_LOG_STI() << "stitchVertical 固定区 topH=" << topH << " botH=" << botH
                 << " movingH=" << movingH;

    // Fail-Fast：可动区为零则无法拼接
    if (movingH <= 0)
    {
        SK_LOG_WARN() << "可动区高度 ≤ 0，无法拼接";
        return {};
    }

    // 第三步：构建画布
    // 画布布局 = [顶部固定 topH 行] + [可动内容拼接] + [底部固定 botH 行]
    cv::Mat canvas(totalH * frames.size() * G_CANVAS_MULTIPLIER, width, base.type(),
                   cv::Scalar(0, 0, 0, 0));

    // 写入顶部固定区（从首帧复制，只出现一次）
    int cursorY = 0;
    if (topH > 0)
    {
        cv::Rect topRect(0, 0, width, topH);
        base(topRect).copyTo(canvas(topRect));
        cursorY = topH;
    }

    // 写入首帧可动内容 [topH, totalH - botH)
    cv::Rect firstMovingSrc(0, topH, width, movingH);
    cv::Mat firstMovingDst = canvas(cv::Rect(0, cursorY, width, movingH));
    base(firstMovingSrc).copyTo(firstMovingDst);
    cursorY += movingH;

    // 第四步：逐帧拼接可动内容
    for (int i = 1; i < frames.size(); ++i)
    {
        // 取消检查（帧对粒度）
        if (cancelFlag && cancelFlag->load())
        {
            SK_LOG_STI() << "stitchVertical 取消，已处理" << (i - 1) << "帧对";
            return {};
        }

        const QImage& next = frames[i];
        cv::Mat nextMat = qImageToMat(next);
        // Fail-Fast：无效帧或宽度不一致时跳过
        if (nextMat.empty() || (nextMat.cols != width))
        {
            SK_LOG_WARN() << "第" << i << "帧无效，跳过。";
            continue;
        }

        double conf = 0.0;
        int overlap = computeOverlap(frames[i - 1], next, fx, &conf);
        SK_LOG_INFO() << "帧" << (i - 1) << "->" << i
                      << "重叠:" << overlap << "置信度:" << conf;

        // overlap == 0 表示无有效匹配（黑帧/匹配失败），丢弃该帧
        if (overlap <= G_INVALID_OVERLAP)
        {
            SK_LOG_WARN() << "overlap=0，丢弃帧" << i;
            continue;
        }

        int appendH = movingH - overlap;
        // 无新增内容时跳过
        if (appendH <= 0)
        {
            SK_LOG_WARN() << "appendH<=0，跳过帧" << i;
            continue;
        }

        // 画布不足时扩展
        if (cursorY + appendH > canvas.rows)
        {
            expandCanvas(canvas, width, cursorY, appendH, totalH, base.type());
        }

        // 从 next 帧可动区 [topH + overlap, totalH - botH) 复制新内容
        cv::Rect srcRect(0, topH + overlap, width, appendH);
        cv::Mat dst = canvas(cv::Rect(0, cursorY, width, appendH));
        nextMat(srcRect).copyTo(dst);
        cursorY += appendH;
    }

    // 第五步：追加底部固定区（从最后一帧复制，只出现一次）
    if (botH > 0)
    {
        // 找最后一帧的有效 Mat
        cv::Mat lastMat;
        for (int i = frames.size() - 1; i >= 0; --i)
        {
            lastMat = qImageToMat(frames[i]);
            if (!lastMat.empty())
            {
                break;
            }
        }
        if (!lastMat.empty())
        {
            if (cursorY + botH > canvas.rows)
            {
                expandCanvas(canvas, width, cursorY, botH, totalH, base.type());
            }
            cv::Rect botSrc(0, totalH - botH, width, botH);
            cv::Mat botDst = canvas(cv::Rect(0, cursorY, width, botH));
            lastMat(botSrc).copyTo(botDst);
            cursorY += botH;
        }
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
