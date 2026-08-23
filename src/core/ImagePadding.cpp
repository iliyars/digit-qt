#include "ImagePadding.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace digitqt::core {

namespace {

uchar dominantBorderGray(const QImage &gray) {
  const int w = gray.width();
  const int h = gray.height();

  std::array<int, 256> counts{};
  auto tally = [&](int x, int y) { ++counts[gray.constScanLine(y)[x]]; };

  for (int x = 0; x < w; ++x) {
    tally(x, 0);
    tally(x, h - 1);
  }
  for (int y = 1; y + 1 < h; ++y) {
    tally(0, y);
    tally(w - 1, y);
  }

  const auto it = std::max_element(counts.begin(), counts.end());
  return static_cast<uchar>(std::distance(counts.begin(), it));
}

}  // namespace

QImage padImageBackground(const QImage &image, double marginFraction) {
  if (image.isNull())
    return image;

  const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
  const int w = gray.width();
  const int h = gray.height();
  if (w <= 0 || h <= 0)
    return gray;

  const int margin = std::max(
      1, static_cast<int>(std::lround(marginFraction * std::max(w, h))));
  const uchar bg = dominantBorderGray(gray);

  QImage padded(w + 2 * margin, h + 2 * margin, QImage::Format_Grayscale8);
  padded.fill(bg);
  for (int y = 0; y < h; ++y) {
    std::memcpy(padded.scanLine(y + margin) + margin, gray.constScanLine(y),
                static_cast<size_t>(w));
  }

  return padded;
}

}  // namespace digitqt::core
