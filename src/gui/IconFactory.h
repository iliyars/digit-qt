#pragma once

#include <QColor>
#include <QIcon>

namespace digitqt::gui::icons {
/**
 * @brief Small, dependency-free icon set for the boundary-editing toolbar.
 *
 * Icons are painted at runtime (no external image assets, no licensing
 * concerns) and deliberately reuse the same color/line-style convention as
 * canvas::BoundaryItem (green solid = external, red dashed = internal), so
 * a toolbar button always looks like the shape it draws.
 */

QIcon cursorIcon();
QIcon shapeIcon(bool ellipse, const QColor &color, Qt::PenStyle penStyle);
} // namespace digitqt::gui::icons
