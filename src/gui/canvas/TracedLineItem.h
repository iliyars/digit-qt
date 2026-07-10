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
  TracedLineItem(const digitqt::core::tracing::TracedLine &line, size_t index, double order);

  /// Thicker/brighter pen while this line is being edited (see
  /// FringeTracingController's line-edit mode).
  void setEditing(bool editing);

private:
  QPen m_basePen;
  QGraphicsSimpleTextItem *m_orderLabel;
};

}  // namespace digitqt::gui::canvas
