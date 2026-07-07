/**
 * @file SeedStepTracer.cpp
 * @brief Step-by-step fringe tracer, ported from SCAN360/STEP.C.
 *
 * Ported from the uploaded InterferometryApp project's CFringeTracer, with
 * two adaptations:
 *   - initialize() takes a QImage + isVisible predicate instead of a
 *     cv::Mat + CEllipseBoundary, so this tracer has no OpenCV dependency
 *     and works against our own (multi-shape) aperture, not a single ellipse.
 *   - TracedPoint uses double x/y (to stay uniform with other tracers that
 *     produce sub-pixel positions); this algorithm itself still computes
 *     in integer pixel coordinates internally, unchanged from the original.
 *
 * The numeric logic (thresholds, step formulas, direction handling) is
 * otherwise unchanged from the original STEP.C port.
 */
#include "SeedStepTracer.h"

#include <algorithm>
#include <cmath>

namespace digitqt::core::tracing {

namespace {
int sgn(int v) { return (v > 0) - (v < 0); }
} // namespace

SeedStepTracer::SeedStepTracer() = default;

bool SeedStepTracer::initialize(const QImage &image,
                                std::function<bool(int, int)> isVisible) {
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
SeedStepTracer::extract(const std::vector<SeedPoint> &seeds) {
  std::vector<TracedLine> result;
  result.reserve(seeds.size());

  for (const auto &seed : seeds) {
    TracedLine line;
    if (traceLineInto(seed.x, seed.y, line) && line.size() >= 2)
      result.push_back(std::move(line));
  }
  return result;
}

TracedLine SeedStepTracer::traceLine(int startX, int startY) {
  TracedLine result;
  if (!m_image) {
    m_lastError =
        QStringLiteral("Tracer not initialized. Call initialize() first.");
    return result;
  }
  traceLineInto(startX, startY, result);
  return result;
}

bool SeedStepTracer::isInside(int x, int y) const {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    return false;
  return !m_isVisible || m_isVisible(x, y);
}

uint8_t SeedStepTracer::getPixel(int x, int y) const {
  if (!m_image || x < 0 || x >= m_width || y < 0 || y >= m_height)
    return 0;
  return m_image[y * m_stride + x];
}

float SeedStepTracer::averageIntensity(int x, int y) const {
  if (!isInside(x, y))
    return 0.0f;

  static const int neighbors[8][2] = {{0, 1},  {0, -1}, {1, 0},  {1, 1},
                                      {1, -1}, {-1, 0}, {-1, 1}, {-1, -1}};

  float sum = static_cast<float>(getPixel(x, y));
  int count = 1;

  for (const auto &n : neighbors) {
    const int nx = x + n[0];
    const int ny = y + n[1];
    if (isInside(nx, ny)) {
      sum += static_cast<float>(getPixel(nx, ny));
      ++count;
    }
  }
  return sum / static_cast<float>(count);
}

void SeedStepTracer::directionToVector(TraceDirection direction, int &dx,
                                       int &dy) const {
  switch (direction) {
  case TraceDirection::Vertical:
    dx = 0;
    dy = 1;
    break;
  case TraceDirection::Diagonal45:
    dx = 1;
    dy = 1;
    break;
  case TraceDirection::Horizontal:
    dx = 1;
    dy = 0;
    break;
  case TraceDirection::Diagonal135:
    dx = 1;
    dy = -1;
    break;
  }
}

bool SeedStepTracer::traceLineInto(int startX, int startY,
                                   TracedLine &outPoints) {
  outPoints.clear();
  m_tempLine.clear();
  m_lastError.clear();

  m_curWidth = static_cast<float>(m_width) / 6.0f;
  m_wideLine = static_cast<float>(m_width) / 5.0f;
  m_curAverage = 0.0f;
  m_average = 0.0f;

  if (!isInside(startX, startY)) {
    m_lastError =
        QStringLiteral("Start point is outside the aperture/boundaries");
    return false;
  }

  TracedPoint point1, point2;
  if (!firstStep(startX, startY, point1, point2)) {
    m_lastError =
        QStringLiteral("Could not determine initial fringe direction");
    return false;
  }

  m_tempLine.clear();
  m_tempLine.push_back(point1);
  m_tempLine.push_back(point2);

  int stop = 0;
  int i = 1;
  while (i < m_params.maxSteps) {
    stop = step(m_tempLine);
    if (stop != 0)
      break;
    ++i;
  }

  if (stop == -10) {
    outPoints = m_tempLine;
    return outPoints.size() >= 2;
  }

  // Bidirectional tracing: re-trace from the start, going the other way,
  // then splice reverse+forward together (see original STEP.C:130-161).
  if (m_params.bidirectional && m_tempLine.size() >= 3) {
    TracedLine forwardLine = m_tempLine;
    m_tempLine.clear();

    m_tempLine.push_back(forwardLine[2]);
    m_tempLine.push_back(forwardLine[1]);
    m_tempLine.push_back(forwardLine[0]);

    m_curWidth = forwardLine[0].width;
    if (m_curWidth < 5.0f)
      m_curWidth = forwardLine[1].width;
    if (m_curWidth < 5.0f)
      m_curWidth = static_cast<float>(m_width) / 6.0f;
    m_wideLine = m_curWidth;
    m_average = 0.0f;

    i = 2;
    while (i < m_params.maxSteps) {
      stop = step(m_tempLine);
      if (stop != 0)
        break;
      ++i;
    }

    TracedLine reversePart;
    if (m_tempLine.size() > 3) {
      reversePart.assign(m_tempLine.begin() + 3, m_tempLine.end());
      std::reverse(reversePart.begin(), reversePart.end());
    }

    m_tempLine = reversePart;
    for (const auto &p : forwardLine)
      m_tempLine.push_back(p);
  }

  outPoints = m_tempLine;
  return outPoints.size() >= 2;
}

bool SeedStepTracer::firstStep(int x, int y, TracedPoint &point1,
                               TracedPoint &point2) {
  TraceDirection direction = TraceDirection::Vertical;
  if (!measureWidth(x, y, m_curWidth, direction))
    return false;

  if (m_curWidth < 5.0f)
    m_curWidth = 5.0f;

  m_curDirection = direction;
  m_curAverage = averageIntensity(x, y);

  int dx = 0, dy = 0;
  directionToVector(direction, dx, dy);

  // Search radius = half-width (not full width) so we don't jump onto
  // the neighboring fringe.
  int xx = x, yy = y;
  if (!findMaxAlong(xx, yy, dx, dy, m_curWidth))
    return false;

  if (!measureWidth(xx, yy, m_curWidth, direction))
    return false;
  if (m_curWidth < 5.0f)
    m_curWidth = 5.0f;

  point1.x = xx;
  point1.y = yy;
  point1.width = m_curWidth;
  point1.intensity = averageIntensity(xx, yy);

  int perpDx = 0, perpDy = 0;
  switch (direction) {
  case TraceDirection::Vertical:
    perpDx = static_cast<int>(m_curWidth + 0.5f);
    perpDy = 0;
    break;
  case TraceDirection::Diagonal45:
    perpDx = static_cast<int>(0.707f * m_curWidth + 0.5f);
    perpDy = -perpDx;
    break;
  case TraceDirection::Horizontal:
    perpDx = 0;
    perpDy = static_cast<int>(m_curWidth + 0.5f);
    break;
  case TraceDirection::Diagonal135:
    perpDx = static_cast<int>(0.707f * m_curWidth + 0.5f);
    perpDy = perpDx;
    break;
  }

  xx = static_cast<int>(point1.x) + perpDx;
  yy = static_cast<int>(point1.y) + perpDy;

  if (!isInside(xx, yy))
    return false;

  if (!centerPerpendicular(xx, yy, perpDx, perpDy))
    return false;

  point2.x = xx;
  point2.y = yy;
  point2.width = m_curWidth;
  point2.intensity = averageIntensity(xx, yy);

  return true;
}

int SeedStepTracer::step(TracedLine &line) {
  const float coeffWide = 1.5f;

  const int numPoint = static_cast<int>(line.size()) - 1;
  if (numPoint < 1)
    return -100;

  if (m_curWidth < 2.0f)
    return -3;

  const int x = static_cast<int>(line[static_cast<size_t>(numPoint)].x);
  const int y = static_cast<int>(line[static_cast<size_t>(numPoint)].y);

  // If we've dropped below the "floor" threshold, we've slid off the
  // fringe (e.g. reached a dark zone near the aperture edge). Stop.
  // Compared against the OLD m_average (before measureWidth's
  // recalculation below) -- that's the "real" fringe threshold.
  if (m_average > 0.0f && averageIntensity(x, y) < m_average)
    return -5;

  TraceDirection dir = TraceDirection::Vertical;
  float measuredWidth = 0.0f;
  if (!measureWidth(x, y, measuredWidth, dir))
    return -3;

  m_curDirection = dir;

  m_wideLine = measuredWidth;
  if (m_wideLine < 5.0f)
    m_wideLine = 5.0f;
  if (m_wideLine > 80.0f)
    m_wideLine = 80.0f;

  // Width can't change by more than coeffWide per step.
  if (m_curWidth / m_wideLine > coeffWide)
    m_wideLine = m_curWidth / coeffWide;
  else if (m_wideLine / m_curWidth > coeffWide)
    m_wideLine = m_curWidth * coeffWide;

  if (m_wideLine > 80.0f)
    m_wideLine = 80.0f;

  m_curWidth = m_wideLine;

  int dx = x - static_cast<int>(line[static_cast<size_t>(numPoint - 1)].x);
  int dy = y - static_cast<int>(line[static_cast<size_t>(numPoint - 1)].y);

  if (std::fabs(static_cast<float>(dx)) < 2.0f &&
      std::fabs(static_cast<float>(dy)) < 2.0f) {
    if (numPoint < 2)
      return -100;
    dx = x - static_cast<int>(line[static_cast<size_t>(numPoint - 2)].x);
    dy = y - static_cast<int>(line[static_cast<size_t>(numPoint - 2)].y);
  }

  if (std::fabs(static_cast<float>(dx)) < 1.0f &&
      std::fabs(static_cast<float>(dy)) < 1.0f)
    return -100;

  float fdx = static_cast<float>(dx);
  float fdy = static_cast<float>(dy);
  const float sqrWide = std::sqrt(fdx * fdx + fdy * fdy);

  if (m_wideLine <= 5.0f) {
    fdx = fdx * m_wideLine / sqrWide;
    fdy = fdy * m_wideLine / sqrWide;
  } else if (m_wideLine <= 10.0f) {
    fdx = 0.8f * fdx * m_wideLine / sqrWide;
    fdy = 0.8f * fdy * m_wideLine / sqrWide;
  } else if (m_wideLine <= 20.0f) {
    fdx = 0.6f * fdx * m_wideLine / sqrWide;
    fdy = 0.6f * fdy * m_wideLine / sqrWide;
  } else {
    fdx = 0.4f * fdx * m_wideLine / sqrWide;
    fdy = 0.4f * fdy * m_wideLine / sqrWide;
  }

  const int stepX = (fdx < 0.0f) ? static_cast<int>(std::ceil(fdx - 0.5f))
                                 : static_cast<int>(std::floor(fdx + 0.5f));
  const int stepY = (fdy < 0.0f) ? static_cast<int>(std::ceil(fdy - 0.5f))
                                 : static_cast<int>(std::floor(fdy + 0.5f));

  const int predX = x + stepX;
  const int predY = y + stepY;

  if (!isInside(predX, predY)) {
    int boundX = predX, boundY = predY;
    linStepToBoundary(x, y, predX, predY, boundX, boundY);

    TracedPoint p;
    p.x = boundX;
    p.y = boundY;
    p.width = m_curWidth;
    p.intensity = averageIntensity(boundX, boundY);
    line.push_back(p);
    return -1;
  }

  const int ndx = sgn(stepX);
  const int ndy = sgn(stepY);

  int cx = predX, cy = predY;
  if (!centerPerpendicular(cx, cy, ndx, ndy)) {
    TracedPoint p;
    p.x = predX;
    p.y = predY;
    p.width = m_curWidth;
    p.intensity = averageIntensity(predX, predY);
    line.push_back(p);
    return -2;
  }

  TracedPoint np;
  np.x = cx;
  np.y = cy;
  np.width = m_curWidth;
  np.intensity = averageIntensity(cx, cy);
  line.push_back(np);

  // Closure stop condition: back near the start -> the loop closed.
  if (static_cast<int>(line.size()) > 5) {
    const TracedPoint &ref = line.front();
    const float dx0 = static_cast<float>(cx - static_cast<int>(ref.x));
    const float dy0 = static_cast<float>(cy - static_cast<int>(ref.y));
    if (std::sqrt(dx0 * dx0 + dy0 * dy0) < m_wideLine)
      return -10;
  }

  return 0;
}

bool SeedStepTracer::measureWidth(int x, int y, float &outWidth,
                                  TraceDirection &outDirection) {
  const float coefAver = 1.5f;
  const int d[4][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}};

  // --- Phase 1: find the "floor" threshold (average) ------------------
  // average gets overwritten on every direction -- the final value comes
  // from the last direction {1,-1}. That's how the original behaves.
  float minAver = m_average / coefAver;
  const float maxWide = m_wideLine * 1.41f;
  const int stepLimit = 3;

  for (int i = 0; i < 4; ++i) {
    const int dx = d[i][0];
    const int dy = d[i][1];
    int ddx = dx, ddy = dy;

    int n = 1;
    float s = static_cast<float>(getPixel(x, y));

    if (isInside(x + dx, y + dy)) {
      s += getPixel(x + dx, y + dy);
      ++n;
    }
    if (isInside(x - dx, y - dy)) {
      s += getPixel(x - dx, y - dy);
      ++n;
    }

    float ss = s / static_cast<float>(n);
    int r = 1, k = 0;

    while ((k < stepLimit && r < m_width / 6) ||
           r < static_cast<int>(maxWide)) {
      ++r;
      ddx += dx;
      ddy += dy;

      if (isInside(x + ddx, y + ddy)) {
        s += getPixel(x + ddx, y + ddy);
        ++n;
      }
      if (isInside(x - ddx, y - ddy)) {
        s += getPixel(x - ddx, y - ddy);
        ++n;
      }

      const float buf = s / static_cast<float>(n);
      if (ss > buf) {
        ss = buf;
        k = 0;
      } else {
        ++k;
        minAver = ss; // fix the floor
        ss = buf;
      }
    }
    m_average = minAver;
  }

  // --- Phase 2: measure width in 4 directions, take the minimum -------
  float minWide = static_cast<float>(m_width) / 6.0f;
  int bestDirectionIdx = 0;

  for (int j = 0; j < 4; ++j) {
    const int dx = d[j][0];
    const int dy = d[j][1];
    int ddx = dx, ddy = dy;
    float ii = (j == 1 || j == 3) ? 1.42f : 1.0f; // diagonals are longer

    while (isInside(x + ddx, y + ddy) &&
           averageIntensity(x + ddx, y + ddy) > m_average && ii < minWide) {
      ii += (j == 1 || j == 3) ? 1.42f : 1.0f;
      ddx += dx;
      ddy += dy;
    }

    ddx = dx;
    ddy = dy;
    while (isInside(x - ddx, y - ddy) &&
           averageIntensity(x - ddx, y - ddy) > m_average &&
           ii <= minWide + 3) {
      ii += (j == 1 || j == 3) ? 1.42f : 1.0f;
      ddx += dx;
      ddy += dy;
    }

    if (minWide > ii) {
      minWide = ii;
      bestDirectionIdx = j;
    }
  }

  if (minWide < 2.0f)
    return false;

  outWidth = minWide;
  outDirection = static_cast<TraceDirection>(bestDirectionIdx);
  m_curAverage = averageIntensity(x, y);
  m_wideLine = minWide;
  return true;
}

bool SeedStepTracer::findMaxAlong(int &x, int &y, int dx, int dy,
                                  float searchDist) {
  int halfSteps = static_cast<int>(searchDist / 2.0f + 0.5f);
  if (halfSteps < 2)
    halfSteps = 2;

  float maxIntensity = averageIntensity(x, y);
  int maxX = x, maxY = y;

  int plusX = x, plusY = y;
  int minusX = x, minusY = y;

  for (int i = 0; i <= halfSteps; ++i) {
    if (isInside(plusX, plusY)) {
      const float intensity = averageIntensity(plusX, plusY);
      if (intensity > maxIntensity) {
        maxIntensity = intensity;
        maxX = plusX;
        maxY = plusY;
      }
    } else {
      break;
    }

    if (isInside(minusX, minusY)) {
      const float intensity = averageIntensity(minusX, minusY);
      if (intensity > maxIntensity) {
        maxIntensity = intensity;
        maxX = minusX;
        maxY = minusY;
      }
    }

    plusX += dx;
    plusY += dy;
    minusX -= dx;
    minusY -= dy;
  }

  x = maxX;
  y = maxY;
  return true; // always succeeds -- worst case, the start point itself
}

bool SeedStepTracer::centerPerpendicular(int &x, int &y, int dx, int dy) {
  if (dx != 0)
    dx = (dx > 0) ? 1 : -1;
  if (dy != 0)
    dy = (dy > 0) ? 1 : -1;

  int perpDx = -dy;
  int perpDy = dx;

  int xx = x + dx;
  int yy = y + dy;

  if (!isInside(xx, yy))
    return false;

  // --- Phase 1: "catch" the fringe if the predicted point fell off it -
  int maxRetries = 3;
  while (averageIntensity(xx, yy) < m_average && maxRetries-- > 0) {
    bool found = false;
    const int halfWidth = static_cast<int>(m_wideLine / 3.0f);

    for (int i = 1; i <= halfWidth; ++i) {
      const int testPlusX = xx + perpDx * i;
      const int testPlusY = yy + perpDy * i;
      if (isInside(testPlusX, testPlusY) &&
          averageIntensity(testPlusX, testPlusY) >= m_average) {
        xx = testPlusX;
        yy = testPlusY;
        dx = xx - x;
        dy = yy - y;
        if (dx != 0)
          dx = (dx > 0) ? 1 : -1;
        if (dy != 0)
          dy = (dy > 0) ? 1 : -1;
        perpDx = -dy;
        perpDy = dx;
        found = true;
        break;
      }

      const int testMinusX = xx - perpDx * i;
      const int testMinusY = yy - perpDy * i;
      if (isInside(testMinusX, testMinusY) &&
          averageIntensity(testMinusX, testMinusY) >= m_average) {
        xx = testMinusX;
        yy = testMinusY;
        dx = xx - x;
        dy = yy - y;
        if (dx != 0)
          dx = (dx > 0) ? 1 : -1;
        if (dy != 0)
          dy = (dy > 0) ? 1 : -1;
        perpDx = -dy;
        perpDy = dx;
        found = true;
        break;
      }
    }
    if (!found)
      break;
  }

  // --- Phase 2: precise centering -- find the max within the fringe ---
  float maxIntensity = averageIntensity(xx, yy);
  int maxX = xx, maxY = yy;

  const int halfWidth = static_cast<int>(m_wideLine / 2.0f);

  for (int i = 1; i < halfWidth; ++i) {
    const int testX = xx + perpDx * i;
    const int testY = yy + perpDy * i;
    if (!isInside(testX, testY))
      break;
    const float intensity = averageIntensity(testX, testY);
    if (intensity < m_average)
      break;
    if (intensity > maxIntensity) {
      maxIntensity = intensity;
      maxX = testX;
      maxY = testY;
    }
  }

  for (int i = 1; i < halfWidth; ++i) {
    const int testX = xx - perpDx * i;
    const int testY = yy - perpDy * i;
    if (!isInside(testX, testY))
      break;
    const float intensity = averageIntensity(testX, testY);
    if (intensity < m_average)
      break;
    if (intensity > maxIntensity) {
      maxIntensity = intensity;
      maxX = testX;
      maxY = testY;
    }
  }

  x = maxX;
  y = maxY;
  return true;
}

void SeedStepTracer::linStepToBoundary(int x1, int y1, int x2, int y2,
                                       int &outX, int &outY) const {
  float dx = static_cast<float>(x2 - x1);
  float dy = static_cast<float>(y2 - y1);

  const int signX = (dx < 0) ? -1 : 1;
  const int signY = (dy < 0) ? -1 : 1;
  dx = std::fabs(dx);
  dy = std::fabs(dy);

  int stop = 0;
  if (dx == 0.0f) {
    stop = static_cast<int>(dy);
    dy = static_cast<float>(signY);
  } else if (dy == 0.0f) {
    stop = static_cast<int>(dx);
    dx = static_cast<float>(signX);
  } else {
    stop = static_cast<int>(dy);
    dx = dx / dy * static_cast<float>(signX);
    dy = static_cast<float>(signY);
  }

  float x = static_cast<float>(x1);
  float y = static_cast<float>(y1);
  int lastInsideX = x1, lastInsideY = y1;

  for (int i = 1; i < stop; ++i) {
    x += dx;
    y += dy;

    if (!isInside(static_cast<int>(x), static_cast<int>(y))) {
      outX = lastInsideX;
      outY = lastInsideY;
      return;
    }
    lastInsideX = static_cast<int>(x);
    lastInsideY = static_cast<int>(y);
  }

  outX = x2;
  outY = y2;
}

} // namespace digitqt::core::tracing
