#include "IconFactory.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <cmath>

namespace digitqt::gui::icons {

namespace {

constexpr int kSize = 24;

QPixmap newCanvas() {
  QPixmap pixmap(kSize, kSize);
  pixmap.fill(Qt::transparent);
  return pixmap;
}

} // namespace

QIcon cursorIcon() {
  QPixmap pixmap = newCanvas();
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Simple mouse-pointer silhouette.
  QPolygonF arrow;
  arrow << QPointF(5, 3) << QPointF(5, 20) << QPointF(9.5, 15.5)
        << QPointF(12.5, 21.5) << QPointF(15, 20.3) << QPointF(12, 14.3)
        << QPointF(18, 14) << QPointF(5, 3);

  painter.setBrush(QColor(70, 70, 70));
  painter.setPen(QPen(Qt::white, 1));
  painter.drawPolygon(arrow);

  return QIcon(pixmap);
}

QIcon shapeIcon(bool ellipse, const QColor &color, Qt::PenStyle penStyle) {
  QPixmap pixmap = newCanvas();
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  QPen pen(color);
  pen.setWidth(2);
  pen.setStyle(penStyle);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  const QRectF rect(3.5, 3.5, kSize - 7, kSize - 7);
  if (ellipse)
    painter.drawEllipse(rect);
  else
    painter.drawRect(rect);

  return QIcon(pixmap);
}

QIcon pointsEllipseIcon(const QColor &color) {
  QPixmap pixmap = newCanvas();
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  const QRectF rect(3.5, 3.5, kSize - 7, kSize - 7);
  constexpr int kDots = 8;
  constexpr double kTwoPi = 6.283185307179586;

  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  for (int i = 0; i < kDots; ++i) {
    const double angle = kTwoPi * i / kDots;
    const double x = rect.center().x() + (rect.width() / 2.0) * std::cos(angle);
    const double y =
        rect.center().y() + (rect.height() / 2.0) * std::sin(angle);
    painter.drawEllipse(QPointF(x, y), 1.6, 1.6);
  }

  return QIcon(pixmap);
}

QIcon seedIcon() {
  QPixmap pixmap = newCanvas();
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  const QColor seedColor(255, 200, 0);
  painter.setPen(QPen(seedColor, 2));
  painter.setBrush(
      QColor(seedColor.red(), seedColor.green(), seedColor.blue(), 160));
  painter.drawEllipse(QRectF(kSize / 2.0 - 5, kSize / 2.0 - 5, 10, 10));

  return QIcon(pixmap);
}

QIcon autoSeedIcon() {
  QPixmap pixmap = newCanvas();
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  const double midY = kSize / 2.0;
  QPen linePen(QColor(140, 140, 140));
  linePen.setWidth(1);
  painter.setPen(linePen);
  painter.drawLine(QPointF(3.0, midY), QPointF(kSize - 3.0, midY));

  const QColor seedColor(255, 200, 0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(seedColor);
  for (double fx : {5.5, 11.0, 16.5}) // scattered dots along the scan line
    painter.drawEllipse(QPointF(fx, midY), 2.0, 2.0);

  return QIcon(pixmap);
}

} // namespace digitqt::gui::icons
