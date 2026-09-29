/**
 * @file plugin.cpp
 * @brief Ridge Tracking (Structure Tensor) -- самодостаточный
 * DqtFringeTracer C ABI-плагин. Не подключает и не линкует DigitQt::Core --
 * см. тот же приём в sequential_fringe_tracker_plugin/plugin.cpp.
 *
 * Порт из core::tracing::StructureTensorTracker (см. git-историю
 * src/core/pipeline/stages/fringe_tracing/StructureTensorTracker.{h,cpp}
 * до выноса) без изменений в самой арифметике -- core::Bitmap там
 * использовался только в initialize() для копирования пикселей, весь
 * остальной код и так работал на сыром указателе.
 *
 * Направление вдоль полосы ищется через структурный тензор локального
 * градиента (непрерывный угол, не квантованный на 45°, в отличие от
 * SequentialFringeTracker), центрирование -- честный 2D-поиск максимума
 * с суб-пиксельной доводкой через квадратичный фит.
 */

#include "dqt_fringe_tracer_abi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TracedPoint {
  double x = 0.0;
  double y = 0.0;
  float width = 0.0f;
  float intensity = 0.0f;
};
using TracedLine = std::vector<TracedPoint>;

struct SeedPoint {
  int x = 0;
  int y = 0;
};

struct StructureTensorParams {
  int maxSteps = 200;
  bool bidirectional = true;
  int orientationWindowRadius = 3;
  int directionSmoothingWindow = 5;
  float stepFraction = 0.5f;
  float minStepSize = 2.0f;
  float maxStepSize = 15.0f;
  float searchRadiusFraction = 0.5f;
  float minSearchRadius = 2.0f;
  float maxSearchRadius = 15.0f;
  float maxCenteringJumpFactor = 1.0f;
  float contrastSmoothingAlpha = 0.3f;
  float minContrastFraction = 0.4f;
};

class StructureTensorTracker {
 public:
  bool initialize(const uint8_t *pixels, int width, int height,
                  std::function<bool(int, int)> isVisible) {
    if (!pixels || width <= 0 || height <= 0) {
      m_lastError = "Empty image";
      return false;
    }
    m_pixelStorage.assign(pixels, pixels + static_cast<size_t>(width) * height);
    m_image = m_pixelStorage.data();
    m_width = width;
    m_height = height;
    m_stride = width;
    m_isVisible = std::move(isVisible);
    m_lastError.clear();
    return true;
  }

  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds) {
    std::vector<TracedLine> result;
    result.reserve(seeds.size());
    for (const auto &seed : seeds) {
      TracedLine line;
      if (traceLineInto(seed.x, seed.y, line) && line.size() >= 2)
        result.push_back(std::move(line));
    }
    return result;
  }

  const std::string &lastError() const { return m_lastError; }

 private:
  bool isInside(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
      return false;
    return !m_isVisible || m_isVisible(x, y);
  }

  uint8_t getPixel(int x, int y) const {
    if (!m_image || x < 0 || x >= m_width || y < 0 || y >= m_height)
      return 0;
    return m_image[y * m_stride + x];
  }

  float sampleBilinear(double x, double y) const {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const double fx = x - x0;
    const double fy = y - y0;

    const float p00 = static_cast<float>(getPixel(x0, y0));
    const float p10 = static_cast<float>(getPixel(x0 + 1, y0));
    const float p01 = static_cast<float>(getPixel(x0, y0 + 1));
    const float p11 = static_cast<float>(getPixel(x0 + 1, y0 + 1));

    const float top = static_cast<float>(p00 * (1.0 - fx) + p10 * fx);
    const float bottom = static_cast<float>(p01 * (1.0 - fx) + p11 * fx);
    return static_cast<float>(top * (1.0 - fy) + bottom * fy);
  }

  bool estimateDirection(double x, double y, double &dirX, double &dirY) const {
    const int radius = m_params.orientationWindowRadius;
    const int cx = static_cast<int>(std::lround(x));
    const int cy = static_cast<int>(std::lround(y));

    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    bool any = false;

    for (int oy = -radius; oy <= radius; ++oy) {
      for (int ox = -radius; ox <= radius; ++ox) {
        const int sx = cx + ox;
        const int sy = cy + oy;
        if (!isInside(sx, sy))
          continue;

        const float gx =
            static_cast<float>(getPixel(sx + 1, sy)) - static_cast<float>(getPixel(sx - 1, sy));
        const float gy =
            static_cast<float>(getPixel(sx, sy + 1)) - static_cast<float>(getPixel(sx, sy - 1));

        sxx += static_cast<double>(gx) * gx;
        syy += static_cast<double>(gy) * gy;
        sxy += static_cast<double>(gx) * gy;
        any = true;
      }
    }

    if (!any)
      return false;

    const double trace = sxx + syy;
    const double det = sxx * syy - sxy * sxy;
    const double disc = std::sqrt(std::max(0.0, (trace * trace) / 4.0 - det));
    const double lambdaMin = trace / 2.0 - disc;

    double vx, vy;
    if (std::fabs(sxy) > 1e-9) {
      vx = sxy;
      vy = lambdaMin - sxx;
    } else if (sxx <= syy) {
      vx = 1.0;
      vy = 0.0;
    } else {
      vx = 0.0;
      vy = 1.0;
    }

    const double len = std::sqrt(vx * vx + vy * vy);
    if (len < 1e-9)
      return false;

    dirX = vx / len;
    dirY = vy / len;
    return true;
  }

  float estimateWidth(double x, double y, double dirX, double dirY, float &outBackground) const {
    const double px = -dirY;
    const double py = dirX;

    const float centerIntensity = sampleBilinear(x, y);

    constexpr double kProbeDistance = 25.0;
    float bgPlus = centerIntensity;
    for (double t = 1.0; t <= kProbeDistance; t += 1.0)
      bgPlus = std::min(bgPlus, sampleBilinear(x + px * t, y + py * t));
    float bgMinus = centerIntensity;
    for (double t = 1.0; t <= kProbeDistance; t += 1.0)
      bgMinus = std::min(bgMinus, sampleBilinear(x - px * t, y - py * t));
    const float background = std::min(bgPlus, bgMinus);
    outBackground = background;

    const float halfLevel = (centerIntensity + background) / 2.0f;

    double distPlus = 1.0;
    for (double t = 0.5; t < kProbeDistance; t += 0.5) {
      if (sampleBilinear(x + px * t, y + py * t) < halfLevel)
        break;
      distPlus = t;
    }
    double distMinus = 1.0;
    for (double t = 0.5; t < kProbeDistance; t += 0.5) {
      if (sampleBilinear(x - px * t, y - py * t) < halfLevel)
        break;
      distMinus = t;
    }

    return std::clamp(static_cast<float>(distPlus + distMinus), 2.0f, 80.0f);
  }

  bool findLocalMaximum(double cx, double cy, float radius, double &outX, double &outY,
                        float &outIntensity) const {
    const int ix = static_cast<int>(std::lround(cx));
    const int iy = static_cast<int>(std::lround(cy));
    const int r = std::max(1, static_cast<int>(std::lround(radius)));

    int bestX = ix, bestY = iy;
    float bestVal = -1.0f;
    bool any = false;

    for (int oy = -r; oy <= r; ++oy) {
      for (int ox = -r; ox <= r; ++ox) {
        const int sx = ix + ox;
        const int sy = iy + oy;
        if (!isInside(sx, sy))
          continue;
        const float v = static_cast<float>(getPixel(sx, sy));
        any = true;
        if (v > bestVal) {
          bestVal = v;
          bestX = sx;
          bestY = sy;
        }
      }
    }

    if (!any)
      return false;

    double refinedX = bestX;
    double refinedY = bestY;

    if (isInside(bestX - 1, bestY) && isInside(bestX + 1, bestY)) {
      const float left = static_cast<float>(getPixel(bestX - 1, bestY));
      const float center = static_cast<float>(getPixel(bestX, bestY));
      const float right = static_cast<float>(getPixel(bestX + 1, bestY));
      const float denom = left - 2.0f * center + right;
      if (std::fabs(denom) > 1e-4f) {
        const double offset = 0.5 * (left - right) / denom;
        if (std::fabs(offset) < 1.0)
          refinedX = bestX + offset;
      }
    }
    if (isInside(bestX, bestY - 1) && isInside(bestX, bestY + 1)) {
      const float up = static_cast<float>(getPixel(bestX, bestY - 1));
      const float center = static_cast<float>(getPixel(bestX, bestY));
      const float down = static_cast<float>(getPixel(bestX, bestY + 1));
      const float denom = up - 2.0f * center + down;
      if (std::fabs(denom) > 1e-4f) {
        const double offset = 0.5 * (up - down) / denom;
        if (std::fabs(offset) < 1.0)
          refinedY = bestY + offset;
      }
    }

    outX = refinedX;
    outY = refinedY;
    outIntensity = sampleBilinear(refinedX, refinedY);
    return true;
  }

  bool traceLineInto(int startX, int startY, TracedLine &outPoints) {
    outPoints.clear();
    m_lastError.clear();

    if (!isInside(startX, startY)) {
      m_lastError = "Start point is outside the aperture/boundaries";
      return false;
    }

    double dirX = 0.0, dirY = 1.0;
    if (!estimateDirection(startX, startY, dirX, dirY)) {
      m_lastError = "Could not determine initial fringe direction";
      return false;
    }

    double seedX = startX, seedY = startY;
    float seedIntensity = 0.0f;
    if (!findLocalMaximum(startX, startY, 5.0f, seedX, seedY, seedIntensity)) {
      m_lastError = "No usable intensity peak near the seed point";
      return false;
    }

    float seedBackground = 0.0f;
    const float seedWidth = estimateWidth(seedX, seedY, dirX, dirY, seedBackground);

    TracedPoint seedPoint;
    seedPoint.x = seedX;
    seedPoint.y = seedY;
    seedPoint.width = seedWidth;
    seedPoint.intensity = seedIntensity;

    TracedLine forwardLine;
    forwardLine.push_back(seedPoint);
    m_contrastEma = 0.0f;
    traceDirectionally(forwardLine, dirX, dirY);

    outPoints = forwardLine;

    if (m_params.bidirectional) {
      TracedLine backwardLine;
      backwardLine.push_back(seedPoint);
      m_contrastEma = 0.0f;
      traceDirectionally(backwardLine, -dirX, -dirY);

      if (backwardLine.size() > 1) {
        TracedLine combined;
        combined.assign(backwardLine.rbegin(), backwardLine.rend() - 1);
        combined.insert(combined.end(), forwardLine.begin(), forwardLine.end());
        outPoints = std::move(combined);
      }
    }

    return outPoints.size() >= 2;
  }

  void traceDirectionally(TracedLine &line, double dirX, double dirY) {
    std::vector<std::pair<double, double>> dirHistory;
    dirHistory.emplace_back(dirX, dirY);

    for (int i = 0; i < m_params.maxSteps; ++i) {
      const TracedPoint &last = line.back();
      const float width =
          (last.width > 0.0f) ? last.width : static_cast<float>(m_width) / 6.0f;

      const double stepSize =
          std::clamp(static_cast<double>(width) * m_params.stepFraction,
                     static_cast<double>(m_params.minStepSize),
                     static_cast<double>(m_params.maxStepSize));

      const double predX = last.x + dirX * stepSize;
      const double predY = last.y + dirY * stepSize;

      if (!isInside(static_cast<int>(std::lround(predX)), static_cast<int>(std::lround(predY))))
        break;

      const float searchRadius = std::clamp(width * m_params.searchRadiusFraction,
                                            m_params.minSearchRadius, m_params.maxSearchRadius);

      double foundX = 0.0, foundY = 0.0;
      float foundIntensity = 0.0f;
      if (!findLocalMaximum(predX, predY, searchRadius, foundX, foundY, foundIntensity))
        break;

      const double jumpDx = foundX - predX;
      const double jumpDy = foundY - predY;
      if (std::sqrt(jumpDx * jumpDx + jumpDy * jumpDy) > width * m_params.maxCenteringJumpFactor)
        break;

      float background = 0.0f;
      const float newWidth = estimateWidth(foundX, foundY, dirX, dirY, background);
      const float contrast = foundIntensity - background;

      if (m_contrastEma > 0.0f && contrast < m_contrastEma * m_params.minContrastFraction)
        break;
      m_contrastEma = (m_contrastEma <= 0.0f)
                          ? contrast
                          : (m_params.contrastSmoothingAlpha * contrast +
                             (1.0f - m_params.contrastSmoothingAlpha) * m_contrastEma);

      TracedPoint p;
      p.x = foundX;
      p.y = foundY;
      p.width = newWidth;
      p.intensity = foundIntensity;
      line.push_back(p);

      if (static_cast<int>(line.size()) > 5) {
        const TracedPoint &ref = line.front();
        const double dx0 = foundX - ref.x;
        const double dy0 = foundY - ref.y;
        if (std::sqrt(dx0 * dx0 + dy0 * dy0) < newWidth)
          break;
      }

      double nextDirX = dirX, nextDirY = dirY;
      if (estimateDirection(foundX, foundY, nextDirX, nextDirY)) {
        if (nextDirX * dirX + nextDirY * dirY < 0.0) {
          nextDirX = -nextDirX;
          nextDirY = -nextDirY;
        }
        dirHistory.emplace_back(nextDirX, nextDirY);
        if (static_cast<int>(dirHistory.size()) > m_params.directionSmoothingWindow)
          dirHistory.erase(dirHistory.begin());

        double sumX = 0.0, sumY = 0.0;
        for (const auto &d : dirHistory) {
          sumX += d.first;
          sumY += d.second;
        }
        const double len = std::sqrt(sumX * sumX + sumY * sumY);
        if (len > 1e-6) {
          dirX = sumX / len;
          dirY = sumY / len;
        }
      }
    }
  }

  std::vector<uint8_t> m_pixelStorage;
  const uint8_t *m_image = nullptr;
  int m_width = 0;
  int m_height = 0;
  int m_stride = 0;
  std::function<bool(int, int)> m_isVisible;

  StructureTensorParams m_params;
  float m_contrastEma = 0.0f;
  std::string m_lastError;
};

// --- Граница ABI --------------------------------------------------------

struct PluginState {
  StructureTensorTracker tracker;
  std::vector<std::vector<DqtTracedPoint>> pointStorage;
  std::vector<DqtTracedLine> lineStorage;
  std::string lastErrorUtf8;
};

DqtFringeTracerHandle create() {
  return reinterpret_cast<DqtFringeTracerHandle>(new (std::nothrow) PluginState());
}

void destroy(DqtFringeTracerHandle self) {
  delete reinterpret_cast<PluginState *>(self);
}

int initialize(DqtFringeTracerHandle self, const DqtBitmapView *image, DqtVisibilityFn isVisible,
              void *isVisibleUserData) {
  auto *state = reinterpret_cast<PluginState *>(self);
  auto predicate = [isVisible, isVisibleUserData](int x, int y) {
    return isVisible(x, y, isVisibleUserData) != 0;
  };
  const bool ok = state->tracker.initialize(image->pixels, image->width, image->height, predicate);
  state->lastErrorUtf8 = state->tracker.lastError();
  return ok ? 1 : 0;
}

int extract(DqtFringeTracerHandle self, const DqtSeedPoint *seeds, size_t seedCount,
           DqtTracedLine **outLines, size_t *outLineCount) {
  auto *state = reinterpret_cast<PluginState *>(self);
  *outLines = nullptr;
  *outLineCount = 0;

  std::vector<SeedPoint> seedVec;
  seedVec.reserve(seedCount);
  for (size_t i = 0; i < seedCount; ++i)
    seedVec.push_back({seeds[i].x, seeds[i].y});

  std::vector<TracedLine> lines = state->tracker.extract(seedVec);
  state->lastErrorUtf8 = state->tracker.lastError();

  state->pointStorage.assign(lines.size(), {});
  state->lineStorage.clear();
  state->lineStorage.reserve(lines.size());

  for (size_t i = 0; i < lines.size(); ++i) {
    auto &dst = state->pointStorage[i];
    dst.reserve(lines[i].size());
    for (const auto &p : lines[i])
      dst.push_back(DqtTracedPoint{p.x, p.y, p.width, p.intensity});
    state->lineStorage.push_back(DqtTracedLine{dst.data(), dst.size(), 0.0, 0});
  }

  if (!state->lineStorage.empty()) {
    *outLines = state->lineStorage.data();
    *outLineCount = state->lineStorage.size();
  }
  return 1;
}

void freeLines(DqtFringeTracerHandle /*self*/, DqtTracedLine * /*lines*/, size_t /*lineCount*/) {
}

int setParam(DqtFringeTracerHandle /*self*/, const char * /*key*/, const char * /*value*/) {
  // SetupStage сейчас не настраивает StructureTensorParams -- все ключи
  // неизвестны.
  return 0;
}

const char *name(DqtFringeTracerHandle /*self*/) {
  static const char *const kName = "Ride Tracking (Structure Tensor)";
  return kName;
}

const char *lastError(DqtFringeTracerHandle self) {
  return reinterpret_cast<PluginState *>(self)->lastErrorUtf8.c_str();
}

const DqtFringeTracerVTable kVTable = {
    &create, &destroy, &initialize, &extract, &freeLines, &setParam, &name, &lastError,
};

}  // namespace

extern "C" DQT_ABI_EXPORT int dqt_plugin_entry(uint32_t hostAbiVersion, DqtPluginInfo *outInfo,
                                               const DqtFringeTracerVTable **outVTable) {
  if (hostAbiVersion != DQT_FRINGE_TRACER_ABI_VERSION) {
    *outVTable = nullptr;
    return 0;
  }
  outInfo->pluginName = "StructureTensorTracker";
  outInfo->pluginVersion = "2.0.0";  // 2.0.0: полностью самодостаточная реализация
  *outVTable = &kVTable;
  return 1;
}
