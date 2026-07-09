#include "SeedItem.h"

#include <QBrush>
#include <QPen>

namespace digitqt::gui::canvas {

namespace {
constexpr double kRadius = 4.0;
}

SeedItem::SeedItem(double x, double y, size_t index)
    : QGraphicsEllipseItem(x - kRadius, y - kRadius, kRadius * 2, kRadius * 2),
      m_index(index) {
  setZValue(30.0);
  QPen pen(QColor(255, 200, 0));
  pen.setCosmetic(true);
  pen.setWidth(2);
  setPen(pen);
  setBrush(QColor(255, 200, 0, 120));
}

void SeedItem::setSelectedStyle(bool selected) {
  QPen p = pen();
  p.setWidth(selected ? 3 : 2);
  setPen(p);
  setZValue(selected ? 35.0 : 30.0);
}

}  // namespace digitqt::gui::canvas
