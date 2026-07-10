#include "PhaseMapView.h"

#include "core/Measurement.h"

#include <QPainter>
#include <QPixmap>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>


namespace digitqt::gui::canvas {

namespace {

/// Диапазон известных значений карты фазы (NaN игнорируются).
bool computeRange(const digitqt::core::PhaseMap &map, double &outMin, double &outMax) {
  outMin = std::numeric_limits<double>::max();
  outMax = std::numeric_limits<double>::lowest();
  bool any = false;
  for (int y = 0; y < map.height(); ++y) {
    for (int x = 0; x < map.width(); ++x) {
      if (!map.hasValue(x, y))
        continue;
      const double v = map.value(x, y);
      outMin = std::min(outMin, v);
      outMax = std::max(outMax, v);
      any = true;
    }
  }
  return any;
}

}  // namespace

PhaseMapView::PhaseMapView(QWidget *parent) : QGraphicsView(parent) {
  setScene(&m_scene);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::NoDrag);
}

void PhaseMapView::setMeasurement(digitqt::core::Measurement *measurement) {
  m_measurement = measurement;

  if (!m_backgroundItem) {
    m_backgroundItem = m_scene.addPixmap(QPixmap());
    m_backgroundItem->setZValue(0.0);
  }

  if (measurement && measurement->hasImage()) {
    m_backgroundItem->setPixmap(QPixmap::fromImage(measurement->image()));
    m_scene.setSceneRect(m_backgroundItem->boundingRect());
    fitInView(m_backgroundItem, Qt::KeepAspectRatio);
  }

  refresh();
}

void PhaseMapView::refresh() {
  rebuildHeatmap();
  rebuildIsolines();
}

void PhaseMapView::setHeatmapVisible(bool visible) {
  m_heatmapVisible = visible;
  if (m_heatmapItem)
    m_heatmapItem->setVisible(visible && m_measurement && !m_measurement->phaseMap().isEmpty());
}

void PhaseMapView::setIsolinesVisible(bool visible) {
  m_isolinesVisible = visible;
  if (m_isolinesItem)
    m_isolinesItem->setVisible(visible && m_measurement && !m_measurement->phaseMap().isEmpty());
}

void PhaseMapView::setIsolineStep(double step) {
  m_isolineStep = step;
  rebuildIsolines();
}

void PhaseMapView::wheelEvent(QWheelEvent *event) {
  const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
  scale(factor, factor);
}

void PhaseMapView::rebuildHeatmap() {
  if (!m_heatmapItem) {
    m_heatmapItem = m_scene.addPixmap(QPixmap());
    m_heatmapItem->setZValue(10.0);
    m_heatmapItem->setOpacity(0.65);
    m_heatmapItem->setTransformationMode(Qt::SmoothTransformation);
  }

  if (!m_measurement || m_measurement->phaseMap().isEmpty()) {
    m_heatmapItem->hide();
    return;
  }

  const auto &map = m_measurement->phaseMap();
  double minV = 0.0, maxV = 0.0;
  if (!computeRange(map, minV, maxV)) {
    m_heatmapItem->hide();
    return;
  }
  const double range = (maxV > minV) ? (maxV - minV) : 1.0;

  const int w = map.width();
  const int h = map.height();
  QImage img(w, h, QImage::Format_ARGB32);
  img.fill(Qt::transparent);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!map.hasValue(x, y))
        continue;
      const double t = (map.value(x, y) - minV) / range;  // 0..1
      // 240° (синий, минимум) -> 0° (красный, максимум) -- классическая
      // "радужная" карта, простая и наглядная для первой версии.
      const int hue = static_cast<int>(std::clamp((1.0 - t) * 240.0, 0.0, 240.0));
      img.setPixelColor(x, y, QColor::fromHsv(hue, 255, 255));
    }
  }

  m_heatmapItem->setPixmap(QPixmap::fromImage(img));
  if (m_measurement->hasImage() && w > 0 && h > 0) {
    const double sx = static_cast<double>(m_measurement->image().width()) / w;
    const double sy = static_cast<double>(m_measurement->image().height()) / h;
    m_heatmapItem->setTransform(QTransform::fromScale(sx, sy));
  }
  m_heatmapItem->setVisible(m_heatmapVisible);
}

void PhaseMapView::rebuildIsolines() {
  if (!m_isolinesItem) {
    m_isolinesItem = new QGraphicsPathItem();
    QPen pen(Qt::white);
    pen.setCosmetic(true);
    pen.setWidth(1);
    m_isolinesItem->setPen(pen);
    m_isolinesItem->setZValue(11.0);
    m_scene.addItem(m_isolinesItem);
  }

  if (!m_measurement || m_measurement->phaseMap().isEmpty() || m_isolineStep <= 0.0) {
    m_isolinesItem->hide();
    return;
  }

  const auto &map = m_measurement->phaseMap();
  double minV = 0.0, maxV = 0.0;
  if (!computeRange(map, minV, maxV)) {
    m_isolinesItem->hide();
    return;
  }

  const int w = map.width();
  const int h = map.height();
  const double sx = (m_measurement->hasImage() && w > 0)
                        ? static_cast<double>(m_measurement->image().width()) / w
                        : 1.0;
  const double sy = (m_measurement->hasImage() && h > 0)
                        ? static_cast<double>(m_measurement->image().height()) / h
                        : 1.0;

  auto interp = [](double v0, double v1, double level, double p0, double p1) -> double {
    if (std::fabs(v1 - v0) < 1e-12)
      return p0;
    const double t = (level - v0) / (v1 - v0);
    return p0 + t * (p1 - p0);
  };

  QPainterPath path;
  auto addSeg = [&](QPointF a, QPointF b) {
    path.moveTo(a.x() * sx, a.y() * sy);
    path.lineTo(b.x() * sx, b.y() * sy);
  };

  const int levelStart = static_cast<int>(std::floor(minV / m_isolineStep));
  const int levelEnd = static_cast<int>(std::ceil(maxV / m_isolineStep));

  // Классический marching squares по сетке карты фазы. Седловые случаи
  // (5 и 10) разрешаются упрощённо (без проверки среднего значения в
  // центре ячейки) -- для гладких после решения Лапласа карт этого
  // достаточно для первой версии.
  for (int levelIdx = levelStart; levelIdx <= levelEnd; ++levelIdx) {
    const double level = levelIdx * m_isolineStep;

    for (int y = 0; y + 1 < h; ++y) {
      for (int x = 0; x + 1 < w; ++x) {
        if (!map.hasValue(x, y) || !map.hasValue(x + 1, y) || !map.hasValue(x, y + 1) ||
            !map.hasValue(x + 1, y + 1))
          continue;

        const double v00 = map.value(x, y);
        const double v10 = map.value(x + 1, y);
        const double v01 = map.value(x, y + 1);
        const double v11 = map.value(x + 1, y + 1);

        int caseIdx = 0;
        if (v00 > level)
          caseIdx |= 1;
        if (v10 > level)
          caseIdx |= 2;
        if (v11 > level)
          caseIdx |= 4;
        if (v01 > level)
          caseIdx |= 8;

        if (caseIdx == 0 || caseIdx == 15)
          continue;  // вся ячейка по одну сторону уровня

        const QPointF top(interp(v00, v10, level, x, x + 1), y);
        const QPointF right(x + 1, interp(v10, v11, level, y, y + 1));
        const QPointF bottom(interp(v01, v11, level, x, x + 1), y + 1);
        const QPointF left(x, interp(v00, v01, level, y, y + 1));

        switch (caseIdx) {
          case 1:
          case 14:
            addSeg(left, top);
            break;
          case 2:
          case 13:
            addSeg(top, right);
            break;
          case 3:
          case 12:
            addSeg(left, right);
            break;
          case 4:
          case 11:
            addSeg(right, bottom);
            break;
          case 6:
          case 9:
            addSeg(top, bottom);
            break;
          case 7:
          case 8:
            addSeg(left, bottom);
            break;
          case 5:
            addSeg(left, top);
            addSeg(right, bottom);
            break;
          case 10:
            addSeg(top, right);
            addSeg(left, bottom);
            break;
          default:
            break;
        }
      }
    }
  }

  m_isolinesItem->setPath(path);
  m_isolinesItem->setVisible(m_isolinesVisible);
}

}  // namespace digitqt::gui::canvas
