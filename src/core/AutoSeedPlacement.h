#pragma once
#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QImage>
#include <functional>
#include <vector>

namespace digitqt::core {

/**
 * @brief Automatically places seed points for the seed-based fringe
 * tracers (SequentialFringeTracker, StructureTensorTracker).
 *
 * Scans a single horizontal row for intensity peaks and returns one seed
 * per peak found, left to right. Reuses
 * tracing::scanline_extremum::removeBackground()/detectPeaks() -- the
 * same per-row peak detection already used by ScanlineExtremumTracker --
 * rather than re-implementing peak finding.
 *
 * The row scanned is not necessarily the image's geometric vertical
 * center: it's whichever row has the most visible (inside-aperture)
 * pixels, so this still works for an aperture that isn't centered in the
 * image.
 */
std::vector<tracing::SeedPoint>
findRowSeeds(const QImage &image,
             const std::function<bool(int, int)> &isVisible);

} // namespace digitqt::core
