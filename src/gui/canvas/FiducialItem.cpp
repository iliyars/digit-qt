#include "FiducialItem.h"

namespace digitqt::gui::canvas {

namespace {
constexpr double kArmLength = 6.0;
}

FiducialItem::FiducialItem(double x, double y, size_t index) : m_index(index) {
  QPainterPath path;
  path.moveTo(x - kArmLength, y);
  path.lineTo(x + kArmLength, y);
  path.moveTo(x, y - kArmLength);
  path.lineTo(x, y + kArmLength);
  setPath(path);

  setZValue(32.0);  // выше seed-точек (30.0)
  QPen pen(QColor(0, 200, 255));
  pen.setCosmetic(true);
  pen.setWidth(2);
  m_basePen = pen;
  setPen(pen);
}

void FiducialItem::setSelectedStyle(bool selected) {
  if (!selected) {
    setPen(m_basePen);
    setZValue(32.0);
    return;
  }
  QPen p(QColor(255, 0, 0));
  p.setCosmetic(true);
  p.setWidth(3);
  setPen(p);
  setZValue(36.0);
}

}  // namespace digitqt::gui::canvas
