#pragma once

#include "ScanlineExtremumTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace scanline_extremum_plugin {

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

/**
 * @brief Scanline Extremum Method (FTM) -- самодостаточный порт
 * core::tracing::ScanlineExtremumTracker. Глобальный, seed-свободный
 * трассировщик: сканирует изображение построчно (RedCenterDetector),
 * затем связывает и нумерует найденные экстремумы в непрерывные полосы
 * (FringeConstructor).
 */
class ScanlineExtremumTracker {
 public:
  struct Params {
    FringeCenterMode fringeCenterAs = FringeCenterMode::MinMax;
    double fringeStep = 1.0;
    double toleranceFactor = 0.3;
    bool hasInternalObstruction = false;
  };

  bool initialize(const uint8_t *pixels, int width, int height,
                  std::function<bool(int, int)> isVisible);
  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds);

  const std::string &lastError() const { return m_lastError; }

  void setParams(const Params &params) { m_params = params; }
  bool setParam(const std::string &key, const std::string &value);

  /// Реальный, глобально-согласованный номер полосы, посчитанный
  /// FringeConstructor -- по одному на линию из последнего extract(), в
  /// том же порядке.
  const std::vector<double> &lastFringeNumbers() const { return m_lastFringeNumbers; }

 private:
  std::vector<uint8_t> m_pixelStorage;
  const uint8_t *m_image = nullptr;
  int m_width = 0;
  int m_height = 0;
  std::function<bool(int, int)> m_isVisible;
  Params m_params;
  std::string m_lastError;
  std::vector<double> m_lastFringeNumbers;
};

}  // namespace scanline_extremum_plugin
