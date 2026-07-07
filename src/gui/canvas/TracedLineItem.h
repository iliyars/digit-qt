#pragma once

#include <QGraphicsPathItem>
#include <QPen>

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

namespace digitqt::gui::canvas {

/// Renders one traced fringe centerline as a polyline. Different lines
/// get a color cycled from a fixed palette, so multiple traced fringes
/// stay visually distinguishable.
class TracedLineItem : public QGraphicsPathItem {
public:
  TracedLineItem(const digitqt::core::tracing::TracedLine &line, size_t index);

  /// Thicker/brighter pen while this line is being edited (see
  /// FringeTracingController's line-edit mode).
  void setEditing(bool editing);

private:
  QPen m_basePen;
};

} // namespace digitqt::gui::canvas
