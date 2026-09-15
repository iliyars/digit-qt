#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPen>


namespace digitqt::gui::canvas {

/**
 * @brief Renders one traced fringe centerline as a polyline, plus a small
 * text label showing its fringe order number near the start of the line.
 *
 * Different lines get a color cycled from a fixed palette, so multiple
 * traced fringes stay visually distinguishable.
 */
class TracedLineItem : public QGraphicsPathItem {
public:
  /// synthetic: drawn dashed instead of solid (same per-index color) --
  /// see NumberedFringeLine::synthetic. Marks a line that contains
  /// auto-generated (not measured/hand-drawn) points added by the
  /// edge-extrapolation tools.
  TracedLineItem(const digitqt::core::tracing::TracedLine &line, size_t index, double order,
                bool synthetic = false);

  size_t lineIndex() const { return m_index; }

  /// Thicker/brighter pen while this line is being edited (see
  /// FringeTracingController's line-edit mode).
  void setEditing(bool editing);

  /// Highlight pen while this line is selected as a whole (single click,
  /// as opposed to double-click's per-point edit mode) -- Delete removes
  /// it entirely while this style is showing.
  void setSelectedStyle(bool selected);

private:
  size_t m_index;
  QPen m_basePen;
  QGraphicsSimpleTextItem *m_orderLabel;
};

}  // namespace digitqt::gui::canvas
