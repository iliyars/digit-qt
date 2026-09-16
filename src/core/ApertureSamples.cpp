#include "ApertureSamples.h"

#include <algorithm>

namespace digitqt::core {

ApertureGeometry computeApertureGeometry(const PhaseMap &map) {
  const int w = map.width();
  const int h = map.height();

  int minX = w, maxX = -1, minY = h, maxY = -1;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!map.hasValue(x, y))
        continue;
      minX = std::min(minX, x);
      maxX = std::max(maxX, x);
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
    }
  }

  ApertureGeometry geometry;
  geometry.centerX = w / 2.0;
  geometry.centerY = h / 2.0;
  geometry.radius = std::max(w, h) / 2.0;
  if (maxX >= minX && maxY >= minY) {
    geometry.centerX = (minX + maxX) / 2.0;
    geometry.centerY = (minY + maxY) / 2.0;
    geometry.radius = std::max(maxX - minX, maxY - minY) / 2.0;
  }
  if (geometry.radius <= 0.0)
    geometry.radius = std::max(w, h) / 2.0;
  return geometry;
}

std::vector<ApertureSample> collectApertureSamples(const PhaseMap &map,
                                                    const ApertureGeometry &geometry,
                                                    int edgeErosionPixels) {
  const int w = map.width();
  const int h = map.height();
  const int erosion = std::max(0, edgeErosionPixels);

  auto isCore = [&](int x, int y) {
    for (int dy = -erosion; dy <= erosion; ++dy)
      for (int dx = -erosion; dx <= erosion; ++dx)
        if (!map.hasValue(x + dx, y + dy))
          return false;
    return true;
  };

  std::vector<ApertureSample> samples;
  samples.reserve(static_cast<size_t>(w) * static_cast<size_t>(h) / 4);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!isCore(x, y))
        continue;
      const double nx = (x - geometry.centerX) / geometry.radius;
      const double ny = (y - geometry.centerY) / geometry.radius;
      samples.push_back({nx, ny, map.value(x, y), x, y});
    }
  }
  return samples;
}

}  // namespace digitqt::core
