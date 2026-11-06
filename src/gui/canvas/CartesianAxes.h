#pragma once

#include <QPainter>
#include "core/ApertureSamples.h"

namespace digitqt::gui::canvas {

// Square of pupil coordinates [-1,1], using the same radius on both axes.
inline QRectF unitApertureRect(const digitqt::core::ApertureGeometry &geometry) {
  return QRectF(geometry.centerX - geometry.radius, geometry.centerY - geometry.radius,
                2.0 * geometry.radius, 2.0 * geometry.radius);
}

// Screen-space direction marker shared by image and scalar-map views.
inline void drawCartesianAxes(QPainter &painter, const QRect &viewport) {
  painter.save();
  painter.resetTransform();
  painter.setRenderHint(QPainter::Antialiasing);
  const QPoint origin(viewport.left() + 26, viewport.bottom() - 26);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(30, 30, 30, 170));
  painter.drawRoundedRect(QRect(origin.x() - 18, origin.y() - 58, 82, 76), 5, 5);
  painter.setPen(QPen(Qt::white, 1.5));
  const QPoint xEnd = origin + QPoint(42, 0);
  const QPoint yEnd = origin + QPoint(0, -42);
  painter.drawLine(origin, xEnd);
  painter.drawLine(origin, yEnd);
  painter.drawLine(xEnd, xEnd + QPoint(-6, -3));
  painter.drawLine(xEnd, xEnd + QPoint(-6, 3));
  painter.drawLine(yEnd, yEnd + QPoint(-3, 6));
  painter.drawLine(yEnd, yEnd + QPoint(3, 6));
  painter.drawText(xEnd + QPoint(5, 4), QStringLiteral("X"));
  painter.drawText(yEnd + QPoint(-4, -5), QStringLiteral("Y"));
  painter.restore();
}

// Normalized pupil coordinates, matching modal fitting and the 3D surface.
// The frame follows the map on screen, while text stays a fixed readable size.
inline void drawCoordinateFrame(QPainter &painter, const QRect &viewport,
                                const QRectF &mapOnScreen) {
  if (mapOnScreen.width() <= 0 || mapOnScreen.height() <= 0)
    return;
  const QRectF frame = mapOnScreen.intersected(QRectF(viewport).adjusted(6, 6, -6, -6));
  if (frame.width() < 100 || frame.height() < 100)
    return;
  painter.save();
  painter.resetTransform();
  painter.setRenderHint(QPainter::Antialiasing);
  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);
  painter.setPen(QPen(QColor(30, 30, 30, 220), 3));
  painter.drawRect(frame);
  painter.setPen(QPen(Qt::white, 1));
  painter.drawRect(frame);
  const auto label = [&](const QRectF &box, const QString &text) {
    painter.fillRect(box, QColor(30, 30, 30, 185));
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, text);
  };
  // Avoid overlapping labels when the map is small.
  const int divisions = mapOnScreen.width() >= 420 && mapOnScreen.height() >= 300 ? 8 : 4;
  for (int i = 0; i <= divisions; ++i) {
    const double t = static_cast<double>(i) / divisions;
    const double x = mapOnScreen.left() + t * mapOnScreen.width();
    const double y = mapOnScreen.bottom() - t * mapOnScreen.height();
    const QString text = QString::number(-1.0 + 2.0 * t, 'g', 3);
    if (x >= frame.left() && x <= frame.right()) {
      painter.drawLine(QPointF(x, frame.bottom()), QPointF(x, frame.bottom() - 5));
      const double left = qBound(frame.left(), x - 18, frame.right() - 36);
      label(QRectF(left, frame.bottom() - 23, 36, 16), text);
    }
    if (y >= frame.top() && y <= frame.bottom()) {
      painter.drawLine(QPointF(frame.left(), y), QPointF(frame.left() + 5, y));
      const double top = qBound(frame.top(), y - 8, frame.bottom() - 16);
      label(QRectF(frame.left() + 7, top, 36, 16), text);
    }
  }
  label(QRectF(frame.right() - 24, frame.bottom() - 43, 20, 16), QStringLiteral("X"));
  label(QRectF(frame.left() + 46, frame.top() + 4, 20, 16), QStringLiteral("Y"));
  painter.restore();
}

}  // namespace digitqt::gui::canvas
