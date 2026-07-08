#include "RedCenterDetector.h"

#include "MiddleAlgorithm.h"

#include <algorithm>

namespace digitqt::core::tracing::scanline_extremum {

std::vector<Section>
RedCenterDetector::detectExtrema(const DigitizationInput &input) {
  std::vector<Section> results;
  if (!input.bitmapData || input.imageWidth <= 0 || input.imageHeight <= 0 ||
      !input.isVisible)
    return results;

  const int width = input.imageWidth;
  const int height = input.imageHeight;
  const int stride = (input.bytesPerLine > 0) ? input.bytesPerLine : width;

  std::vector<uint8_t> line(static_cast<size_t>(width));
  std::vector<uint8_t> invLine(static_cast<size_t>(width));

  results.reserve(static_cast<size_t>(height));

  for (int y = 0; y < height; ++y) {
    const unsigned char *row =
        input.bitmapData + static_cast<size_t>(y) * static_cast<size_t>(stride);
    for (int x = 0; x < width; ++x) {
      line[static_cast<size_t>(x)] = row[x];
      invLine[static_cast<size_t>(x)] = static_cast<uint8_t>(255 - row[x]);
    }

    std::vector<double> redXs;
    std::vector<double> blackXs;
    Section lineResult;

    const bool wantRed = (input.fringeCenterAs == FringeCenterMode::Max ||
                          input.fringeCenterAs == FringeCenterMode::MinMax);
    const bool wantBlack = (input.fringeCenterAs == FringeCenterMode::Min ||
                            input.fringeCenterAs == FringeCenterMode::MinMax);

    if (wantRed) {
      removeBackground(line.data(), 0, width - 1);
      redXs = detectPeaks(line.data(), static_cast<size_t>(width), y,
                          input.isVisible);
    }
    if (wantBlack) {
      removeBackground(invLine.data(), 0, width - 1);
      blackXs = detectPeaks(invLine.data(), static_cast<size_t>(width), y,
                            input.isVisible);
    }

    auto appendResults = [y, width,
                          &lineResult](const std::vector<double> &xs,
                                       const std::vector<uint8_t> &sourceLine,
                                       ExtremumType type) {
      for (const double x : xs) {
        const int sampleX = static_cast<int>(x + 0.5);
        double intensity = 0.0;
        if (sampleX >= 0 && sampleX < width)
          intensity = sourceLine[static_cast<size_t>(sampleX)];

        ExtremumPoint point;
        point.position = {x, static_cast<double>(y)};
        point.intensity = intensity;
        point.extremumType = type;
        lineResult.points.push_back(point);
      }
    };

    appendResults(redXs, line, ExtremumType::Red);
    appendResults(blackXs, invLine, ExtremumType::Black);

    std::sort(lineResult.points.begin(), lineResult.points.end(),
              [](const ExtremumPoint &a, const ExtremumPoint &b) {
                return a.position.x < b.position.x;
              });

    for (int x = 0; x < width; ++x) {
      if (input.isVisible(x, y)) {
        lineResult.limits.leftEdge.x = x;
        break;
      }
    }
    for (int x = width - 1; x > 0; --x) {
      if (input.isVisible(x, y)) {
        lineResult.limits.rightEdge.x = x;
        break;
      }
    }

    results.push_back(std::move(lineResult));
  }

  return results;
}

} // namespace digitqt::core::tracing::scanline_extremum
