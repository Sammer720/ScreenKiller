#pragma once

/// @file AnnotationConstants.h
/// @brief 标注模块的属性边界常量与预设色板
///
/// 汇集标注工具（画笔、荧光笔、马赛克、文字）的粗细/字号边界，
/// 以及代码内可扩展的常用色板，供 AnnotationScene / AnnotationToolBar 引用。

#include <QtGlobal>
#include <QColor>
#include <QVector>

namespace SK {

/// 水笔线条粗细边界（像素）
constexpr qreal G_MIN_PEN_WIDTH = 2.0;
constexpr qreal G_MAX_PEN_WIDTH = 20.0;
/// 荧光笔粗细边界（像素）
constexpr qreal G_MIN_HIGHLIGHT_WIDTH = 15.0;
constexpr qreal G_MAX_HIGHLIGHT_WIDTH = 45.0;
/// 马赛克涂抹粗细边界（像素）
constexpr qreal G_MIN_MOSAIC_WIDTH = 15.0;
constexpr qreal G_MAX_MOSAIC_WIDTH = 65.0;
/// 文字字号边界（pt）
constexpr qreal G_MIN_FONT_SIZE = 12.0;
constexpr qreal G_MAX_FONT_SIZE = 81.0;
/// 几何图形（直线/箭头/方框/圆）线条粗细边界（像素）
constexpr qreal G_MIN_SHAPE_WIDTH = 2.0;
constexpr qreal G_MAX_SHAPE_WIDTH = 15.0;
/// 全局线宽边界（像素）：各工具滑块边界不同（水笔 2~20 / 荧光笔 15~45 / 马赛克 15~65），
/// 场景 setPenWidth 的防御性 clamp 用最宽边界，避免高上限工具（荧光笔/马赛克）被截断
constexpr qreal G_MIN_ABS_WIDTH = 2.0;
constexpr qreal G_MAX_ABS_WIDTH = 65.0;
/// 标注工具栏宽度（像素，MainWindow 与 AnnotationToolBar 共享，避免定位/构造宽度不一致）
/// v4 仅 5 个 36px 图标按钮竖列（水笔/荧光笔/几何/文字/马赛克），
/// 宽度 = 按钮 36 + 左右内边距 8×2 = 52（168 为 v3 手风琴遗留，已收窄）
constexpr int G_ANN_TOOLBAR_WIDTH = 52;
/// 标注颜色色板（水笔 / 文字 / 几何图案共用；代码内可扩展）
const QVector<QColor> G_ANNOTATION_COLOR_PALETTE = {
    QColor("#D32F2F"), QColor("#000000"), QColor("#388E3C"), QColor("#FF9800"), QColor("#1976D2"), QColor("#FFFFFF"),
    QColor("#424242"), QColor("#E64A19"), QColor("#689F38"), QColor("#FBC02D"), QColor("#0097A7"), QColor("#512DA8"),
    QColor("#BDBDBD"), QColor("#EC407A"), QColor("#AFB42B"), QColor("#FFD54F"), QColor("#42A5F5"), QColor("#7b6ba5"),
    QColor("#E0E0E0"), QColor("#F06292"), QColor("#66BB6A"), QColor("#FFE4B5"), QColor("#4FC3F7"), QColor("#e4d9f0")
};
/// 荧光笔颜色色板（半透明高亮色系，与标注色板独立）
const QVector<QColor> G_HIGHLIGHTER_COLOR_PALETTE = {
    QColor("#FFD400"), QColor("#8BC34A"), QColor("#9575CD"),
    QColor("#81D4FA"), QColor("#FFCC80"), QColor("#F48FB1")
};

} // namespace SK