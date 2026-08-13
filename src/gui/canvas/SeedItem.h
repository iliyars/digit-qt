#pragma once

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QPen>

namespace digitqt::gui::canvas {

/// Renders one fringe-tracing seed point as a small circle marker.
class SeedItem : public QGraphicsEllipseItem {
public:
  SeedItem(double x, double y, size_t index);

  size_t seedIndex() const { return m_index; }
  void setSelectedStyle(bool selected);

private:
  size_t m_index;
  QPen m_basePen;      // un-selected pen, restored when selection is cleared
  QBrush m_baseBrush;  // un-selected brush, restored when selection is cleared
};

}  // namespace digitqt::gui::canvas
