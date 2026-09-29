#pragma once

#include "ScanlineExtremumTypes.h"

#include <vector>

namespace scanline_extremum_plugin {

/**
 * @brief Detects fringe-center extrema (bright and/or dark) row by row.
 * Порт core's RedCenterDetector -- см. там же полное описание.
 */
class RedCenterDetector {
public:
  static std::vector<Section> detectExtrema(const DigitizationInput &input);
};

}  // namespace scanline_extremum_plugin
