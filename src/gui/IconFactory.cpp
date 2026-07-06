#include "IconFactory.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

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

} // namespace digitqt::gui::icons
