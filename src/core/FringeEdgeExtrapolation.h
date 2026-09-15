#pragma once

#include "core/NumberedFringeLine.h"

#include <functional>
#include <vector>

namespace digitqt::core {

/**
 * @brief Synthesizes new fringe lines beyond the leftmost/rightmost
 * traced line, continuing the step to its nearest neighbor, past the
 * aperture edge.
 *
 * Each new line is a rigid horizontal translation (x += k * step, y/width/
 * intensity unchanged) of the outermost line on that side, for
 * k = 1, 2, ... -- added as long as at least one of its points is still
 * visible. Once a candidate has no visible points left, growth doesn't
 * stop immediately: PhaseReconstructor's per-row crossing search only
 * needs a segment spanning the row, not a visible one, so stopping right
 * at the last visible line would leave rows between it and the true
 * (continuous) aperture edge without a bracketing crossing on that side
 * -- the same "aperture pole" gap this feature exists to close. Instead,
 * up to overshootMargin such past-the-edge lines are added (a deliberate
 * margin, not just one), then that side stops. A side is skipped entirely
 * if there are fewer than 2 lines, if the step to the nearest neighbor is
 * below a fixed epsilon (nothing meaningful to continue), or if the seed
 * line itself already has no visible points (an earlier call already
 * overshot on this side by the full margin -- calling again is a no-op,
 * not a further drift outward). maxIterationsPerSide is only a safety
 * cap against a degenerate isVisible.
 *
 * New lines get orderIsManual = false (autoAssignFringeOrder(), called at
 * the end, numbers them normally by position) and syntheticFrontCount set
 * to their full point count (the whole line is synthetic -- see
 * NumberedFringeLine::isSynthetic()). Existing lines are returned
 * unchanged.
 */
std::vector<NumberedFringeLine> extrapolateFringesHorizontally(
    std::vector<NumberedFringeLine> lines,
    const std::function<bool(double, double)> &isVisible, int maxIterationsPerSide,
    int overshootMargin);

/**
 * @brief Extends every traced line's two endpoints past the aperture
 * edge, repeating each end's own local step vector.
 *
 * For each line with >= 2 points, and for each end (front/back): the step
 * is the vector between that end's two nearest points. New points are
 * appended one at a time (candidate = last + step), recomputing the step
 * from the two most-recent points before every next iteration so the
 * continuation follows the line's local curvature rather than a fixed
 * direction. Once a candidate lands outside the visible aperture, growth
 * doesn't stop immediately -- up to overshootMargin such past-the-edge
 * points are added (a deliberate margin, not just one), for the same
 * reason extrapolateFringesHorizontally() does the same: PhaseReconstructor
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
