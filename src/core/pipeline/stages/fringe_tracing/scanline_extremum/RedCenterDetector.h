#pragma once

#include "ScanlineExtremumTypes.h"

#include <vector>

namespace digitqt::core::tracing::scanline_extremum {

/**
 * @brief Detects fringe-center extrema (bright and/or dark) row by row.
 *
 * Faithful port of the original Digit project's RedCenterDetector, with
 * one adaptation: the original expected bottom-up (Windows DIB-style)
 * pixel data and flipped rows internally to compensate; here bitmapData
 * is assumed top-down (row 0 = top), matching QImage, so no flip is
 * needed. The peak-detection numeric logic itself (removeBackground /
 * detectPeaks) is unchanged.
 */
class RedCenterDetector {
public:
  static std::vector<Section> detectExtrema(const DigitizationInput &input);
};

}  // namespace digitqt::core::tracing::scanline_extremum
