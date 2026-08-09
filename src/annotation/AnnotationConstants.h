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
/// 标注工具栏宽度（像素，MainWindow 与 AnnotationToolBar 共享，避免定位/构造宽度不一致）
/// v4 仅 5 个 36px 图标按钮竖列（水笔/荧光笔/几何/文字/马赛克），
/// 宽度 = 按钮 36 + 左右内边距 8×2 = 52（168 为 v3 手风琴遗留，已收窄）
constexpr int G_ANN_TOOLBAR_WIDTH = 52;
/// 标注颜色色板（水笔 / 文字 / 几何图案共用；代码内可扩展）
const QVector<QColor> G_ANNOTATION_COLOR_PALETTE = {
    Qt::red, Qt::yellow, Qt::green, QColor("#3bd862"), QColor("#4A3B6B"), Qt::white
};
/// 荧光笔颜色色板（半透明高亮色系，与标注色板独立）
const QVector<QColor> G_HIGHLIGHTER_COLOR_PALETTE = {
    QColor("#FFEB3B"), QColor("#FF6EC7"), QColor("#4FC3F7"),
    QColor("#81C784"), QColor("#FFB74D"), QColor("#E1BEE7")
};

} // namespace SK