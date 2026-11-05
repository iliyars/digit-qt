#pragma once
#include "core/Bitmap.h"
#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <functional>
#include <vector>

namespace digitqt::core {

/**
 * @brief Automatically places seed points for the seed-based fringe
 * tracers (SequentialFringeTracker, StructureTensorTracker).
 *
 * Scans a horizontal row or vertical column across the fringes, chosen
 * from the dominant image gradient. Returns one seed per bright peak,
 * ordered left to right or top to bottom. Reuses
 * tracing::scanline_extremum::removeBackground()/detectPeaks() -- the
 * same per-row peak detection already used by ScanlineExtremumTracker --
 * rather than re-implementing peak finding.
 *
 * The section scanned is not necessarily the image's geometric
 * center: it's whichever section has the most visible (inside-aperture)
 * pixels, so this still works for an aperture that isn't centered in the
 * image.
 */
std::vector<tracing::SeedPoint> findRowSeeds(
    const digitqt::core::Bitmap &image, const std::function<bool(int, int)> &isVisible);

}  // namespace digitqt::core
