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
  m_basePen = pen;
  m_baseBrush = QBrush(QColor(255, 200, 0, 120));
  setPen(pen);
  setBrush(m_baseBrush);
}

void SeedItem::setSelectedStyle(bool selected) {
  if (!selected) {
    setPen(m_basePen);
    setBrush(m_baseBrush);
    setZValue(30.0);
    return;
  }
  QPen p(QColor(255, 0, 0));
  p.setCosmetic(true);
  p.setWidth(3);
  setPen(p);
  setBrush(QColor(255, 0, 0, 150));
  setZValue(35.0);
}

}  // namespace digitqt::gui::canvas
