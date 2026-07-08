#pragma once

#include <functional>
#include <vector>

#include "ScanlineExtremumTypes.h"

namespace digitqt::core::tracing::scanline_extremum {

/**
 * @brief Constructs continuous, numbered fringe centerlines from the
 * per-scanline extrema detected by RedCenterDetector.
 *
 * Ported from the original Digit project's FringeConstructor, with one
 * correctness fix beyond the literal port: grouping into final polylines
 * uses an explicit chain identity (see ExtremumPoint::chainId), not the
 * display-only fringe number. The original grouped purely by number,
 * which two unrelated, independently-numbered chains (e.g. a fringe
 * reappearing after an obstruction) could collide on -- splicing two
 * unrelated fringe segments into one polyline. Chain identity is only
 * ever inherited through an actual accepted match, so that can no longer
 * happen. Everything else (matching, tolerance, crossing/alternation
 * checks) is unchanged.
 *
 * Algorithm:
 *   1. Pick a "main" scanline (most regular fringe spacing).
 *   2. Number its extrema sequentially (0, fringeStep, 2*fringeStep, ...);
 *      each also starts its own chain.
 *   3. Propagate those numbers/chains up and down to neighboring
 *      scanlines, matching each extremum to the nearest one in the
 *      previous scanline within tolerance, subject to:
 *        - same extremum type (Red/Black)
 *        - the match must not cross any already-built chain segment
 *        - (MinMax mode only) neighboring fringes must alternate Red/Black
 *   4. Unmatched extrema at the left/right edges get a new number AND a
 *      new, independent chain.
 *   5. Group same-chain extrema across scanlines into NumberedFringe
 *      polylines.
 */
class FringeConstructor {
public:
  static std::vector<NumberedFringe>
  constructFringes(std::vector<Section> &scanlines, int imageWidth,
                   int imageHeight,
                   const std::function<bool(int, int)> &isVisible,
                   FringeCenterMode fringeCenterAs, double fringeStep,
                   double toleranceFactor);

private:
  static int selectMainScanline(const std::vector<Section> &scanlines,
                                double fringeStep, double toleranceFactor);

  static int findMatchingExtremum(const ExtremumPoint &extremum,
                                  double currentX,
                                  const Section &adjacentScanline,
                                  double tolerance,
                                  FringeCenterMode fringeCenterAs);

  static bool wouldCross(int currentIdx, int proposedIdx,
                         const std::vector<ExtremumPoint> &currentExtrema,
                         const std::vector<ExtremumPoint> &adjacentExtrema);

  static bool segmentsIntersect(double x1, double y1, double x2, double y2,
                                double x3, double y3, double x4, double y4);

  static bool wouldViolateAlternation(ExtremumType currentType,
                                      double proposedNumber,
                                      const Section &adjacentScanline,
                                      FringeCenterMode fringeCenterAs,
                                      double fringeStep);

  static std::vector<NumberedFringe>
  convertToFringes(const std::vector<Section> &scanlines);
};

} // namespace digitqt::core::tracing::scanline_extremum
