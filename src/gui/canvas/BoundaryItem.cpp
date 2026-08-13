#include "BoundaryItem.h"

#include <QPainterPath>
#include <QPen>

namespace digitqt::gui::canvas {

namespace {
constexpr double kContourStep = 2.0;  // pixels between generated contour points
}

BoundaryItem::BoundaryItem(const aperture::Shape *shape, size_t index)
    : m_shape(shape), m_index(index) {
  setBrush(Qt::NoBrush);
  rebuildPath();
  applyBaseStyle();
}

void BoundaryItem::rebuildPath() {
  const auto points = m_shape->getContour(kContourStep);
  QPainterPath path;
  if (!points.empty()) {
    path.moveTo(points.front().x, points.front().y);
    for (size_t i = 1; i < points.size(); ++i)
      path.lineTo(points[i].x, points[i].y);
    path.closeSubpath();
  }
  setPath(path);
}

void BoundaryItem::applyBaseStyle() {
  QPen pen;
  pen.setCosmetic(true);  // constant on-screen width regardless of view zoom
  pen.setWidth(2);

  switch (m_shape->getTypeLimits()) {
    case aperture::TypeLimits::EXTERNAL:
      pen.setColor(QColor(0, 190, 60));
      pen.setStyle(Qt::SolidLine);
      m_baseZ = 10.0;
      break;
    case aperture::TypeLimits::INTERNAL:
      pen.setColor(QColor(220, 40, 40));
      pen.setStyle(Qt::DashLine);
      m_baseZ = 11.0;
      break;
    case aperture::TypeLimits::APERTURE:
      pen.setColor(QColor(40, 120, 220));
      pen.setStyle(Qt::DotLine);
      m_baseZ = 12.0;
      break;
  }
  m_basePen = pen;
  setPen(pen);
  setZValue(m_baseZ);
}

void BoundaryItem::setSelectedStyle(bool selected) {
  if (!selected) {
    setPen(m_basePen);
    setZValue(m_baseZ);
    return;
  }
  // A clearly different color (not just a thicker line) so the selected
  // boundary is unmistakable even against an internal boundary, whose
  // un-selected color is already a dark red.
  QPen p(QColor(255, 0, 0));
  p.setCosmetic(true);
  p.setWidth(3);
  p.setStyle(Qt::SolidLine);
  setPen(p);
  setZValue(m_baseZ + 5.0);
}

}  // namespace digitqt::gui::canvas
