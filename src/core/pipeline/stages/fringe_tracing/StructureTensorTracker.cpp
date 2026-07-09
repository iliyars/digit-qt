/**
 * @file StructureTensorTracker.cpp
 * @brief Ridge-following fringe tracer using structure-tensor orientation
 * and 2D local-maximum centering -- an alternative to
 * SequentialFringeTracker (the SCAN360/STEP.C-derived tracer), not a
 * replacement for it; both are selectable in the UI.
 *
 * Direction here is a continuous angle from the local gradient structure
 * tensor (not 45-degree-quantized), and centering searches a genuine 2D
 * neighborhood (not a single line) -- see the class doc comment in the
 * header for the full rationale. Public contract (IFringeTracer,
 * TracedPoint/TracedLine, seed points, bidirectional tracing, loop
 * closure) matches SequentialFringeTracker's.
 */
#include "StructureTensorTracker.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace digitqt::core::tracing {

StructureTensorTracker::StructureTensorTracker() = default;

bool StructureTensorTracker::initialize(
    const QImage &image, std::function<bool(int, int)> isVisible) {
  if (image.isNull()) {
    m_lastError = QStringLiteral("Empty image");
    return false;
  }

  m_grayImage = image.convertToFormat(QImage::Format_Grayscale8);
  m_image = m_grayImage.constBits();
  m_width = m_grayImage.width();
  m_height = m_grayImage.height();
  m_stride = static_cast<int>(m_grayImage.bytesPerLine());
  m_isVisible = std::move(isVisible);
  m_lastError.clear();
  return true;
}

std::vector<TracedLine>
StructureTensorTracker::extract(const std::vector<SeedPoint> &seeds) {
  std::vector<TracedLine> result;
  result.reserve(seeds.size());

  for (const auto &seed : seeds) {
    TracedLine line;
    if (traceLineInto(seed.x, seed.y, line) && line.size() >= 2)
      result.push_back(std::move(line));
  }
  return result;
}

TracedLine StructureTensorTracker::traceLine(int startX, int startY) {
  TracedLine result;
  if (!m_image) {
    m_lastError =
        QStringLiteral("Tracer not initialized. Call initialize() first.");
    return result;
  }
  traceLineInto(startX, startY, result);
  return result;
}

bool StructureTensorTracker::isInside(int x, int y) const {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    return false;
  return !m_isVisible || m_isVisible(x, y);
}

uint8_t StructureTensorTracker::getPixel(int x, int y) const {
  if (!m_image || x < 0 || x >= m_width || y < 0 || y >= m_height)
    return 0;
  return m_image[y * m_stride + x];
}

float StructureTensorTracker::sampleBilinear(double x, double y) const {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const double fx = x - x0;
  const double fy = y - y0;

  const float p00 = static_cast<float>(getPixel(x0, y0));
  const float p10 = static_cast<float>(getPixel(x0 + 1, y0));
  const float p01 = static_cast<float>(getPixel(x0, y0 + 1));
  const float p11 = static_cast<float>(getPixel(x0 + 1, y0 + 1));

  const float top = static_cast<float>(p00 * (1.0 - fx) + p10 * fx);
  const float bottom = static_cast<float>(p01 * (1.0 - fx) + p11 * fx);
  return static_cast<float>(top * (1.0 - fy) + bottom * fy);
}

bool StructureTensorTracker::estimateDirection(double x, double y, double &dirX,
                                               double &dirY) const {
  const int radius = m_params.orientationWindowRadius;
  const int cx = static_cast<int>(std::lround(x));
  const int cy = static_cast<int>(std::lround(y));

  double sxx = 0.0, syy = 0.0, sxy = 0.0;
  bool any = false;

  for (int oy = -radius; oy <= radius; ++oy) {
    for (int ox = -radius; ox <= radius; ++ox) {
      const int sx = cx + ox;
      const int sy = cy + oy;
      if (!isInside(sx, sy))
        continue;

      // Central-difference gradient, sampled on the pixel grid.
      const float gx = static_cast<float>(getPixel(sx + 1, sy)) -
                       static_cast<float>(getPixel(sx - 1, sy));
      const float gy = static_cast<float>(getPixel(sx, sy + 1)) -
                       static_cast<float>(getPixel(sx, sy - 1));

      sxx += static_cast<double>(gx) * gx;
      syy += static_cast<double>(gy) * gy;
      sxy += static_cast<double>(gx) * gy;
      any = true;
    }
  }

  if (!any)
    return false;

  // Eigenvector of the SMALLER eigenvalue of the structure tensor
  // [[sxx, sxy], [sxy, syy]] -- the direction of least gradient energy,
  // i.e. along the ridge (gradient energy concentrates *across* a
  // bright/dark stripe, not along its length).
  const double trace = sxx + syy;
  const double det = sxx * syy - sxy * sxy;
  const double disc = std::sqrt(std::max(0.0, (trace * trace) / 4.0 - det));
  const double lambdaMin = trace / 2.0 - disc;

  double vx, vy;
  if (std::fabs(sxy) > 1e-9) {
    vx = sxy;
    vy = lambdaMin - sxx;
  } else if (sxx <= syy) {
    vx = 1.0;
    vy = 0.0;
  } else {
    vx = 0.0;
    vy = 1.0;
  }

  const double len = std::sqrt(vx * vx + vy * vy);
  if (len < 1e-9)
    return false;

  dirX = vx / len;
  dirY = vy / len;
  return true;
}

float StructureTensorTracker::estimateWidth(double x, double y, double dirX,
                                            double dirY,
                                            float &outBackground) const {
  // Perpendicular to the travel direction.
  const double px = -dirY;
  const double py = dirX;

  const float centerIntensity = sampleBilinear(x, y);

  constexpr double kProbeDistance = 25.0;
  const float bgPlus =
      sampleBilinear(x + px * kProbeDistance, y + py * kProbeDistance);
  const float bgMinus =
      sampleBilinear(x - px * kProbeDistance, y - py * kProbeDistance);
  const float background = std::min(bgPlus, bgMinus);
  outBackground = background;

  const float halfLevel = (centerIntensity + background) / 2.0f;

  double distPlus = 1.0;
  for (double t = 0.5; t < kProbeDistance; t += 0.5) {
    if (sampleBilinear(x + px * t, y + py * t) < halfLevel)
      break;
    distPlus = t;
  }
  double distMinus = 1.0;
  for (double t = 0.5; t < kProbeDistance; t += 0.5) {
    if (sampleBilinear(x - px * t, y - py * t) < halfLevel)
      break;
    distMinus = t;
  }

  return std::clamp(static_cast<float>(distPlus + distMinus), 2.0f, 80.0f);
}

bool StructureTensorTracker::findLocalMaximum(double cx, double cy,
                                              float radius, double &outX,
                                              double &outY,
                                              float &outIntensity) const {
  const int ix = static_cast<int>(std::lround(cx));
  const int iy = static_cast<int>(std::lround(cy));
  const int r = std::max(1, static_cast<int>(std::lround(radius)));

  int bestX = ix, bestY = iy;
  float bestVal = -1.0f;
  bool any = false;

  for (int oy = -r; oy <= r; ++oy) {
    for (int ox = -r; ox <= r; ++ox) {
      const int sx = ix + ox;
      const int sy = iy + oy;
      if (!isInside(sx, sy))
        continue;
      const float v = static_cast<float>(getPixel(sx, sy));
      any = true;
      if (v > bestVal) {
        bestVal = v;
        bestX = sx;
        bestY = sy;
      }
    }
  }

  if (!any)
    return false;

  // Sub-pixel refine via independent 1D quadratic (parabola-through-3-
  // samples) fits in x and y, centered on the winning pixel.
  double refinedX = bestX;
  double refinedY = bestY;

  if (isInside(bestX - 1, bestY) && isInside(bestX + 1, bestY)) {
    const float left = static_cast<float>(getPixel(bestX - 1, bestY));
    const float center = static_cast<float>(getPixel(bestX, bestY));
    const float right = static_cast<float>(getPixel(bestX + 1, bestY));
    const float denom = left - 2.0f * center + right;
    if (std::fabs(denom) > 1e-4f) {
      const double offset = 0.5 * (left - right) / denom;
      if (std::fabs(offset) < 1.0)
        refinedX = bestX + offset;
    }
  }
  if (isInside(bestX, bestY - 1) && isInside(bestX, bestY + 1)) {
    const float up = static_cast<float>(getPixel(bestX, bestY - 1));
    const float center = static_cast<float>(getPixel(bestX, bestY));
    const float down = static_cast<float>(getPixel(bestX, bestY + 1));
    const float denom = up - 2.0f * center + down;
    if (std::fabs(denom) > 1e-4f) {
      const double offset = 0.5 * (up - down) / denom;
      if (std::fabs(offset) < 1.0)
        refinedY = bestY + offset;
    }
  }

  outX = refinedX;
  outY = refinedY;
  outIntensity = sampleBilinear(refinedX, refinedY);
  return true;
}

bool StructureTensorTracker::traceLineInto(int startX, int startY,
                                           TracedLine &outPoints) {
  outPoints.clear();
  m_lastError.clear();

  if (!isInside(startX, startY)) {
    m_lastError =
        QStringLiteral("Start point is outside the aperture/boundaries");
    return false;
  }

  double dirX = 0.0, dirY = 1.0;
  if (!estimateDirection(startX, startY, dirX, dirY)) {
    m_lastError =
        QStringLiteral("Could not determine initial fringe direction");
    return false;
  }

  // Snap the seed itself onto the nearest local maximum -- a user click
  // is rarely exactly on the ridge.
  double seedX = startX, seedY = startY;
  float seedIntensity = 0.0f;
  if (!findLocalMaximum(startX, startY, 5.0f, seedX, seedY, seedIntensity)) {
    m_lastError =
        QStringLiteral("No usable intensity peak near the seed point");
    return false;
  }

  float seedBackground = 0.0f;
  const float seedWidth =
      estimateWidth(seedX, seedY, dirX, dirY, seedBackground);

  TracedPoint seedPoint;
  seedPoint.x = seedX;
  seedPoint.y = seedY;
  seedPoint.width = seedWidth;
  seedPoint.intensity = seedIntensity;

  TracedLine forwardLine;
  forwardLine.push_back(seedPoint);
  m_contrastEma = 0.0f;
  traceDirectionally(forwardLine, dirX, dirY);

  outPoints = forwardLine;

  if (m_params.bidirectional) {
    TracedLine backwardLine;
    backwardLine.push_back(seedPoint);
    m_contrastEma = 0.0f;
    traceDirectionally(backwardLine, -dirX, -dirY);

    if (backwardLine.size() > 1) {
      TracedLine combined;
      combined.assign(backwardLine.rbegin(),
                      backwardLine.rend() -
                          1); // reversed, shared seed dropped once
      combined.insert(combined.end(), forwardLine.begin(), forwardLine.end());
      outPoints = std::move(combined);
    }
  }

  return outPoints.size() >= 2;
}

void StructureTensorTracker::traceDirectionally(TracedLine &line, double dirX,
                                                double dirY) {
  std::vector<std::pair<double, double>> dirHistory;
  dirHistory.emplace_back(dirX, dirY);

  for (int i = 0; i < m_params.maxSteps; ++i) {
    const TracedPoint &last = line.back();
    const float width =
        (last.width > 0.0f) ? last.width : static_cast<float>(m_width) / 6.0f;

    const double stepSize =
        std::clamp(static_cast<double>(width) * m_params.stepFraction,
                   static_cast<double>(m_params.minStepSize),
                   static_cast<double>(m_params.maxStepSize));

    const double predX = last.x + dirX * stepSize;
    const double predY = last.y + dirY * stepSize;

    if (!isInside(static_cast<int>(std::lround(predX)),
                  static_cast<int>(std::lround(predY))))
      break; // reached the aperture/image boundary

    const float searchRadius =
        std::clamp(width * m_params.searchRadiusFraction,
                   m_params.minSearchRadius, m_params.maxSearchRadius);

    double foundX = 0.0, foundY = 0.0;
    float foundIntensity = 0.0f;
    if (!findLocalMaximum(predX, predY, searchRadius, foundX, foundY,
                          foundIntensity))
      break;

    // Anti-jump guard: a maximum further than one fringe-width from
    // the *predicted* point almost certainly belongs to a
    // neighboring fringe, not this one.
    const double jumpDx = foundX - predX;
    const double jumpDy = foundY - predY;
    if (std::sqrt(jumpDx * jumpDx + jumpDy * jumpDy) >
        width * m_params.maxCenteringJumpFactor)
      break;

    float background = 0.0f;
    const float newWidth =
        estimateWidth(foundX, foundY, dirX, dirY, background);
    const float contrast = foundIntensity - background;

    // Loss-of-fringe check based on *contrast* (peak - local
    // background), not raw intensity -- contrast stays meaningful
    // even where overall brightness has faded due to vignetting.
    if (m_contrastEma > 0.0f &&
        contrast < m_contrastEma * m_params.minContrastFraction)
      break;
    m_contrastEma =
        (m_contrastEma <= 0.0f)
            ? contrast
            : (m_params.contrastSmoothingAlpha * contrast +
               (1.0f - m_params.contrastSmoothingAlpha) * m_contrastEma);

    TracedPoint p;
    p.x = foundX;
    p.y = foundY;
    p.width = newWidth;
    p.intensity = foundIntensity;
    line.push_back(p);

    // Loop-closure check (closed fringe contour).
    if (static_cast<int>(line.size()) > 5) {
      const TracedPoint &ref = line.front();
      const double dx0 = foundX - ref.x;
      const double dy0 = foundY - ref.y;
      if (std::sqrt(dx0 * dx0 + dy0 * dy0) < newWidth)
        break;
    }

    // Re-estimate direction at the new point and smooth over recent
    // history so a single noisy estimate can't swing the trajectory.
    double nextDirX = dirX, nextDirY = dirY;
    if (estimateDirection(foundX, foundY, nextDirX, nextDirY)) {
      if (nextDirX * dirX + nextDirY * dirY < 0.0) {
        nextDirX = -nextDirX;
        nextDirY = -nextDirY;
      }
      dirHistory.emplace_back(nextDirX, nextDirY);
      if (static_cast<int>(dirHistory.size()) >
          m_params.directionSmoothingWindow)
        dirHistory.erase(dirHistory.begin());

      double sumX = 0.0, sumY = 0.0;
      for (const auto &d : dirHistory) {
        sumX += d.first;
        sumY += d.second;
      }
      const double len = std::sqrt(sumX * sumX + sumY * sumY);
      if (len > 1e-6) {
        dirX = sumX / len;
        dirY = sumY / len;
      }
    }
  }
}

} // namespace digitqt::core::tracing
