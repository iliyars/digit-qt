#include "AutoSeedPlacement.h"

#include "core/pipeline/stages/fringe_tracing/scanline_extremum/MiddleAlgorithm.h"

#include <cmath>
#include <cstdint>

namespace digitqt::core {

std::vector<tracing::SeedPoint> findRowSeeds(
    const digitqt::core::Bitmap &image, const std::function<bool(int, int)> &isVisible) {
  std::vector<tracing::SeedPoint> result;
  if (image.isNull() || !isVisible)
    return result;

  const int width = image.width();
  const int height = image.height();
  if (width <= 0 || height <= 0)
    return result;

  double gradientX = 0, gradientY = 0;
  for (int y = 1; y + 1 < height; ++y)
    for (int x = 1; x + 1 < width; ++x) {
      if (!isVisible(x - 1, y) || !isVisible(x + 1, y) ||
          !isVisible(x, y - 1) || !isVisible(x, y + 1))
        continue;
      const double dx = double(image.pixel(x + 1, y)) - image.pixel(x - 1, y);
      const double dy = double(image.pixel(x, y + 1)) - image.pixel(x, y - 1);
      gradientX += dx * dx;
      gradientY += dy * dy;
    }
  const bool columns = gradientY > gradientX;
  const int crossSize = columns ? height : width;
  const int alongSize = columns ? width : height;
  auto visible = [&](int cross, int along) {
    return columns ? isVisible(along, cross) : isVisible(cross, along);
  };

  // Pick the row with the most visible pixels, rather than assuming the
  // image's geometric vertical center falls inside the aperture.
  int bestY = alongSize / 2;
  int bestVisibleCount = -1;
  for (int y = 0; y < alongSize; ++y) {
    int count = 0;
    for (int x = 0; x < crossSize; ++x)
      if (visible(x, y))
        ++count;
    if (count > bestVisibleCount) {
      bestVisibleCount = count;
      bestY = y;
    }
  }

  if (bestVisibleCount <= 0)
    return result;  // nothing visible anywhere

  std::vector<uint8_t> line(static_cast<size_t>(crossSize));
  for (int x = 0; x < crossSize; ++x)
    line[static_cast<size_t>(x)] = columns ? image.pixel(bestY, x) : image.pixel(x, bestY);

  tracing::scanline_extremum::removeBackground(line.data(), 0, crossSize - 1);
  const auto peaks = tracing::scanline_extremum::detectPeaks(
      line.data(), static_cast<size_t>(crossSize), bestY, visible);

  result.reserve(peaks.size());
  for (const double x : peaks)
    result.push_back(columns ? tracing::SeedPoint{bestY, static_cast<int>(std::lround(x))}
                             : tracing::SeedPoint{static_cast<int>(std::lround(x)), bestY});

  return result;
}

}  // namespace digitqt::core
