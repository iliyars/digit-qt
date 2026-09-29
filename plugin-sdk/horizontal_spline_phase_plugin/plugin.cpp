/**
 * @file plugin.cpp
 * @brief Горизонтальная сплайн-интерполяция фазы -- самодостаточный
 * DqtPhaseReconstructor C ABI-плагин. Не подключает и не линкует
 * DigitQt::Core -- см. тот же приём в sequential_fringe_tracker_plugin/
 * plugin.cpp. Порт core::pipeline::PhaseReconstructor (см. git-историю
 * PhaseReconstructor.{h,cpp} до выноса).
 *
 * Этому методу не нужны пиксели изображения вообще -- только геометрия
 * пронумерованных линий полос (image в reconstruct() игнорируется).
 */

#include "dqt_phase_reconstructor_abi.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace {

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct Line {
  std::vector<Point> points;
  double order = 0.0;
};

struct FringeCrossing {
  double x;
  double value;
  bool operator<(const FringeCrossing &other) const { return x < other.x; }
};

constexpr double kMinCrossingSpacing = 1.0;

std::vector<FringeCrossing> mergeNearDuplicateCrossings(std::vector<FringeCrossing> crossings) {
  std::vector<FringeCrossing> merged;
  merged.reserve(crossings.size());
  for (const auto &c : crossings) {
    if (!merged.empty() && c.x - merged.back().x < kMinCrossingSpacing) {
      merged.back().x = 0.5 * (merged.back().x + c.x);
      merged.back().value = 0.5 * (merged.back().value + c.value);
      continue;
    }
    merged.push_back(c);
  }
  return merged;
}

std::vector<FringeCrossing> findFringeCrossings(double worldY, const std::vector<Line> &lines) {
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
  return mergeNearDuplicateCrossings(std::move(crossings));
}

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
        l[i] = 1e-10;
      mu[i] = h[i] / l[i];
      z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

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
      return m_a[0] + m_b[0] * dx;
    }
    if (xi >= m_x[n - 1]) {
      const int i = n - 2;
      const double h = m_x[i + 1] - m_x[i];
      const double slopeAtEnd = m_b[i] + 2.0 * m_c[i] * h + 3.0 * m_d[i] * h * h;
      const double dx = xi - m_x[n - 1];
      return m_a[n - 1] + slopeAtEnd * dx;
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

class HorizontalSplinePhaseReconstructor {
 public:
  bool reconstruct(int width, int height, std::function<bool(int, int)> isVisible,
                   const std::vector<Line> &lines, double *outValues) {
    m_lastError.clear();

    // NaN по умолчанию -- "вне апертуры/не вычислено" (тот же контракт,
    // что у core::PhaseMap). Плагин полностью владеет заполнением
    // буфера, хосту незачем его предварительно нулить.
    std::fill(outValues, outValues + static_cast<size_t>(width) * height,
             std::numeric_limits<double>::quiet_NaN());

    if (width <= 0 || height <= 0) {
      m_lastError = "Invalid grid size";
      return false;
    }
    if (lines.empty()) {
      m_lastError = "No numbered fringe lines to reconstruct from";
      return false;
    }

    auto value = [&](int x, int y) -> double & { return outValues[static_cast<size_t>(y) * width + x]; };
    auto hasValue = [&](int x, int y) { return !std::isnan(value(x, y)); };

    bool anyRow = false;
    for (int y = 0; y < height; ++y) {
      const auto crossings = findFringeCrossings(static_cast<double>(y), lines);
      if (crossings.size() < 2)
        continue;

      const NaturalCubicSpline spline(crossings);
      anyRow = true;
      for (int x = 0; x < width; ++x) {
        if (!isVisible(x, y))
          continue;
        value(x, y) = spline.evaluate(static_cast<double>(x));
      }
    }

    if (!anyRow) {
      m_lastError = "Not enough fringe crossings per row to reconstruct phase";
      return false;
    }

    // Строки с <2 пересечений остаются NaN -- подтягиваем по вертикали
    // (см. объяснение в оригинале, core::pipeline::PhaseReconstructor).
    for (int x = 0; x < width; ++x) {
      std::vector<double> aboveValue(height, std::numeric_limits<double>::quiet_NaN());
      std::vector<int> aboveDist(height, -1);
      double lastValue = std::numeric_limits<double>::quiet_NaN();
      int lastDist = -1;
      for (int y = 0; y < height; ++y) {
        if (hasValue(x, y)) {
          lastValue = value(x, y);
          lastDist = 0;
        } else if (lastDist >= 0) {
          ++lastDist;
        }
        aboveValue[y] = lastValue;
        aboveDist[y] = lastDist;
      }

      lastValue = std::numeric_limits<double>::quiet_NaN();
      lastDist = -1;
      for (int y = height - 1; y >= 0; --y) {
        if (hasValue(x, y)) {
          lastValue = value(x, y);
          lastDist = 0;
        } else if (lastDist >= 0) {
          ++lastDist;
        }
        if (hasValue(x, y) || !isVisible(x, y))
          continue;

        const int dAbove = aboveDist[y];
        const int dBelow = lastDist;
        if (dAbove >= 0 && dBelow >= 0) {
          value(x, y) =
              (aboveValue[y] * dBelow + lastValue * dAbove) / static_cast<double>(dAbove + dBelow);
        } else if (dAbove >= 0) {
          value(x, y) = aboveValue[y];
        } else if (dBelow >= 0) {
          value(x, y) = lastValue;
        }
      }
    }

    return true;
  }

  const std::string &lastError() const { return m_lastError; }

 private:
  std::string m_lastError;
};

// --- Граница ABI --------------------------------------------------------

struct PluginState {
  HorizontalSplinePhaseReconstructor reconstructor;
  std::string lastErrorUtf8;
};

DqtPhaseReconstructorHandle create() {
  return reinterpret_cast<DqtPhaseReconstructorHandle>(new (std::nothrow) PluginState());
}

void destroy(DqtPhaseReconstructorHandle self) {
  delete reinterpret_cast<PluginState *>(self);
}

int reconstruct(DqtPhaseReconstructorHandle self, int32_t gridWidth, int32_t gridHeight,
                const DqtPhaseBitmapView * /*image*/, DqtPhaseVisibilityFn isVisible,
                void *isVisibleUserData, const DqtPhaseNumberedFringeLine *lines, size_t lineCount,
                double *outValues) {
  auto *state = reinterpret_cast<PluginState *>(self);

  std::vector<Line> cLines;
  cLines.reserve(lineCount);
  for (size_t i = 0; i < lineCount; ++i) {
    Line line;
    line.order = lines[i].order;
    line.points.reserve(lines[i].count);
    for (size_t j = 0; j < lines[i].count; ++j)
      line.points.push_back({lines[i].points[j].x, lines[i].points[j].y});
    cLines.push_back(std::move(line));
  }

  auto predicate = [isVisible, isVisibleUserData](int x, int y) {
    return isVisible(x, y, isVisibleUserData) != 0;
  };

  const bool ok = state->reconstructor.reconstruct(gridWidth, gridHeight, predicate, cLines, outValues);
  state->lastErrorUtf8 = state->reconstructor.lastError();
  return ok ? 1 : 0;
}

const char *name(DqtPhaseReconstructorHandle /*self*/) {
  static const char *const kName = "Horizontal Spline Interpolation";
  return kName;
}

const char *lastError(DqtPhaseReconstructorHandle self) {
  return reinterpret_cast<PluginState *>(self)->lastErrorUtf8.c_str();
}

const DqtPhaseReconstructorVTable kVTable = {
    &create, &destroy, &reconstruct, &name, &lastError,
};

}  // namespace

extern "C" DQT_ABI_EXPORT int dqt_phase_plugin_entry(uint32_t hostAbiVersion,
                                                      DqtPhasePluginInfo *outInfo,
                                                      const DqtPhaseReconstructorVTable **outVTable) {
  if (hostAbiVersion != DQT_PHASE_RECONSTRUCTOR_ABI_VERSION) {
    *outVTable = nullptr;
    return 0;
  }
  outInfo->pluginName = "HorizontalSplinePhaseReconstructor";
  outInfo->pluginVersion = "1.0.0";
  outInfo->needsFringeLines = 1;  // работает по геометрии линий, не по пикселям
  *outVTable = &kVTable;
  return 1;
}
