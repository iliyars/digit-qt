#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace digitqt::core::tracing::scanline_extremum {

/**
 * @brief Removes a linear/piecewise-linear background estimate from a
 * scanline segment before peak detection, zeroing pixels below it.
 *
 * For long segments (>70 px) the interval is split into 5 parts; the
 * average of each part gives a piecewise-linear background. Short
 * segments use one constant background (mean of non-zero pixels).
 *
 * Faithful port of the original Digit project's Utils/middle.cpp
 * (fon_del_) -- numeric logic unchanged.
 */
void removeBackground(uint8_t *line, int leftIdx, int rightIdx);

/**
 * @brief Sub-pixel peak position via quadratic fit y = c0 + c1*x + c2*x^2.
 * @param n Number of sample points in/out; set negative on failure.
 * @param x Integer X-coordinates of samples.
 * @param y Integer intensity values of samples.
 * @return Estimated peak position (vertex + 0.5). 0.0 on failure (see *n).
 *
 * Faithful port of the original Digit project's Utils/middle.cpp
 * (approx_) -- numeric logic unchanged.
 */
double fitQuadraticPeak(int *n, int *x, int *y);

/**
 * @brief Detects all peak (fringe-center) sub-pixel positions in one
 * scanline, scanning left to right over runs of non-zero, visible pixels.
 *
 * Faithful port of the original Digit project's Utils/middle.cpp
 * (middle_) -- numeric logic unchanged.
 */
std::vector<double> detectPeaks(const uint8_t *line, std::size_t nx, int y,
                                const std::function<bool(int, int)> &isVisible);

}  // namespace digitqt::core::tracing::scanline_extremum
