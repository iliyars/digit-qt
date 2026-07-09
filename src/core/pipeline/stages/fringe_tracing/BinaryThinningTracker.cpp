/**
 * @file BinaryThinningTracker.cpp
 * @brief Fringe Binary Method (FBM): adaptive threshold + Zhang-Suen
 * skeletonization, ported from the uploaded InterferometryApp project's
 * CFringeSkeletonizer.
 *
 * Numeric logic (thresholds, thinning rules, pruning, linking heuristics)
 * is unchanged from the original. Adaptations: QImage + isVisible(x,y)
 * instead of cv::Mat + CEllipseBoundary; development-only
 * cv::imwrite()/std::cout debug statements removed.
 */
#include "BinaryThinningTracker.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace digitqt::core::tracing {

bool BinaryThinningTracker::initialize(
    const QImage &image, std::function<bool(int, int)> isVisible) {
  m_lastError.clear();

  if (image.isNull()) {
    m_lastError = QStringLiteral("Empty image");
    return false;
  }

  const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

  // Wrap QImage's buffer, then clone -- QImage's underlying data is
  // reference-counted/shared and we need a copy we own independently.
  const cv::Mat wrapped(gray.height(), gray.width(), CV_8UC1,
                        const_cast<uchar *>(gray.constBits()),
                        static_cast<size_t>(gray.bytesPerLine()));
  m_image = wrapped.clone();

  m_isVisible = std::move(isVisible);
  return true;
}

std::vector<TracedLine>
BinaryThinningTracker::extract(const std::vector<SeedPoint> & /*seeds*/) {
  // Global algorithm -- seeds are not used, see IFringeTracer's contract.
  if (m_image.empty()) {
    m_lastError =
        QStringLiteral("Tracer not initialized. Call initialize() first.");
    return {};
  }

  if (!buildBinary())
    return {};

  skeletonize(m_binary, m_skeleton);

  cv::bitwise_and(m_skeleton, m_mask, m_skeleton);
  pruneSkeleton(m_skeleton, m_params.pruneLength);

  if (m_params.computeWidth)
    cv::distanceTransform(m_binary, m_distMap, cv::DIST_L2, 3);

  auto lines = extractPolylines();
  linkBrokenLines(lines);
  filterByCurvature(lines);
  if (m_params.smoothLines)
    for (auto &line : lines)
      smoothLine(line);

  if (lines.empty()) {
    m_lastError =
        QStringLiteral("No fringes detected -- check the aperture, contrast, "
                       "and the adaptiveC/pruneLength parameters");
  }

  return lines;
}

bool BinaryThinningTracker::buildBinary() {
  // 1. Smoothing.
  cv::Mat blurred;
  if (m_params.gaussianKernel >= 3 && m_params.gaussianKernel % 2 == 1) {
    cv::GaussianBlur(m_image, blurred,
                     cv::Size(m_params.gaussianKernel, m_params.gaussianKernel),
                     0);
  } else {
    blurred = m_image.clone();
  }

  // 2. Aperture mask, from our isVisible predicate.
  m_mask = cv::Mat::zeros(m_image.size(), CV_8UC1);
  if (m_isVisible) {
    for (int y = 0; y < m_image.rows; ++y)
      for (int x = 0; x < m_image.cols; ++x)
        if (m_isVisible(x, y))
          m_mask.at<uchar>(y, x) = 255;
  } else {
    m_mask.setTo(255);
  }

  // 3. Adaptive threshold.
  int blockSize = m_params.adaptiveBlockSize;
  if (blockSize % 2 == 0)
    ++blockSize;
  if (blockSize < 3)
    blockSize = 3;

  cv::adaptiveThreshold(blurred, m_binary, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                        cv::THRESH_BINARY, blockSize, m_params.adaptiveC);

  // 4. Apply mask.
  cv::bitwise_and(m_binary, m_mask, m_binary);

  // 5. Morphological cleanup.
  const int k = m_params.morphKernelSize;
  if (k >= 3 && k % 2 == 1) {
    const cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::morphologyEx(m_binary, m_binary, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(m_binary, m_binary, cv::MORPH_CLOSE, kernel);
  }

  // 6. Re-apply the mask -- morphology can bleed pixels across its edge.
  cv::bitwise_and(m_binary, m_mask, m_binary);

  return true;
}

void BinaryThinningTracker::skeletonize(const cv::Mat &binary,
                                        cv::Mat &skeleton) const {
  skeleton = binary.clone();
  skeleton /= 255;

  cv::Mat marker = cv::Mat::zeros(skeleton.size(), CV_8UC1);
  bool changed = true;

  auto P = [&](int y, int x, int dy, int dx) -> int {
    const int ny = y + dy, nx = x + dx;
    if (ny < 0 || ny >= skeleton.rows || nx < 0 || nx >= skeleton.cols)
      return 0;
    return skeleton.at<uchar>(ny, nx);
  };

  while (changed) {
    changed = false;

    for (int sub = 0; sub < 2; ++sub) {
      marker.setTo(0);

      for (int y = 1; y < skeleton.rows - 1; ++y) {
        for (int x = 1; x < skeleton.cols - 1; ++x) {
          if (skeleton.at<uchar>(y, x) == 0)
            continue;

          const int p2 = P(y, x, -1, 0);
          const int p3 = P(y, x, -1, +1);
          const int p4 = P(y, x, 0, +1);
          const int p5 = P(y, x, +1, +1);
          const int p6 = P(y, x, +1, 0);
          const int p7 = P(y, x, +1, -1);
          const int p8 = P(y, x, 0, -1);
          const int p9 = P(y, x, -1, -1);

          const int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
          if (B < 2 || B > 6)
            continue;

          const int A = (p2 == 0 && p3 == 1) + (p3 == 0 && p4 == 1) +
                        (p4 == 0 && p5 == 1) + (p5 == 0 && p6 == 1) +
                        (p6 == 0 && p7 == 1) + (p7 == 0 && p8 == 1) +
                        (p8 == 0 && p9 == 1) + (p9 == 0 && p2 == 1);
          if (A != 1)
            continue;

          if (sub == 0) {
            if (p2 * p4 * p6 != 0)
              continue;
            if (p4 * p6 * p8 != 0)
              continue;
          } else {
            if (p2 * p4 * p8 != 0)
              continue;
            if (p2 * p6 * p8 != 0)
              continue;
          }

          marker.at<uchar>(y, x) = 1;
        }
      }

      for (int y = 0; y < skeleton.rows; ++y)
        for (int x = 0; x < skeleton.cols; ++x)
          if (marker.at<uchar>(y, x)) {
            skeleton.at<uchar>(y, x) = 0;
            changed = true;
          }
    }
  }

  skeleton *= 255;
}

int BinaryThinningTracker::countNeighbors(const cv::Mat &skel, int x,
                                          int y) const {
  int count = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0)
        continue;
      const int nx = x + dx, ny = y + dy;
      if (nx < 0 || nx >= skel.cols || ny < 0 || ny >= skel.rows)
        continue;
      if (skel.at<uchar>(ny, nx) > 0)
        ++count;
    }
  return count;
}

TracedLine BinaryThinningTracker::traceBranch(const cv::Mat &skel,
                                              cv::Mat &visited, int sx,
                                              int sy) const {
  TracedLine branch;
  static const int dx8[8] = {0, 1, 0, -1, 1, 1, -1, -1};
  static const int dy8[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

  int x = sx, y = sy;
  int prevDx = 0, prevDy = 0;

  while (true) {
    visited.at<uchar>(y, x) = 1;

    TracedPoint pt;
    pt.x = x;
    pt.y = y;
    pt.intensity = static_cast<float>(m_image.at<uchar>(y, x));
    pt.width = (m_params.computeWidth && !m_distMap.empty())
                   ? 2.0f * m_distMap.at<float>(y, x)
                   : 0.0f;
    branch.push_back(pt);

    struct Candidate {
      int x, y, dx, dy;
    };
    Candidate candidates[8];
    int numCandidates = 0;

    for (int i = 0; i < 8; ++i) {
      const int nx = x + dx8[i];
      const int ny = y + dy8[i];
      if (nx < 0 || nx >= skel.cols || ny < 0 || ny >= skel.rows)
        continue;
      if (skel.at<uchar>(ny, nx) == 0)
        continue;
      if (visited.at<uchar>(ny, nx))
        continue;

      candidates[numCandidates++] = {nx, ny, dx8[i], dy8[i]};
    }

    if (numCandidates == 0)
      break; // dead end -- line's end

    // One candidate: just go there. Several: pick the one closest in
    // direction to the previous step (keeps the branch smooth through
    // junctions instead of jumping erratically).
    int bestIdx = 0;
    if (numCandidates > 1 && (prevDx != 0 || prevDy != 0)) {
      const float prevLen =
          std::sqrt(static_cast<float>(prevDx * prevDx + prevDy * prevDy));
      float bestScore = -1e9f;
      for (int i = 0; i < numCandidates; ++i) {
        const float cdLen =
            std::sqrt(static_cast<float>(candidates[i].dx * candidates[i].dx +
                                         candidates[i].dy * candidates[i].dy));
        const float dot = static_cast<float>(prevDx * candidates[i].dx +
                                             prevDy * candidates[i].dy);
        const float score = dot / (prevLen * cdLen); // cos(angle)
        if (score > bestScore) {
          bestScore = score;
          bestIdx = i;
        }
      }
    }

    prevDx = candidates[bestIdx].dx;
    prevDy = candidates[bestIdx].dy;
    x = candidates[bestIdx].x;
    y = candidates[bestIdx].y;
  }

  return branch;
}

std::vector<TracedLine> BinaryThinningTracker::extractPolylines() const {
  std::vector<TracedLine> lines;
  cv::Mat visited = cv::Mat::zeros(m_skeleton.size(), CV_8UC1);

  // Phase 1: from endpoints (skeleton pixels with exactly 1 neighbor) --
  // walking from there gives the fullest line, through any junctions.
  for (int y = 0; y < m_skeleton.rows; ++y) {
    for (int x = 0; x < m_skeleton.cols; ++x) {
      if (m_skeleton.at<uchar>(y, x) == 0)
        continue;
      if (visited.at<uchar>(y, x))
        continue;
      if (countNeighbors(m_skeleton, x, y) != 1)
        continue;

      auto branch = traceBranch(m_skeleton, visited, x, y);
      if (static_cast<int>(branch.size()) >= m_params.minLineLength)
        lines.push_back(std::move(branch));
    }
  }

  // Phase 2: remaining unvisited pixels -- closed contours (no
  // endpoints) and fragments not reachable from any endpoint.
  for (int y = 0; y < m_skeleton.rows; ++y) {
    for (int x = 0; x < m_skeleton.cols; ++x) {
      if (m_skeleton.at<uchar>(y, x) == 0)
        continue;
      if (visited.at<uchar>(y, x))
        continue;

      auto branch = traceBranch(m_skeleton, visited, x, y);
      if (static_cast<int>(branch.size()) >= m_params.minLineLength)
        lines.push_back(std::move(branch));
    }
  }

  return lines;
}

void BinaryThinningTracker::smoothLine(TracedLine &line) const {
  const int w = m_params.smoothWindow;
  if (static_cast<int>(line.size()) < w * 2 + 1)
    return;

  TracedLine sm = line;
  for (int i = w; i < static_cast<int>(line.size()) - w; ++i) {
    double sx = 0.0, sy = 0.0;
    for (int j = -w; j <= w; ++j) {
      sx += line[static_cast<size_t>(i + j)].x;
      sy += line[static_cast<size_t>(i + j)].y;
    }
    sm[static_cast<size_t>(i)].x = sx / (2 * w + 1);
    sm[static_cast<size_t>(i)].y = sy / (2 * w + 1);
  }
  line = std::move(sm);
}

void BinaryThinningTracker::pruneSkeleton(cv::Mat &skel,
                                          int maxBranchLength) const {
  if (maxBranchLength <= 0)
    return;

  static const int dx8[8] = {0, 1, 0, -1, 1, 1, -1, -1};
  static const int dy8[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

  bool changed = true;
  int iterations = 0;
  const int maxIterations = 20; // guards against an infinite loop

  while (changed && iterations++ < maxIterations) {
    changed = false;

    std::vector<std::pair<int, int>> endpoints;
    for (int y = 0; y < skel.rows; ++y)
      for (int x = 0; x < skel.cols; ++x) {
        if (skel.at<uchar>(y, x) == 0)
          continue;
        if (countNeighbors(skel, x, y) == 1)
          endpoints.emplace_back(x, y);
      }

    for (auto &ep : endpoints) {
      int x = ep.first, y = ep.second;

      if (skel.at<uchar>(y, x) == 0)
        continue; // already erased earlier this pass

      std::vector<std::pair<int, int>> branch;
      branch.emplace_back(x, y);

      int curX = x, curY = y;
      int prevX = -1, prevY = -1;

      while (static_cast<int>(branch.size()) <= maxBranchLength) {
        int nextX = -1, nextY = -1;
        for (int i = 0; i < 8; ++i) {
          const int nx = curX + dx8[i];
          const int ny = curY + dy8[i];
          if (nx < 0 || nx >= skel.cols || ny < 0 || ny >= skel.rows)
            continue;
          if (skel.at<uchar>(ny, nx) == 0)
            continue;
          if (nx == prevX && ny == prevY)
            continue;

          nextX = nx;
          nextY = ny;
          break;
        }

        if (nextX < 0)
          break; // dead end

        if (countNeighbors(skel, nextX, nextY) >= 3)
          break; // reached a junction -- branch ends here

        prevX = curX;
        prevY = curY;
        curX = nextX;
        curY = nextY;
        branch.emplace_back(curX, curY);
      }

      if (static_cast<int>(branch.size()) < maxBranchLength) {
        for (auto &p : branch)
          skel.at<uchar>(p.second, p.first) = 0;
        changed = true;
      }
    }
  }
}

void BinaryThinningTracker::linkBrokenLines(
    std::vector<TracedLine> &lines) const {
  if (m_params.linkDistance <= 0)
    return;
  const float maxDist = static_cast<float>(m_params.linkDistance);
  const float maxDist2 = maxDist * maxDist;

  bool merged = true;
  while (merged) {
    merged = false;

    for (size_t i = 0; i < lines.size() && !merged; ++i) {
      for (size_t j = i + 1; j < lines.size() && !merged; ++j) {
        if (lines[i].empty() || lines[j].empty())
          continue;

        const TracedPoint &iFront = lines[i].front();
        const TracedPoint &iBack = lines[i].back();
        const TracedPoint &jFront = lines[j].front();
        const TracedPoint &jBack = lines[j].back();

        struct LinkOption {
          double distSq;
          bool reverseI;
          bool reverseJ;
        };

        auto sq = [](double dx, double dy) { return dx * dx + dy * dy; };

        const LinkOption opts[4] = {
            {sq(iBack.x - jFront.x, iBack.y - jFront.y), false, false},
            {sq(iBack.x - jBack.x, iBack.y - jBack.y), false, true},
            {sq(iFront.x - jFront.x, iFront.y - jFront.y), true, false},
            {sq(iFront.x - jBack.x, iFront.y - jBack.y), true, true},
        };

        int bestIdx = 0;
        for (int k = 1; k < 4; ++k)
          if (opts[k].distSq < opts[bestIdx].distSq)
            bestIdx = k;

        if (opts[bestIdx].distSq > maxDist2)
          continue;

        TracedLine lineI = lines[i];
        TracedLine lineJ = lines[j];
        if (opts[bestIdx].reverseI)
          std::reverse(lineI.begin(), lineI.end());
        if (opts[bestIdx].reverseJ)
          std::reverse(lineJ.begin(), lineJ.end());

        if (lineI.size() < 3 || lineJ.size() < 3)
          continue;

        // --- Guard 1: end directions must be roughly parallel ---
        // Sharply diverging directions mean this is two distinct,
        // nearby fringes, not one broken one.
        const double iTailDx = lineI.back().x - lineI[lineI.size() - 3].x;
        const double iTailDy = lineI.back().y - lineI[lineI.size() - 3].y;
        const double jHeadDx = lineJ[2].x - lineJ.front().x;
        const double jHeadDy = lineJ[2].y - lineJ.front().y;

        const double iLen = std::sqrt(iTailDx * iTailDx + iTailDy * iTailDy);
        const double jLen = std::sqrt(jHeadDx * jHeadDx + jHeadDy * jHeadDy);
        if (iLen < 0.5 || jLen < 0.5)
          continue;

        const double cosAngle =
            (iTailDx * jHeadDx + iTailDy * jHeadDy) / (iLen * jLen);
        if (cosAngle < 0.5) // > 60 degrees apart, or antiparallel
          continue;

        // --- Guard 2: the bridge must run over the binarized
        // fringe, not across a dark gap between two different ones ---
        const int x1 = static_cast<int>(lineI.back().x);
        const int y1 = static_cast<int>(lineI.back().y);
        const int x2 = static_cast<int>(lineJ.front().x);
        const int y2 = static_cast<int>(lineJ.front().y);

        const int steps = std::max(std::abs(x2 - x1), std::abs(y2 - y1));
        if (steps > 1 && !m_binary.empty()) {
          int whiteCount = 0;
          int total = 0;
          for (int s = 1; s < steps; ++s) {
            const double t = static_cast<double>(s) / steps;
            const int xi = static_cast<int>(x1 + (x2 - x1) * t + 0.5);
            const int yi = static_cast<int>(y1 + (y2 - y1) * t + 0.5);
            if (xi < 0 || xi >= m_binary.cols || yi < 0 || yi >= m_binary.rows)
              continue;
            if (m_binary.at<uchar>(yi, xi) > 0)
              ++whiteCount;
            ++total;
          }

          if (total > 0) {
            const double whiteRatio = static_cast<double>(whiteCount) / total;
            if (whiteRatio < 0.6) // require at least 60% of the bridge on white
              continue;
          }
        }

        // --- Passed all checks: splice via a cubic Bezier ---
        const double ndirIx = iTailDx / iLen;
        const double ndirIy = iTailDy / iLen;
        const double ndirJx = jHeadDx / jLen;
        const double ndirJy = jHeadDy / jLen;

        TracedLine combined;
        combined.reserve(lineI.size() + lineJ.size() + 10);
        combined.insert(combined.end(), lineI.begin(), lineI.end());

        const TracedPoint p0 = lineI.back();
        const TracedPoint p3 = lineJ.front();
        const double gapDist = std::sqrt((p0.x - p3.x) * (p0.x - p3.x) +
                                         (p0.y - p3.y) * (p0.y - p3.y));

        if (gapDist <= maxDist && gapDist > 1.0) {
          TracedPoint p1 = p0;
          TracedPoint p2 = p3;
          const double extension = gapDist * 0.5;

          p1.x += ndirIx * extension;
          p1.y += ndirIy * extension;
          p2.x -= ndirJx * extension;
          p2.y -= ndirJy * extension;

          const int numPoints = std::max(3, static_cast<int>(gapDist / 1.5));

          for (int k = 1; k <= numPoints; ++k) {
            const double t = static_cast<double>(k) / (numPoints + 1);
            const double mt = 1.0 - t;
            const double mt2 = mt * mt;
            const double t2 = t * t;

            TracedPoint bezier;
            bezier.x = mt2 * mt * p0.x + 3 * mt2 * t * p1.x +
                       3 * mt * t2 * p2.x + t2 * t * p3.x;
            bezier.y = mt2 * mt * p0.y + 3 * mt2 * t * p1.y +
                       3 * mt * t2 * p2.y + t2 * t * p3.y;
            bezier.intensity =
                static_cast<float>(p0.intensity * (1 - t) + p3.intensity * t);
            bezier.width =
                static_cast<float>(p0.width * (1 - t) + p3.width * t);

            combined.push_back(bezier);
          }
        }

        combined.insert(combined.end(), lineJ.begin(), lineJ.end());

        lines[i] = std::move(combined);
        lines.erase(lines.begin() + static_cast<long>(j));
        merged = true;
      }
    }
  }
}

void BinaryThinningTracker::filterByCurvature(
    std::vector<TracedLine> &lines) const {
  if (m_params.maxAvgAngle <= 0)
    return;

  lines.erase(
      std::remove_if(lines.begin(), lines.end(),
                     [this](const TracedLine &line) {
                       if (line.size() < 5)
                         return true;

                       double totalAngle = 0.0;
                       int count = 0;
                       for (size_t i = 1; i + 1 < line.size(); ++i) {
                         const double dx1 = line[i].x - line[i - 1].x;
                         const double dy1 = line[i].y - line[i - 1].y;
                         const double dx2 = line[i + 1].x - line[i].x;
                         const double dy2 = line[i + 1].y - line[i].y;
                         const double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
                         const double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
                         if (len1 < 0.5 || len2 < 0.5)
                           continue;
                         double cosA = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
                         cosA = std::clamp(cosA, -1.0, 1.0);
                         totalAngle += std::acos(cosA);
                         ++count;
                       }
                       if (count == 0)
                         return true;

                       const double avgAngle = totalAngle / count;
                       return avgAngle > m_params.maxAvgAngle;
                     }),
      lines.end());
}

} // namespace digitqt::core::tracing
