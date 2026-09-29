#pragma once

#include "ScanlineExtremumTypes.h"

#include <functional>
#include <vector>

namespace scanline_extremum_plugin {

/**
 * @brief Constructs continuous, numbered fringe centerlines from the
 * per-scanline extrema detected by RedCenterDetector. Порт core's
 * FringeConstructor -- см. там же полное описание алгоритма.
 */
class FringeConstructor {
public:
  static std::vector<NumberedFringe> constructFringes(
      std::vector<Section> &scanlines, int imageWidth, int imageHeight,
      const std::function<bool(int, int)> &isVisible, FringeCenterMode fringeCenterAs,
      double fringeStep, double toleranceFactor, bool hasInternalObstruction = false);

private:
  static int selectMainScanline(const std::vector<Section> &scanlines, double fringeStep,
                                double toleranceFactor, bool hasInternalObstruction);

  static int findMatchingExtremum(const ExtremumPoint &extremum, double currentX,
                                  const Section &adjacentScanline, double tolerance,
                                  FringeCenterMode fringeCenterAs);

  static bool wouldCross(int currentIdx, int proposedIdx,
                         const std::vector<ExtremumPoint> &currentExtrema,
                         const std::vector<ExtremumPoint> &adjacentExtrema);

  static bool segmentsIntersect(double x1, double y1, double x2, double y2, double x3, double y3,
                                double x4, double y4);

  static bool wouldViolateAlternation(ExtremumType currentType, double proposedNumber,
                                      const Section &adjacentScanline,
                                      FringeCenterMode fringeCenterAs, double fringeStep);

  static std::vector<NumberedFringe> convertToFringes(const std::vector<Section> &scanlines);
};

}  // namespace scanline_extremum_plugin
