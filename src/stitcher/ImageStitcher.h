/**
 * \file ImageStitcher.h
 * \brief 基于 OpenCV 的垂直图像拼接器
 *
 * 算法：
 *   1. 将前后两帧 QImage 转 cv::Mat（BGRA）
 *   2. 取上一帧的底部 strip（高 H_strip，例如 60 像素）作为模板
 *   3. 在下一帧的上半部分用 cv::matchTemplate 做 TM_CCOEFF_NORMED 匹配
 *   4. 找到最大响应位置 -> 计算重叠行数 overlap
 *   5. 将下一帧去掉重叠部分后，垂直拼接到结果底部
 *
 * 备选方案（特征点）：
 *   - 当模板匹配置信度过低（< 0.5）时，回退到 ORB 特征点匹配
 *   - 通过匹配点对估计纯垂直平移
 *
 * 注：
 *   - 适用于滚动文本/网页，不适合剧烈变形场景
 *   - 输入帧必须宽度一致
 */
#pragma once

#include <QImage>
#include <QVector>

#include <opencv2/core.hpp>

/**
 * @brief 固定区探测结果
 *
 * 顶部固定区（如表头）和底部固定区（如滚动条）在滚动过程中像素不变，
 * 会干扰模板匹配。detectFixedRegion() 返回两侧固定行数。
 */
struct FixedRegion
{
    int topH = 0;  ///< 顶部固定行数
    int botH = 0;  ///< 底部固定行数
};

/**
 * @brief 基于 OpenCV 的垂直图像拼接器
 *
 * 通过模板匹配计算相邻帧的重叠区域，实现长截图拼接。
 */
class ImageStitcher
{
public:
    /**
     * @brief 构造函数
     */
    ImageStitcher();

    /**
     * @brief 析构函数
     */
    ~ImageStitcher();

    /**
     * @brief 设置模板高度
     * @param h 模板高度（像素）
     */
    void setStripHeight(int h)         { m_stripHeight = h; }

    /**
     * @brief 设置最小置信度阈值
     * @param c 置信度 [0,1]
     */
    void setMinConfidence(double c)    { m_minConfidence = c; }

    /**
     * @brief 设置搜索范围比例
     * @param r 搜索范围 [0,1]，从下一帧顶部起算
     */
    void setSearchRatio(double r)      { m_searchRatio = r; }

    /**
     * @brief 垂直拼接一系列帧
     * @param frames 输入帧（宽度需一致）
     * @return 合并后的长图；失败返回空 QImage
     */
    QImage stitchVertical(const QVector<QImage>& frames);

    /**
     * @brief 计算两帧之间的垂直重叠行数
     * @param prev 上一帧
     * @param next 下一帧
     * @param confidence 输出：匹配置信度 [0,1]
     * @return 重叠行数（0 表示无重叠）
     */
    int computeOverlap(const QImage& prev, const QImage& next, double* confidence = nullptr);

    /**
     * @brief 计算两帧可动内容的重叠行数（固定区感知版）
     *
     * 模板取自可动区底部（避开底部固定区），搜索区域排除顶/底固定区。
     * 返回值是可动内容的重叠行数，用于拼接时计算 appendH。
     *
     * @param prev 上一帧
     * @param next 下一帧
     * @param fixed 顶/底固定区高度
     * @param confidence 输出：匹配置信度 [0,1]
     * @return 可动内容重叠行数（0 表示无有效重叠）
     */
    int computeOverlap(const QImage& prev, const QImage& next,
                       const FixedRegion& fixed, double* confidence = nullptr);

    /**
     * @brief 探测顶部/底部固定不动的行数
     *
     * 对前若干帧做行级交叉比对：若某行在所有相邻帧对中像素高度一致
     * （≥95% 像素差 ≤8），则视为固定行。容忍滚动条滑块等微小变化。
     *
     * @param frames 输入帧序列（至少 3 帧才探测，否则返回 {0,0}）
     * @return 顶部固定行数 topH、底部固定行数 botH
     */
    FixedRegion detectFixedRegion(const QVector<QImage>& frames);

private:
    /**
     * @brief QImage 转 cv::Mat（BGRA 格式，深拷贝）
     * @param img 输入图像
     * @return 转换后的 Mat
     */
    cv::Mat qImageToMat(const QImage& img);

    /**
     * @brief cv::Mat 转 QImage（深拷贝）
     * @param mat 输入 Mat
     * @return 转换后的 QImage
     */
    QImage  matToQImage(const cv::Mat& mat);

    /**
     * @brief 扩展画布容量
     * @param canvas 当前画布（引用，会被替换）
     * @param width 画布宽度
     * @param cursorY 当前写入位置
     * @param appendH 待追加高度
     * @param totalH 单帧高度
     * @param type 画布类型
     */
    void expandCanvas(cv::Mat& canvas, int width, int cursorY,
                      int appendH, int totalH, int type);

private:
    int    m_stripHeight   = 64;    ///< 模板高度
    double m_minConfidence = 0.55; ///< 模板匹配置信度阈值
    double m_searchRatio   = 1.0;  ///< 在下一帧中搜索的范围（0~1，从顶部起算）
};
