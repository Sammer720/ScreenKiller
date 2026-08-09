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

/// 画笔/几何线条粗细边界（像素）
constexpr qreal G_MIN_PEN_WIDTH = 1.0;
constexpr qreal G_MAX_PEN_WIDTH = 30.0;
/// 荧光笔粗细边界（像素）
constexpr qreal G_MIN_HIGHLIGHT_WIDTH = 5.0;
constexpr qreal G_MAX_HIGHLIGHT_WIDTH = 40.0;
/// 马赛克涂抹粗细边界（像素）
constexpr qreal G_MIN_MOSAIC_WIDTH = 10.0;
constexpr qreal G_MAX_MOSAIC_WIDTH = 60.0;
/// 文字字号边界（pt）
constexpr qreal G_MIN_FONT_SIZE = 8.0;
constexpr qreal G_MAX_FONT_SIZE = 72.0;
/// 标注工具栏宽度（像素，MainWindow 与 AnnotationToolBar 共享，避免定位/构造宽度不一致）
constexpr int G_ANN_TOOLBAR_WIDTH = 168;
/// 预设常用色板（代码内可扩展：往此列表追加颜色实例即可）
const QVector<QColor> G_COLOR_PALETTE = {
    Qt::red, Qt::yellow, Qt::green, QColor("#3bd862"), QColor("#4A3B6B"), Qt::white
};

} // namespace SK