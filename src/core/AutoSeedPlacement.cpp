#include "AutoSeedPlacement.h"

#include "core/pipeline/stages/fringe_tracing/scanline_extremum/MiddleAlgorithm.h"

#include <cmath>
#include <cstdint>

namespace digitqt::core {

std::vector<tracing::SeedPoint> findRowSeeds(
    const QImage &image, const std::function<bool(int, int)> &isVisible) {
  std::vector<tracing::SeedPoint> result;
  if (image.isNull() || !isVisible)
    return result;

  const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
  const int width = gray.width();
  const int height = gray.height();
  if (width <= 0 || height <= 0)
    return result;

  // Pick the row with the most visible pixels, rather than assuming the
  // image's geometric vertical center falls inside the aperture.
  int bestY = height / 2;
  int bestVisibleCount = -1;
  for (int y = 0; y < height; ++y) {
    int count = 0;
    for (int x = 0; x < width; ++x)
      if (isVisible(x, y))
        ++count;
    if (count > bestVisibleCount) {
      bestVisibleCount = count;
      bestY = y;
    }
  }

  if (bestVisibleCount <= 0)
    return result;  // nothing visible anywhere

  std::vector<uint8_t> line(static_cast<size_t>(width));
  const uchar *row = gray.constScanLine(bestY);
  for (int x = 0; x < width; ++x)
    line[static_cast<size_t>(x)] = row[x];

  tracing::scanline_extremum::removeBackground(line.data(), 0, width - 1);
  const auto peaks = tracing::scanline_extremum::detectPeaks(
      line.data(), static_cast<size_t>(width), bestY, isVisible);

  result.reserve(peaks.size());
  for (const double x : peaks)
    result.push_back(
        tracing::SeedPoint{static_cast<int>(std::lround(x)), bestY});

  return result;
}

}  // namespace digitqt::core
