#pragma once

#include "core/Bitmap.h"

#include <functional>
#include <string>
#include <vector>

namespace digitqt::core::tracing {

/// One point along a traced fringe centerline, in image pixel coordinates.
struct TracedPoint {
  double x = 0.0;
  double y = 0.0;
  float width = 0.0f;      // locally measured fringe width, in pixels
  float intensity = 0.0f;  // locally measured average intensity (0..255)
};

using TracedLine = std::vector<TracedPoint>;

/// A user-provided starting point for tracers that need one (seed-based
/// algorithms). Ignored by tracers that find lines globally (e.g. a
/// future morphological skeletonizer).
struct SeedPoint {
  int x = 0;
  int y = 0;
};

/**
 * @brief Common contract for fringe-centerline extraction algorithms.
 *
 * Multiple independent implementations exist on purpose, so they can be
 * run on the same image and compared side by side (see the S1 parameters
 * panel's algorithm picker):
 *   - SequentialFringeTracker: Sequential Fringe Tracking (FTM), step-by-step
 *     from a seed point, ported from the classic SCAN360/STEP.C algorithm.
 *   - ScanlineExtremumTracker: Scanline Extremum Method (FTM), global
 *     row-by-row extrema + fringe numbering, ported from the original Digit
 * project's RedCenterDetector / FringeConstructor.
 *   - (planned) MorphologicalSkeletonTracer: global morphological
 *     skeleton (OpenCV); needs no seed points at all.
 */
class IFringeTracer {
public:
  virtual ~IFringeTracer() = default;

  /// Binds the algorithm to an image and a visibility predicate (true
  /// if pixel (x,y) is inside the current aperture/boundaries). Must be
  /// called before extract().
  virtual bool initialize(const digitqt::core::Bitmap &image,
                          std::function<bool(int, int)> isVisible) = 0;

  /// Extracts all centerlines found. seeds is only used by tracers that
  /// need starting points; global algorithms ignore it.
  virtual std::vector<TracedLine> extract(
      const std::vector<SeedPoint> &seeds) = 0;

  virtual std::string name() const = 0;
  virtual const std::string &lastError() const = 0;

  /// Optional algorithm-specific parameter, set before initialize().
  /// Returns true if the key was recognized and applied; unrecognized
  /// keys are a harmless no-op by default, so callers (SetupStage) can
  /// set parameters generically on whichever IFringeTracer they got --
  /// built-in class or DLL plugin (see DllFringeTracer) -- without
  /// knowing which concrete algorithm it is.
  virtual bool setParam(const std::string & /*key*/, const std::string & /*value*/) {
    return false;
  }

  /// Fringe order computed internally by the algorithm itself, one entry
  /// per line returned by the most recent extract(), in the same order.
  /// Empty (the default) means "this tracer doesn't compute order" --
  /// callers fall back to autoAssignFringeOrder(). Only
  /// ScanlineExtremumTracker (and its plugin equivalent) populates this.
  virtual std::vector<double> lastFringeOrders() const { return {}; }
};

}  // namespace digitqt::core::tracing
