#pragma once

#include <QGraphicsPathItem>
#include <QPen>

namespace digitqt::gui::canvas {

/// Рисует один репер как маленький крестик -- намеренно отличается от
/// SeedItem (закрашенный кружок), чтобы не путать репер с seed-точкой,
/// когда оба видны одновременно.
class FiducialItem : public QGraphicsPathItem {
public:
  FiducialItem(double x, double y, size_t index);

  size_t fiducialIndex() const { return m_index; }
  void setSelectedStyle(bool selected);

private:
  size_t m_index;
  QPen m_basePen;
};

}  // namespace digitqt::gui::canvas
