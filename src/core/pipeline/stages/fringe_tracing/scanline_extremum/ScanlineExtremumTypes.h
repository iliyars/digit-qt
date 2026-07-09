#pragma once

#include <functional>
#include <vector>

namespace digitqt::core::tracing::scanline_extremum {

struct Point2d {
  double x{0.0};
  double y{0.0};
};

struct Limits {
  Point2d leftEdge;
  Point2d rightEdge;
};

/// Which kind of intensity extremum a fringe center corresponds to.
enum class ExtremumType { Red = 0, Black = 1 };

/// Fringe-center-selection mode, matching the original FC_MAX/FC_MIN/FC_MINMAX.
enum class FringeCenterMode { Max = 0, Min = 1, MinMax = 2 };

struct ExtremumPoint {
  Point2d position;
  double intensity{0.0};
  ExtremumType extremumType{ExtremumType::Red};
  bool assigned{false};
  double number{-1000.0};
  Limits window{{0.0, 0.0}, {0.0, 0.0}};

  /// Identifies which physical chain of matched points this belongs to.
  /// Inherited ONLY when this point is actually matched to a specific
  /// point in the adjacent scanline (see FringeConstructor); a fresh,
  /// globally-unique id is assigned when a point starts a new chain
  /// (main-scanline seed, or an unmatched point at a scanline edge).
  ///
  /// This is deliberately separate from `number`: number is a
  /// re-derivable display label (fringe order) that can legitimately
  /// collide between two unrelated chains (e.g. a fringe reappearing
  /// after an obstruction may coincidentally get the same order as an
  /// unrelated fringe elsewhere) -- grouping by number alone caused
  /// unrelated chains to be spliced into one polyline. Grouping by
  /// chainId instead makes that structurally impossible: two points
  /// only ever share a chainId if one was propagated from the other
  /// through an actual accepted match.
  int chainId{-1};
};

/// All detected extrema for one scanline (one row of the image).
struct Section {
  std::vector<ExtremumPoint> points;
  Limits limits{{0.0, 0.0}, {0.0, 0.0}};
  double averageStep{0.0};
};

/// One completed, numbered fringe centerline (output of FringeConstructor).
struct NumberedFringe {
  std::vector<Point2d> points;
  double number{0.0};
  int segmentIndex{-1};
};

/// Input bundle for RedCenterDetector::detectExtrema().
///
/// Unlike the original DigitizationInput, bitmapData is expected in
/// standard top-down row order (row 0 = top row), matching QImage --
/// the original expected bottom-up (Windows DIB-style) data and flipped
/// rows internally; see RedCenterDetector's port notes.
struct DigitizationInput {
  const unsigned char *bitmapData{nullptr};
  int imageWidth{0};
  int imageHeight{0};
  int bytesPerLine{0}; // stride; defaults to imageWidth if 0

  std::function<bool(int, int)> isVisible;

  FringeCenterMode fringeCenterAs{FringeCenterMode::MinMax};
  double fringeStep{1.0};
  double toleranceFactor{0.3};
};

} // namespace digitqt::core::tracing::scanline_extremum
