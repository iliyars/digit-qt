#include "ImagePadding.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace digitqt::core {

namespace {

uint8_t dominantBorderGray(const digitqt::core::Bitmap &gray) {
  const int w = gray.width();
  const int h = gray.height();

  std::array<int, 256> counts{};
  auto tally = [&](int x, int y) { ++counts[gray.scanLine(y)[x]]; };

  for (int x = 0; x < w; ++x) {
    tally(x, 0);
    tally(x, h - 1);
  }
  for (int y = 1; y + 1 < h; ++y) {
    tally(0, y);
    tally(w - 1, y);
  }

  const auto it = std::max_element(counts.begin(), counts.end());
  return static_cast<uint8_t>(std::distance(counts.begin(), it));
}

}  // namespace

digitqt::core::Bitmap padImageBackground(const digitqt::core::Bitmap &image,
                                         double marginFraction) {
  if (image.isNull())
    return image;

  const int w = image.width();
  const int h = image.height();

  const int margin = std::max(
      1, static_cast<int>(std::lround(marginFraction * std::max(w, h))));
  const uint8_t bg = dominantBorderGray(image);

  digitqt::core::Bitmap padded(w + 2 * margin, h + 2 * margin);
  padded.fill(bg);
  for (int y = 0; y < h; ++y) {
    std::memcpy(padded.scanLine(y + margin) + margin, image.scanLine(y),
                static_cast<size_t>(w));
  }

  return padded;
}

}  // namespace digitqt::core
