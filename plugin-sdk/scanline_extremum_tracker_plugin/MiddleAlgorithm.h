#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace scanline_extremum_plugin {

/// Порт core's MiddleAlgorithm.h (removeBackground/fitQuadraticPeak/
/// detectPeaks) -- см. там же полное описание.
void removeBackground(uint8_t *line, int leftIdx, int rightIdx);

double fitQuadraticPeak(int *n, int *x, int *y);

std::vector<double> detectPeaks(const uint8_t *line, std::size_t nx, int y,
                                const std::function<bool(int, int)> &isVisible);

}  // namespace scanline_extremum_plugin
