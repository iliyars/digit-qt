#pragma once

#include <QGraphicsEllipseItem>

namespace digitqt::gui::canvas {

/// Renders one fringe-tracing seed point as a small circle marker.
class SeedItem : public QGraphicsEllipseItem {
public:
  SeedItem(double x, double y, size_t index);

  size_t seedIndex() const { return m_index; }
  void setSelectedStyle(bool selected);

private:
  size_t m_index;
};

} // namespace digitqt::gui::canvas
