#pragma once

#include <QGraphicsPathItem>

#include <aperture/include/geometry/Shape.h>

namespace digitqt::gui::canvas {
/**
 * @brief Renders a single aperture::Shape (external/internal/aperture
 * boundary) inside a QGraphicsScene.
 *
 * Pure presentation: it never mutates the underlying Shape. It is rebuilt
 * from scratch whenever ShapeCollection changes (see
 * ImageCanvas::rebuildBoundaryItems) -- simple and correct, since
 * interactive edits typically involve only a handful of boundary shapes.
 *
 * Uses Shape::getContour() so it works uniformly for Ellipse, Rectangle and
 * Polygon without any type-specific rendering code.
 */
class BoundaryItem : public QGraphicsPathItem {
public:
  BoundaryItem(const aperture::Shape *shape, size_t index);

  size_t shapeIndex() const { return m_index; }
  aperture::TypeLimits shapeType() const { return m_shape->getTypeLimits(); }

  void setSelectedStyle(bool selected);

private:
  void rebuildPath();
  void applyBaseStyle();

  const aperture::Shape *m_shape;
  size_t m_index;
  double m_baseZ = 0.0;
};

} // namespace digitqt::gui::canvas
