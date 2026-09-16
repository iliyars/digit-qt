#pragma once

#include "core/NumberedFringeLine.h"

#include <functional>
#include <vector>

namespace digitqt::core {

/**
 * @brief Synthesizes one new fringe line just beyond the leftmost line
 * and one just beyond the rightmost line, continuing each side's step to
 * its nearest neighbor.
 *
 * Each new line is a rigid horizontal translation (x += step, y/width/
 * intensity unchanged) of the outermost line on that side -- exactly one
 * line per side per call, added unconditionally (even if it lands fully
 * outside the visible aperture -- PhaseReconstructor's per-row crossing
 * search only needs a segment spanning the row, not a visible one, so a
 * line just past the edge still closes the "aperture pole" gap this
 * feature exists to address). Calling this again after an earlier call
 * grows one step further out from there (so repeated calls add one more
 * line per side each time, not a fixed batch). A side is skipped
 * entirely if there are fewer than 2 lines, if the step to the nearest
 * neighbor is below a fixed epsilon (nothing meaningful to continue), or
 * if the current outermost line on that side already has no visible
 * points (an earlier call already pushed this side fully out of view --
 * calling again is a no-op there, not a further drift outward).
 *
 * New lines get orderIsManual = false (autoAssignFringeOrder(), called at
 * the end, numbers them normally by position) and syntheticFrontCount set
 * to their full point count (the whole line is synthetic -- see
 * NumberedFringeLine::isSynthetic()). Existing lines are returned
 * unchanged.
 */
std::vector<NumberedFringeLine> extrapolateFringesHorizontally(
    std::vector<NumberedFringeLine> lines, const std::function<bool(double, double)> &isVisible);

/**
 * @brief Extends every traced line's two endpoints past the aperture
 * edge, repeating each end's own fixed local step vector.
 *
 * For each line with >= 2 points, and for each end (front/back): the step
 * is computed ONCE, and then reused unchanged for every new point
 * (candidate = last + the same fixed step) -- NOT recomputed from the
 * growing synthetic tail each iteration. An earlier version did
 * recompute it every time, meaning to follow the line's local curvature
 * -- but for a genuinely curving line that compounds instead (each
 * synthetic point nudges the direction a little further than the last,
 * so the step magnitude drifts with every iteration), producing uneven
 * point spacing and wildly different extension lengths on ends with
 * different curvature.
 *
 * The step itself comes from the INTERIOR segment one point in from the
 * edge (points[1]-points[2], or the mirror image at the back), not from
 * that end's outermost pair -- S1's own tracer places the outermost
 * point exactly at the aperture-boundary crossing, not one regular step
 * from its neighbor, so for a fringe crossing a curved boundary at a
 * shallow angle that last real segment can be a small fraction of the
 * line's actual spacing; using it directly would faithfully repeat that
 * fluke-short segment for the whole synthetic tail (observed in
 * practice as wildly different, if individually uniform, extension
 * lengths concentrated on whichever end has shallower boundary
 * crossings). Growth still starts from the true outermost point, so
 * position is exact -- only direction/magnitude come from the unclipped
 * interior pair. Falls back to the outermost pair when a line has only
 * 2 points (nothing else to derive a step from). Once a
 * candidate lands outside the visible aperture, growth doesn't stop
 * immediately -- up to overshootMargin such past-the-edge points are
 * added (a deliberate margin, not just one), for the same reason
 * extrapolateFringesHorizontally() does the same: PhaseReconstructor
 * needs a segment spanning each row up to the true edge, not a visible
 * endpoint. An end is skipped entirely if its step magnitude is below a
 * fixed epsilon, or if it's already outside the visible aperture when
 * growth starts (an earlier call already overshot it by the full margin
 * -- calling again is a no-op, not a further drift). maxIterationsPerEnd
 * is only a safety cap against a degenerate isVisible.
 *
 * syntheticFrontCount/syntheticBackCount accumulate (not overwrite) how
 * many points were appended at each end -- calling this again after an
 * earlier extension continues growing from there, and
 * removeFringeExtensions() needs the running total to strip all of it
 * back off. Lines with < 2 points are passed through unchanged (nothing
 * to derive a step from).
 */
std::vector<NumberedFringeLine> extrapolateFringesVertically(
    std::vector<NumberedFringeLine> lines,
    const std::function<bool(double, double)> &isVisible, int maxIterationsPerEnd,
    int overshootMargin);

/**
 * @brief Strips every auto-generated point added by
 * extrapolateFringesHorizontally()/Vertically(), restoring the traced
 * lines to their pre-extension state.
 *
 * For each line, erases syntheticBackCount points off the back and
 * syntheticFrontCount off the front, then resets both counts to 0. A
 * line that had no synthetic points is left untouched. A line that ends
 * up with zero points left (a horizontally-extrapolated line is wholly
 * synthetic -- there's no real data underneath it) is dropped entirely
 * rather than kept empty.
 *
 * autoAssignFringeOrder() is re-run afterward, since dropping lines (or
 * shrinking others) can change the correct left-to-right numbering.
 */
std::vector<NumberedFringeLine> removeFringeExtensions(std::vector<NumberedFringeLine> lines);

}  // namespace digitqt::core
