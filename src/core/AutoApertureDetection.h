#pragma once

#include "core/Bitmap.h"

#include <memory>
#include <string>

namespace aperture {
class Ellipse;
}

namespace digitqt::core {

/**
 * @brief Result of detectApertureBoundary(): either a fitted boundary
 * ellipse, or an empty ellipse + errorMessage explaining why none could
 * be found.
 */
struct ApertureDetectionResult {
  std::unique_ptr<aperture::Ellipse> ellipse;
  std::string errorMessage;
  bool ok() const { return ellipse != nullptr; }
};

/**
 * @brief Finds the aperture in `image` and fits an external boundary
 * ellipse to it, with no seeds/manual clicking required.
 *
 * Interferograms don't agree on which side (bright or dark) is the
 * aperture and which is background, so this doesn't threshold on
 * brightness at all. Instead it looks for local intensity VARIATION:
 * fringes modulate intensity from row to row and column to column, a
 * flat background doesn't, regardless of which side is brighter. The
 * largest connected high-variation region's contour is then fit with an
 * ellipse the same way
 * BoundaryEditController::finalizePointsEllipse() fits one from
 * manually-clicked points (centroid-centered first, to avoid
 * aperture::Ellipse::FitEllipse's ill-conditioning on raw pixel
 * coordinates far from the origin).
 *
 * A heuristic, not a measurement -- exactly like
 * AutoSeedPlacement::findRowSeeds(), it can fail outright (no visible
 * activity) or land slightly off on a low-contrast/noisy image; the
 * result is meant as a fast starting point, still editable/movable/
 * resizable like any other boundary shape afterward.
 */
ApertureDetectionResult detectApertureBoundary(const digitqt::core::Bitmap &image);

}  // namespace digitqt::core
