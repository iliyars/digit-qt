/**
 * @file PhaseReconstructor.cpp
 * @brief Построчная кубическая сплайн-интерполяция фазы по
 * пронумерованным линиям полос. Порт
 * WavefrontFromContoursSolver_HorizontalSpline из оригинального проекта
 * Digit -- метода, которым там реально формируется .mtr при экспорте.
 */
#include "PhaseReconstructor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace digitqt::core::pipeline {

namespace {

struct FringeCrossing {
  double x;
  double value;
  bool operator<(const FringeCrossing &other) const { return x < other.x; }
};

/// Точки пересечения всех пронумерованных линий с горизонталью y=worldY,
/// отсортированные по X. Порт
/// WavefrontFromContoursContext::findFringeCrossings: полуоткрытое
/// правило [ymin, ymax) для наклонных сегментов, чтобы горизонталь,
/// проходящая точно через общую вершину двух сегментов, не давала
/// пересечение дважды.
std::vector<FringeCrossing> findFringeCrossings(double worldY,
                                                const std::vector<NumberedFringeLine> &lines) {
  std::vector<FringeCrossing> crossings;
  constexpr double eps = 1e-12;

  for (const auto &line : lines) {
    const auto &pts = line.points;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
      const double x0 = pts[i].x, y0 = pts[i].y;
      const double x1 = pts[i + 1].x, y1 = pts[i + 1].y;

      if (std::abs(y1 - y0) < eps) {
        if (std::abs(worldY - y0) < eps)
          crossings.push_back({0.5 * (x0 + x1), line.order});
        continue;
      }

      const double ymin = std::min(y0, y1);
      const double ymax = std::max(y0, y1);
      if (worldY >= ymin - eps && worldY < ymax - eps) {
        const double t = (worldY - y0) / (y1 - y0);
        crossings.push_back({x0 + t * (x1 - x0), line.order});
      }
    }
  }

  std::sort(crossings.begin(), crossings.end());
  return crossings;
}

/// Натуральный кубический сплайн через (x, value) точки пересечения,
/// с экстраполяцией за крайние точки крайним сегментом. Требует не
/// менее 2 точек. Порт
/// WavefrontFromContoursSolver_HorizontalSpline::SplineCoefficients
/// (метод Томаса для трёхдиагональной системы вторых производных).
class NaturalCubicSpline {
public:
  explicit NaturalCubicSpline(const std::vector<FringeCrossing> &crossings) {
    const int n = static_cast<int>(crossings.size());
    m_x.resize(n);
    m_a.resize(n);
    for (int i = 0; i < n; ++i) {
      m_x[i] = crossings[i].x;
      m_a[i] = crossings[i].value;
    }

    if (n == 2) {
      m_b.resize(1);
      m_c.assign(1, 0.0);
      m_d.assign(1, 0.0);
      const double h = m_x[1] - m_x[0];
      m_b[0] = (std::abs(h) > 1e-10) ? (m_a[1] - m_a[0]) / h : 0.0;
      return;
    }

    std::vector<double> h(n - 1);
    for (int i = 0; i < n - 1; ++i)
      h[i] = m_x[i + 1] - m_x[i];

    std::vector<double> alpha(n, 0.0);
    for (int i = 1; i < n - 1; ++i)
      alpha[i] = 3.0 * ((m_a[i + 1] - m_a[i]) / h[i] - (m_a[i] - m_a[i - 1]) / h[i - 1]);

    std::vector<double> l(n, 1.0), mu(n, 0.0), z(n, 0.0);
    for (int i = 1; i < n - 1; ++i) {
      l[i] = 2.0 * (m_x[i + 1] - m_x[i - 1]) - h[i - 1] * mu[i - 1];
      if (std::abs(l[i]) < 1e-10)
        l[i] = 1e-10;  // избегаем деления на ноль на почти вырожденных узлах
      mu[i] = h[i] / l[i];
      z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    // Натуральные граничные условия: c[0] = c[n-1] = 0.
    m_c.assign(n, 0.0);
    m_b.resize(n - 1);
    m_d.resize(n - 1);
    for (int j = n - 2; j >= 0; --j) {
      m_c[j] = z[j] - mu[j] * m_c[j + 1];
      m_b[j] = (m_a[j + 1] - m_a[j]) / h[j] - h[j] * (m_c[j + 1] + 2.0 * m_c[j]) / 3.0;
      m_d[j] = (m_c[j + 1] - m_c[j]) / (3.0 * h[j]);
    }
  }

  double evaluate(double xi) const {
    const int n = static_cast<int>(m_x.size());

    if (xi <= m_x[0]) {
      const double dx = xi - m_x[0];
      return m_a[0] + m_b[0] * dx + m_c[0] * dx * dx + m_d[0] * dx * dx * dx;
    }
    if (xi >= m_x[n - 1]) {
      const int i = n - 2;
      const double dx = xi - m_x[i];
      return m_a[i] + m_b[i] * dx + m_c[i] * dx * dx + m_d[i] * dx * dx * dx;
    }

    int i = 0, j = n - 1;
    while (j - i > 1) {
      const int k = (i + j) / 2;
      if (xi < m_x[k])
        j = k;
      else
        i = k;
    }
    const double dx = xi - m_x[i];
    return m_a[i] + m_b[i] * dx + m_c[i] * dx * dx + m_d[i] * dx * dx * dx;
  }

private:
  std::vector<double> m_x, m_a, m_b, m_c, m_d;
};

}  // namespace

PhaseMap PhaseReconstructor::reconstruct(int width, int height,
                                         const std::function<bool(int, int)> &isVisible,
                                         const std::vector<NumberedFringeLine> &lines) {
  m_lastError.clear();

  if (width <= 0 || height <= 0) {
    m_lastError = QStringLiteral("Invalid grid size");
    return {};
  }
  if (lines.empty()) {
    m_lastError = QStringLiteral("No numbered fringe lines to reconstruct from");
    return {};
  }

  PhaseMap result(width, height);
  bool anyRow = false;

  for (int y = 0; y < height; ++y) {
    const auto crossings = findFringeCrossings(static_cast<double>(y), lines);
    if (crossings.size() < 2)
      continue;  // недостаточно данных для сплайна -- строка остаётся NaN, как в оригинале

    const NaturalCubicSpline spline(crossings);
    anyRow = true;
    for (int x = 0; x < width; ++x) {
      if (!isVisible(x, y))
        continue;
      result.setValue(x, y, spline.evaluate(static_cast<double>(x)));
    }
  }

  if (!anyRow) {
    m_lastError = QStringLiteral("Not enough fringe crossings per row to reconstruct phase");
    return {};
  }

  return result;
}

}  // namespace digitqt::core::pipeline
