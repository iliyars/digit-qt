#include "FringeEdgeExtrapolation.h"

#include "core/FringeOrdering.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace digitqt::core {

namespace {

// Kept as a local copy rather than exported from FringeOrdering.cpp --
// that file already keeps its own averageX() private, same convention.
double meanX(const tracing::TracedLine &points) {
  if (points.empty())
    return 0.0;
  double sum = 0.0;
  for (const auto &p : points)
    sum += p.x;
  return sum / static_cast<double>(points.size());
}

// Below this, a step is degenerate (coincident lines/points) -- nothing
// meaningful to continue, and dividing by it would be numerically unsafe.
constexpr double kMinStepMagnitude = 0.5;

bool anyPointVisible(const tracing::TracedLine &points,
                     const std::function<bool(double, double)> &isVisible) {
  for (const auto &p : points) {
    if (isVisible(p.x, p.y))
      return true;
  }
  return false;
}

}  // namespace

std::vector<NumberedFringeLine> extrapolateFringesHorizontally(
    std::vector<NumberedFringeLine> lines,
    const std::function<bool(double, double)> &isVisible) {
  if (lines.size() < 2)
    return lines;

  std::vector<size_t> sorted(lines.size());
  std::iota(sorted.begin(), sorted.end(), size_t{0});
  std::sort(sorted.begin(), sorted.end(), [&lines](size_t a, size_t b) {
    return meanX(lines[a].points) < meanX(lines[b].points);
  });

  auto growSide = [&](size_t seedIdx, double step) {
    if (std::abs(step) < kMinStepMagnitude)
      return;
    // Idempotency guard: if the seed itself no longer has any visible
    // point, an earlier call already pushed this side fully out of view
    // -- nothing left to do (otherwise a repeated call would keep
    // drifting further out forever).
    if (!anyPointVisible(lines[seedIdx].points, isVisible))
      return;

    // Exactly one new line per call, even if it lands fully outside the
    // visible aperture: PhaseReconstructor's per-row crossing search
    // doesn't care about a line's own visibility, only about a segment
    // spanning the row -- stopping right at the last visible line would
    // leave rows between it and the true (continuous) boundary without a
    // bracketing crossing on that side, exactly the "aperture pole" gap
    // this feature exists to close. Calling this again grows one step
    // further out from there.
    tracing::TracedLine candidatePoints = lines[seedIdx].points;
    for (auto &p : candidatePoints)
      p.x += step;

    NumberedFringeLine newLine;
    newLine.orderIsManual = false;
    // The whole line is synthetic (there's no real data to attach a
    // continuation to -- it IS the continuation). Recorded as a front
    // count purely by convention; removeFringeExtensions() strips
    // front+back points regardless of which end they're nominally on,
    // so a wholly-synthetic line always ends up emptied and dropped.
    newLine.syntheticFrontCount = static_cast<int>(candidatePoints.size());
    newLine.points = std::move(candidatePoints);
    lines.push_back(std::move(newLine));
  };

  const size_t leftIdx = sorted.front();
  const size_t leftNeighborIdx = sorted[1];
  growSide(leftIdx, meanX(lines[leftIdx].points) - meanX(lines[leftNeighborIdx].points));

  const size_t rightIdx = sorted.back();
  const size_t rightNeighborIdx = sorted[sorted.size() - 2];
  growSide(rightIdx, meanX(lines[rightIdx].points) - meanX(lines[rightNeighborIdx].points));

  autoAssignFringeOrder(lines);
  return lines;
}

namespace {

// Grows one end of `points` (front if atStart, else back) in place,
// repeating the step to that end's nearest neighbor. Deliberately
// overshoots the visible aperture by up to overshootMargin points once it
// gets there (same reasoning as growSide() in
// extrapolateFringesHorizontally() -- a crossing needs a segment that
// spans the row, and stopping exactly at the last visible point can leave
// rows right up to the true boundary without one). Returns how many
// points were appended (0 if none).
int growEnd(tracing::TracedLine &points, bool atStart,
           const std::function<bool(double, double)> &isVisible, int maxIterations,
           int overshootMargin) {
  // Idempotency guard: if this end is already outside the visible
  // aperture (a previous call's deliberate overshoot, by the full
  // margin), there's nothing left to do -- otherwise a repeated call
  // would keep drifting further out forever.
  const auto &initialLast = atStart ? points.front() : points.back();
  if (!isVisible(initialLast.x, initialLast.y))
    return 0;

  // Fixed step, computed ONCE -- not recomputed from the growing
  // synthetic tail on every iteration. Recomputing from the last two
  // points each time (an earlier version of this function did that, to
  // "follow the line's local curvature") backfires on a genuinely
  // curving line: each new synthetic point nudges the direction a
  // little further than the one before it, so the step compounds
  // instead of tracking the curve -- producing uneven point spacing and
  // wildly different extension lengths between lines with different
  // curvature.
  //
  // Deliberately NOT derived from this end's outermost two points
  // (points.front()/[1] or points.back()/[size-2]): S1's own tracer
  // places that outermost point AT the exact aperture-boundary
  // crossing, not one regular step away from its neighbor -- for a
  // fringe that crosses the (curved) boundary at a shallow angle, that
  // last real segment can be a small fraction of the line's actual
  // point spacing. Using it as the extrapolation step would faithfully
  // repeat that fluke-short (or fluke-long) segment for every synthetic
  // point instead of the line's real, regular spacing -- observed in
  // practice as wildly different (but individually uniform) extension
  // lengths between lines, concentrated on whichever end of the
  // aperture happens to have shallower boundary crossings. Instead, the
  // step comes from the interior segment just before that (skipping the
  // possibly-clipped outermost point on both ends), while growth still
  // starts from the true outermost point -- so position is exact, only
  // direction/magnitude are taken from an unclipped pair. Falls back to
  // the outermost pair when there aren't 3 points to spare.
  double stepX, stepY;
  if (points.size() >= 3) {
    // Interior segment, skipping the possibly boundary-clipped outermost
    // point entirely: for atStart, points[1]-points[2] (direction
    // continuing past points[1], away from the interior); mirrored for
    // the back end.
    const auto &near = atStart ? points[1] : points[points.size() - 2];
    const auto &far = atStart ? points[2] : points[points.size() - 3];
    stepX = near.x - far.x;
    stepY = near.y - far.y;
  } else {
    // Only 2 points total -- no interior pair to fall back on, so use
    // the outermost pair as-is (best available information).
    const auto &neighbor = atStart ? points[1] : points[points.size() - 2];
    stepX = initialLast.x - neighbor.x;
    stepY = initialLast.y - neighbor.y;
  }
  if (std::hypot(stepX, stepY) < kMinStepMagnitude)
    return 0;

  int addedCount = 0;
  int overshootCount = 0;

  for (int i = 0; i < maxIterations; ++i) {
    const auto &last = atStart ? points.front() : points.back();

    tracing::TracedPoint candidate;
    candidate.x = last.x + stepX;
    candidate.y = last.y + stepY;
    candidate.width = last.width;
    candidate.intensity = last.intensity;

    if (atStart)
      points.insert(points.begin(), candidate);
    else
      points.push_back(candidate);
    ++addedCount;

    if (!isVisible(candidate.x, candidate.y)) {
      ++overshootCount;
      if (overshootCount >= overshootMargin)
        break;
    }
  }

  return addedCount;
}

}  // namespace

std::vector<NumberedFringeLine> extrapolateFringesVertically(
    std::vector<NumberedFringeLine> lines,
    const std::function<bool(double, double)> &isVisible, int maxIterationsPerEnd,
    int overshootMargin) {
  for (auto &line : lines) {
    if (line.points.size() < 2)
      continue;

    // Accumulate rather than overwrite: a second call (e.g. after the
    // margin was raised) grows further from wherever the previous call
    // left off, and removeFringeExtensions() needs the total count of
    // synthetic points at each end to strip them all back off.
    line.syntheticFrontCount +=
        growEnd(line.points, /*atStart=*/true, isVisible, maxIterationsPerEnd, overshootMargin);
    line.syntheticBackCount +=
        growEnd(line.points, /*atStart=*/false, isVisible, maxIterationsPerEnd, overshootMargin);
  }

  autoAssignFringeOrder(lines);
  return lines;
}

std::vector<NumberedFringeLine> removeFringeExtensions(std::vector<NumberedFringeLine> lines) {
  std::vector<NumberedFringeLine> result;
  result.reserve(lines.size());

  for (auto &line : lines) {
    if (!line.isSynthetic()) {
      result.push_back(std::move(line));
      continue;
    }

    // Clamp defensively: the counts are maintained internally by
    // extrapolateFringes{Horizontally,Vertically}() and should never
    // exceed the point count, but a corrupt/hand-edited count must not
    // underflow the erase below.
    const int frontCount = std::clamp(line.syntheticFrontCount, 0,
                                      static_cast<int>(line.points.size()));
    const int backCount = std::clamp(line.syntheticBackCount, 0,
                                     static_cast<int>(line.points.size()) - frontCount);

    auto &points = line.points;
    if (backCount > 0)
      points.erase(points.end() - backCount, points.end());
    if (frontCount > 0)
      points.erase(points.begin(), points.begin() + frontCount);

    if (points.empty())
      continue;  // wholly-synthetic line (horizontal extension) -- drop it

    line.syntheticFrontCount = 0;
    line.syntheticBackCount = 0;
    result.push_back(std::move(line));
  }

  autoAssignFringeOrder(result);
  return result;
}

}  // namespace digitqt::core
