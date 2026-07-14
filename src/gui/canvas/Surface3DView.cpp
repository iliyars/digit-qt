#include "Surface3DView.h"

#include "core/Measurement.h"
#include "core/PhaseMap.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>


namespace digitqt::gui::canvas {

namespace {
constexpr int kGridTarget = 55;  // максимум ячеек по каждой оси -- ради читаемости и скорости
}

Surface3DView::Surface3DView(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(300);
  setMouseTracking(false);

  m_legend = new ColorLegendWidget(this);
  m_legend->setNoData();
}

void Surface3DView::setMeasurement(digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  refresh();
}

void Surface3DView::refresh() {
  m_map = (m_measurement && !m_measurement->modalAnalysis().isEmpty())
              ? &m_measurement->modalAnalysis().residual
              : nullptr;
  update();
}

void Surface3DView::mousePressEvent(QMouseEvent *event) {
  m_dragging = true;
  m_lastMousePos = event->pos();
}

void Surface3DView::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging)
    return;
  const QPoint delta = event->pos() - m_lastMousePos;
  m_lastMousePos = event->pos();
  m_azimuth += delta.x() * 0.01;
  m_elevation = std::clamp(m_elevation - delta.y() * 0.01, 0.05, 1.5);
  update();
}

void Surface3DView::mouseReleaseEvent(QMouseEvent * /*event*/) {
  m_dragging = false;
}

void Surface3DView::wheelEvent(QWheelEvent *event) {
  m_zoom *= (event->angleDelta().y() > 0) ? 1.1 : 1.0 / 1.1;
  m_zoom = std::clamp(m_zoom, 0.2, 5.0);
  update();
}

void Surface3DView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (m_legend) {
    const QSize size = m_legend->sizeHint();
    m_legend->resize(size);
    m_legend->move(width() - size.width() - 12, 12);
    m_legend->raise();
  }
}

void Surface3DView::paintEvent(QPaintEvent * /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor(35, 35, 38));

  painter.setPen(QColor(180, 180, 180));
  painter.drawText(QRectF(8, height() - 22, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   tr("Перетащите для поворота, колесо — масштаб"));

  if (!m_map || m_map->isEmpty()) {
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, tr("No data"));
    if (m_legend)
      m_legend->setNoData();
    return;
  }

  const int srcW = m_map->width();
  const int srcH = m_map->height();
  const int stepX = std::max(1, srcW / kGridTarget);
  const int stepY = std::max(1, srcH / kGridTarget);

  std::vector<int> xs, ys;
  for (int x = 0; x < srcW; x += stepX)
    xs.push_back(x);
  for (int y = 0; y < srcH; y += stepY)
    ys.push_back(y);

  const int gw = static_cast<int>(xs.size());
  const int gh = static_cast<int>(ys.size());
  if (gw < 2 || gh < 2) {
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, tr("No data"));
    return;
  }

  std::vector<double> grid(static_cast<size_t>(gw) * static_cast<size_t>(gh),
                           std::numeric_limits<double>::quiet_NaN());
  double minV = std::numeric_limits<double>::max();
  double maxV = std::numeric_limits<double>::lowest();
  bool any = false;
  for (int j = 0; j < gh; ++j) {
    for (int i = 0; i < gw; ++i) {
      if (!m_map->hasValue(xs[static_cast<size_t>(i)], ys[static_cast<size_t>(j)]))
        continue;
      const double v = m_map->value(xs[static_cast<size_t>(i)], ys[static_cast<size_t>(j)]);
      grid[static_cast<size_t>(j) * static_cast<size_t>(gw) + static_cast<size_t>(i)] = v;
      minV = std::min(minV, v);
      maxV = std::max(maxV, v);
      any = true;
    }
  }
  if (!any) {
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, tr("No data"));
    if (m_legend)
      m_legend->setNoData();
    return;
  }
  const double range = (maxV > minV) ? (maxV - minV) : 1.0;

  if (m_legend)
    m_legend->setRange(minV, maxV, m_unitSuffix);

  // Пространственные координаты в [-1, 1]; высота нормализована в ту же
  // шкалу с преувеличением (реальные отклонения на порядки меньше
  // пространственного размера апертуры -- без этого рельеф выглядел бы
  // плоским).
  constexpr double kHeightExaggeration = 0.6;
  auto normX = [&](int i) { return (gw > 1) ? (2.0 * i / (gw - 1) - 1.0) : 0.0; };
  auto normY = [&](int j) { return (gh > 1) ? (2.0 * j / (gh - 1) - 1.0) : 0.0; };
  auto normH = [&](double v) { return ((v - minV) / range - 0.5) * 2.0 * kHeightExaggeration; };

  const double cosA = std::cos(m_azimuth), sinA = std::sin(m_azimuth);
  const double cosE = std::cos(m_elevation), sinE = std::sin(m_elevation);

  struct Proj {
    double sx = 0.0, sy = 0.0, depth = 0.0;
    bool valid = false;
  };
  std::vector<Proj> proj(static_cast<size_t>(gw) * static_cast<size_t>(gh));

  const double scale = std::min(width(), height()) * 0.35 * m_zoom;
  const QPointF center(width() / 2.0, height() / 2.0 + height() * 0.05);

  for (int j = 0; j < gh; ++j) {
    for (int i = 0; i < gw; ++i) {
      const size_t idx = static_cast<size_t>(j) * static_cast<size_t>(gw) + static_cast<size_t>(i);
      const double v = grid[idx];
      if (v != v) {
        proj[idx] = Proj{};
        continue;
      }
      const double x = normX(i), y = normY(j), hgt = normH(v);

      // Поворот вокруг вертикальной (высотной) оси на азимут, затем
      // наклон камеры на угол возвышения.
      const double x1 = x * cosA - y * sinA;
      const double y1 = x * sinA + y * cosA;
      const double h1 = hgt;

      const double y2 = y1 * cosE - h1 * sinE;  // используется как глубина для сортировки
      const double h2 = y1 * sinE + h1 * cosE;

      Proj p;
      p.sx = center.x() + x1 * scale;
      p.sy = center.y() - h2 * scale;
      p.depth = y2;
      p.valid = true;
      proj[idx] = p;
    }
  }

  struct Quad {
    double depth;
    QPointF pts[4];
    double avgValue;
  };
  std::vector<Quad> quads;
  quads.reserve(static_cast<size_t>(gw - 1) * static_cast<size_t>(gh - 1));

  for (int j = 0; j + 1 < gh; ++j) {
    for (int i = 0; i + 1 < gw; ++i) {
      const auto &p00 =
          proj[static_cast<size_t>(j) * static_cast<size_t>(gw) + static_cast<size_t>(i)];
      const auto &p10 =
          proj[static_cast<size_t>(j) * static_cast<size_t>(gw) + static_cast<size_t>(i + 1)];
      const auto &p01 =
          proj[static_cast<size_t>(j + 1) * static_cast<size_t>(gw) + static_cast<size_t>(i)];
      const auto &p11 =
          proj[static_cast<size_t>(j + 1) * static_cast<size_t>(gw) + static_cast<size_t>(i + 1)];
      if (!p00.valid || !p10.valid || !p01.valid || !p11.valid)
        continue;

      Quad q;
      q.pts[0] = QPointF(p00.sx, p00.sy);
      q.pts[1] = QPointF(p10.sx, p10.sy);
      q.pts[2] = QPointF(p11.sx, p11.sy);
      q.pts[3] = QPointF(p01.sx, p01.sy);
      q.depth = (p00.depth + p10.depth + p01.depth + p11.depth) / 4.0;
      q.avgValue =
          (grid[static_cast<size_t>(j) * static_cast<size_t>(gw) + static_cast<size_t>(i)] +
           grid[static_cast<size_t>(j) * static_cast<size_t>(gw) + static_cast<size_t>(i + 1)] +
           grid[static_cast<size_t>(j + 1) * static_cast<size_t>(gw) + static_cast<size_t>(i)] +
           grid[static_cast<size_t>(j + 1) * static_cast<size_t>(gw) +
                static_cast<size_t>(i + 1)]) /
          4.0;
      quads.push_back(q);
    }
  }

  // Painter's algorithm: дальние четырёхугольники рисуем первыми --
  // корректно для гладкой, не самопересекающейся поверхности (какой и
  // является наша карта высот).
  std::sort(quads.begin(), quads.end(),
            [](const Quad &a, const Quad &b) { return a.depth < b.depth; });

  QPen edgePen(QColor(20, 20, 22, 130));
  edgePen.setWidthF(0.6);
  edgePen.setCosmetic(true);

  for (const auto &q : quads) {
    const double t = (q.avgValue - minV) / range;
    const int hue = static_cast<int>(std::clamp((1.0 - t) * 240.0, 0.0, 240.0));

    QPolygonF poly;
    for (const auto &pt : q.pts)
      poly << pt;

    painter.setBrush(QColor::fromHsv(hue, 220, 235));
    painter.setPen(edgePen);
    painter.drawPolygon(poly);
  }
}

}  // namespace digitqt::gui::canvas
