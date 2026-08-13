#include "FringeConstructor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace digitqt::core::tracing::scanline_extremum {

std::vector<NumberedFringe> FringeConstructor::constructFringes(
    std::vector<Section> &scanlines, int imageWidth, int imageHeight,
    const std::function<bool(int, int)> &isVisible,
    FringeCenterMode fringeCenterAs, double fringeStep,
    double toleranceFactor, bool hasInternalObstruction) {
  (void)imageWidth;
  (void)isVisible;

  if (scanlines.empty() || imageHeight <= 0 || fringeStep <= 0.0)
    return {};

  // Average step for each scanline; pick the true median. Matches
  // production legacy SelectFringeStep() (DigitInfoExt.hxx: SortDouble()
  // then SortedSteps[nS/2]) -- an earlier version of this port copied
  // legacy's own disabled/unused prototype, which skips the sort.
  std::vector<double> steps;
  for (auto &scanline : scanlines) {
    if (scanline.points.size() < 2)
      continue;

    double sumStep = 0.0;
    for (size_t i = 1; i < scanline.points.size(); ++i)
      sumStep +=
          scanline.points[i].position.x - scanline.points[i - 1].position.x;
    scanline.averageStep =
        sumStep / static_cast<double>(scanline.points.size() - 1);
    steps.push_back(scanline.averageStep);
  }
  if (steps.empty())
    return {};
  std::sort(steps.begin(), steps.end());
  const double medianStep = steps[steps.size() / 2];

  const int mainIdx = selectMainScanline(scanlines, medianStep, toleranceFactor,
                                         hasInternalObstruction);
  if (mainIdx < 0 || scanlines[static_cast<size_t>(mainIdx)].points.empty())
    return {};

  // Initialize main scanline with sequential numbers.
  double currentNumber = 0.0;
  int nextChainId = 0;
  for (auto &ne : scanlines[static_cast<size_t>(mainIdx)].points) {
    ne.number = currentNumber;
    ne.assigned = true;
    ne.chainId = nextChainId++;
    currentNumber += fringeStep;
  }

  const double tolerance = medianStep * toleranceFactor;

  // Persistent across the *entire* propagation (both passes), exactly
  // like legacy CreateNumLines()/ProcessSectionPropagation()'s minN/maxN/
  // leftX/rightX (DigitMode/CreateNumLines.hxx): never reset per row.
  // This is what makes every newly-invented "new fringe" number globally
  // unique, and lets a row tell a genuine edge fringe (beyond anything
  // seen so far) apart from an interior orphan. leftX/rightX start
  // unbounded (matching legacy's INT_MAX/INT_MIN) rather than seeded from
  // the main scanline -- the very first unmatched point processed always
  // counts as "new", same quirk as the original.
  double minNum = scanlines[static_cast<size_t>(mainIdx)].points.front().number;
  double maxNum = scanlines[static_cast<size_t>(mainIdx)].points.back().number;
  double leftX = std::numeric_limits<double>::max();
  double rightX = std::numeric_limits<double>::lowest();

  // searchBound: how far back (toward already-numbered territory) a row
  // may look for a match, with NO fixed row-gap cap -- matches legacy
  // SelectNumber(), which searches every remaining section until it finds
  // one within tolerance or runs out. For the upward pass that's bounded
  // by mainIdx (nothing beyond it is numbered yet); for the downward
  // pass it's bounded by 0, since by then the *entire* upward pass has
  // already completed -- legacy relies on exactly this (its two passes
  // run sequentially, sharing the same Sections array).
  auto propagate = [&](int from, int to, int step, int searchBound) {
    for (int y = from; step > 0 ? y < to : y > to; y += step) {
      auto &currentScanline = scanlines[static_cast<size_t>(y)];

      for (size_t curIdx = 0; curIdx < currentScanline.points.size();
           ++curIdx) {
        auto &ne = currentScanline.points[curIdx];

        int matchIdx = -1;
        const Section *matchedScanline = nullptr;

        for (int adjacentY = y - step;
             step > 0 ? adjacentY >= searchBound : adjacentY <= searchBound;
             adjacentY -= step) {
          const auto &candidate = scanlines[static_cast<size_t>(adjacentY)];
          const int idx = findMatchingExtremum(ne, ne.position.x, candidate,
                                               tolerance, fringeCenterAs);
          if (idx < 0)
            continue;  // nothing at this distance either -- try further back

          if (wouldCross(static_cast<int>(curIdx), idx, currentScanline.points,
                         candidate.points))
            continue;  // would cross an existing chain segment

          const double proposedNumber =
              candidate.points[static_cast<size_t>(idx)].number;
          if (wouldViolateAlternation(ne.extremumType, proposedNumber,
                                      candidate, fringeCenterAs, fringeStep))
            continue;  // would break Red/Black alternation (MinMax mode)

          matchIdx = idx;
          matchedScanline = &candidate;
          break;
        }

        if (matchIdx >= 0) {
          const auto &matched =
              matchedScanline->points[static_cast<size_t>(matchIdx)];
          ne.number = matched.number;
          ne.assigned = true;
          ne.chainId = matched.chainId;
          leftX = std::min(leftX, ne.position.x);
          rightX = std::max(rightX, ne.position.x);
          minNum = std::min(minNum, ne.number);
          maxNum = std::max(maxNum, ne.number);
        }
      }

      // Unmatched points only get a new number if they're genuinely
      // beyond the spatial range numbered so far (a real edge fringe);
      // otherwise they're an orphan -- an interior fringe that failed to
      // match -- and are left unassigned/dropped, same as legacy's
      // unhandled "orphan case" in ProcessSectionPropagation.
      for (auto &ne : currentScanline.points) {
        if (ne.assigned)
          continue;

        if (ne.position.x < leftX) {
          ne.number = minNum - fringeStep;
          ne.assigned = true;
          ne.chainId = nextChainId++;
          leftX = ne.position.x;
          minNum = ne.number;
        } else if (ne.position.x > rightX) {
          ne.number = maxNum + fringeStep;
          ne.assigned = true;
          ne.chainId = nextChainId++;
          rightX = ne.position.x;
          maxNum = ne.number;
        }
        // else: orphan, dropped.
      }
    }
  };

  propagate(mainIdx - 1, -1, -1, mainIdx);      // upward
  propagate(mainIdx + 1, imageHeight, 1, 0);    // downward

  return convertToFringes(scanlines);
}

int FringeConstructor::selectMainScanline(const std::vector<Section> &scanlines,
                                          double fringeStep,
                                          double toleranceFactor,
                                          bool hasInternalObstruction) {
  const double minStep = fringeStep * (1.0 - toleranceFactor);
  const double maxStep = fringeStep * (1.0 + toleranceFactor);

  int maxFringeCount1 = -1;
  int maxFringeCount2 = -1;
  int idx1 = -1;
  int idx2 = -1;

  for (size_t i = 0; i < scanlines.size(); ++i) {
    const auto &sl = scanlines[i];
    const int count = static_cast<int>(sl.points.size());
    const double step = sl.averageStep;

    if (count > maxFringeCount1 &&
        (step < 0 || (step >= minStep && step <= maxStep))) {
      maxFringeCount1 = count;
      idx1 = static_cast<int>(i);
    }
  }

  for (int i = static_cast<int>(scanlines.size()) - 1; i >= 0; --i) {
    const auto &sl = scanlines[static_cast<size_t>(i)];
    const int count = static_cast<int>(sl.points.size());
    const double step = sl.averageStep;

    if (count >= maxFringeCount2 &&
        (step < 0 || (step >= minStep && step <= maxStep)) &&
        count == maxFringeCount1) {
      maxFringeCount2 = count;
      idx2 = i;
    }
  }

  // Matches production legacy SelectMainSection() (DigitInfoExt.hxx:79):
  // only split the difference between the forward/backward candidates
  // when there's no internal obstruction. With one, the aperture isn't
  // symmetric around it, so the two candidates aren't interchangeable --
  // take the forward-scan one instead, same as legacy's else-branch.
  if (idx1 >= 0 && idx2 >= 0 && idx1 != idx2 &&
      maxFringeCount1 == maxFringeCount2 && !hasInternalObstruction)
    return idx1 + (idx2 - idx1) / 2;

  return idx1 >= 0 ? idx1 : idx2;
}

int FringeConstructor::findMatchingExtremum(
    const ExtremumPoint &extremum, double currentX,
    const Section &adjacentScanline, double tolerance,
    FringeCenterMode /*fringeCenterAs*/) {
  int bestMatch = -1;
  double bestDistance = std::numeric_limits<double>::max();

  for (size_t i = 0; i < adjacentScanline.points.size(); ++i) {
    const auto &adjExt = adjacentScanline.points[i];
    if (!adjExt.assigned)
      continue;

    const double distance = std::abs(currentX - adjExt.position.x);
    if (distance > tolerance)
      continue;

    if (extremum.extremumType != adjExt.extremumType)
      continue;  // type consistency

    if (distance < bestDistance) {
      bestDistance = distance;
      bestMatch = static_cast<int>(i);
    }
  }

  return bestMatch;
}

bool FringeConstructor::wouldCross(
    int currentIdx, int proposedIdx,
    const std::vector<ExtremumPoint> &currentExtrema,
    const std::vector<ExtremumPoint> &adjacentExtrema) {
  if (currentIdx < 0 || currentIdx >= static_cast<int>(currentExtrema.size()))
    return false;
  if (proposedIdx < 0 ||
      proposedIdx >= static_cast<int>(adjacentExtrema.size()))
    return false;

  const auto &current = currentExtrema[static_cast<size_t>(currentIdx)];
  const auto &proposed = adjacentExtrema[static_cast<size_t>(proposedIdx)];

  const double x1 = current.position.x, y1 = current.position.y;
  const double x2 = proposed.position.x, y2 = proposed.position.y;

  for (size_t i = 0; i < currentExtrema.size(); ++i) {
    if (static_cast<int>(i) == currentIdx || !currentExtrema[i].assigned)
      continue;

    // Find the matching extremum in the adjacent scanline belonging
    // to the SAME chain (not just the same display number -- two
    // unrelated chains may legitimately share a number).
    const int targetChainId = currentExtrema[i].chainId;
    for (size_t j = 0; j < adjacentExtrema.size(); ++j) {
      if (static_cast<int>(j) == proposedIdx || !adjacentExtrema[j].assigned)
        continue;
      if (adjacentExtrema[j].chainId != targetChainId)
        continue;

      const double x3 = currentExtrema[i].position.x,
                   y3 = currentExtrema[i].position.y;
      const double x4 = adjacentExtrema[j].position.x,
                   y4 = adjacentExtrema[j].position.y;

      if (segmentsIntersect(x1, y1, x2, y2, x3, y3, x4, y4))
        return true;
    }
  }

  return false;
}

bool FringeConstructor::segmentsIntersect(double x1, double y1, double x2,
                                          double y2, double x3, double y3,
                                          double x4, double y4) {
  auto crossProduct = [](double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
  };

  const double dx1 = x2 - x1, dy1 = y2 - y1;
  const double dx2 = x4 - x3, dy2 = y4 - y3;
  const double dx3 = x3 - x1, dy3 = y3 - y1;

  const double cross1 = crossProduct(dx1, dy1, dx2, dy2);
  if (std::abs(cross1) < 1e-10)
    return false;  // parallel/collinear -> treat as non-crossing

  const double t1 = crossProduct(dx3, dy3, dx2, dy2) / cross1;
  const double t2 = crossProduct(dx3, dy3, dx1, dy1) / cross1;

  return (t1 > 0.0 && t1 < 1.0) && (t2 > 0.0 && t2 < 1.0);
}

bool FringeConstructor::wouldViolateAlternation(ExtremumType currentType,
                                                double proposedNumber,
                                                const Section &adjacentScanline,
                                                FringeCenterMode fringeCenterAs,
                                                double fringeStep) {
  if (fringeCenterAs != FringeCenterMode::MinMax)
    return false;

  const double prevNumber = proposedNumber - fringeStep;
  const double nextNumber = proposedNumber + fringeStep;

  bool foundPrevSameType = false;
  bool foundNextSameType = false;

  for (const auto &ne : adjacentScanline.points) {
    if (!ne.assigned)
      continue;

    if (std::abs(ne.number - prevNumber) < fringeStep * 0.1 &&
        ne.extremumType == currentType)
      foundPrevSameType = true;

    if (std::abs(ne.number - nextNumber) < fringeStep * 0.1 &&
        ne.extremumType == currentType)
      foundNextSameType = true;
  }

  return foundPrevSameType || foundNextSameType;
}

std::vector<NumberedFringe> FringeConstructor::convertToFringes(
    const std::vector<Section> &scanlines) {
  // Grouping by chainId (not by .number -- see ExtremumPoint::chainId's
  // doc comment) is what actually guarantees each resulting polyline is
  // one physically continuous chain of matched points, never two
  // unrelated fringe segments spliced together by a numbering collision.
  std::map<int, std::vector<Point2d>> chainPoints;
  std::map<int, double> chainNumber;

  for (const auto &scanline : scanlines) {
    for (const auto &ne : scanline.points) {
      if (!ne.assigned)
        continue;
      chainPoints[ne.chainId].push_back(ne.position);
      chainNumber.try_emplace(ne.chainId,
                              ne.number);  // label from the chain's first point
    }
  }

  std::vector<NumberedFringe> result;
  result.reserve(chainPoints.size());
  for (auto &pair : chainPoints) {
    NumberedFringe fringe;
    fringe.number = chainNumber[pair.first];
    fringe.points = std::move(pair.second);
    fringe.segmentIndex = static_cast<int>(result.size());
    result.push_back(std::move(fringe));
  }

  return result;
}

}  // namespace digitqt::core::tracing::scanline_extremum
