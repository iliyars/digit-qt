#include "ScanlineExtremumTracker.h"

#include "core/pipeline/stages/fringe_tracing/scanline_extremum/FringeConstructor.h"
#include "core/pipeline/stages/fringe_tracing/scanline_extremum/RedCenterDetector.h"

#include <algorithm>
#include <cmath>

namespace digitqt::core::tracing {

namespace {

// The legacy Digit app never emitted one point per scanned row: its
// CreateZAPSections() picked a small, fixed COUNT of section rows --
// proportional to fringe count and aspect ratio, not to image
// resolution -- and PutDotsOnZAPSections() placed one point per fringe
// per section. RedCenterDetector/FringeConstructor here scan every row
// with no equivalent decimation, so a straight port emits an order of
// magnitude more points than the legacy app ever produced for the same
// image. Mirror the legacy row-spacing formula to match its point
// density instead.
int minRowGapForDecimation(int fringeCount, int imageWidth, int imageHeight) {
  if (fringeCount < 1)
    fringeCount = 1;
  if (imageWidth < 1 || imageHeight < 1)
    return 1;
  const double nSections =
      std::max(2.0, 2.0 * fringeCount * imageHeight / imageWidth);
  const int gap = static_cast<int>(std::ceil(imageHeight / (nSections - 1.0)));
  return std::max(gap, 1);
}

// Keeps a fringe's first and last point (endpoint fidelity, e.g. at
// aperture edges/obstructions) and thins the rest to at most one point
// per minRowGap rows, in original row order.
TracedLine decimateLine(const TracedLine &line, int minRowGap) {
  if (minRowGap <= 1 || line.size() < 3)
    return line;

  TracedLine out;
  out.reserve(line.size() / static_cast<size_t>(minRowGap) + 2);
  out.push_back(line.front());
  double lastKeptY = line.front().y;
  for (size_t i = 1; i + 1 < line.size(); ++i) {
    if (std::abs(line[i].y - lastKeptY) >= minRowGap) {
      out.push_back(line[i]);
      lastKeptY = line[i].y;
    }
  }
  out.push_back(line.back());
  return out;
}

}  // namespace

bool ScanlineExtremumTracker::setParam(const std::string &key, const std::string &value) {
  if (key == "fringeCenterMode") {
    if (value == "max")
      m_params.fringeCenterAs = scanline_extremum::FringeCenterMode::Max;
    else if (value == "min")
      m_params.fringeCenterAs = scanline_extremum::FringeCenterMode::Min;
    else if (value == "minmax")
      m_params.fringeCenterAs = scanline_extremum::FringeCenterMode::MinMax;
    else
      return false;
    return true;
  }
  if (key == "hasInternalObstruction") {
    m_params.hasInternalObstruction = (value == "1" || value == "true");
    return true;
  }
  return false;
}

bool ScanlineExtremumTracker::initialize(
    const digitqt::core::Bitmap &image, std::function<bool(int, int)> isVisible) {
  if (image.isNull()) {
    m_lastError = "Empty image";
    return false;
  }

  m_grayImage = image;
  m_isVisible = std::move(isVisible);
  m_lastError.clear();
  return true;
}

std::vector<TracedLine> ScanlineExtremumTracker::extract(
    const std::vector<SeedPoint> & /*seeds*/) {
  // Global algorithm -- seeds are not used, see IFringeTracer's contract.
  std::vector<TracedLine> result;
  m_lastFringeNumbers.clear();

  if (m_grayImage.isNull()) {
    m_lastError = "Tracer not initialized. Call initialize() first.";
    return result;
  }

  scanline_extremum::DigitizationInput input;
  input.bitmapData = m_grayImage.data();
  input.imageWidth = m_grayImage.width();
  input.imageHeight = m_grayImage.height();
  input.bytesPerLine = m_grayImage.width();
  input.isVisible = m_isVisible;
  input.fringeCenterAs = m_params.fringeCenterAs;
  input.fringeStep = m_params.fringeStep;
  input.toleranceFactor = m_params.toleranceFactor;

  auto scanlines = scanline_extremum::RedCenterDetector::detectExtrema(input);
  if (scanlines.empty()) {
    m_lastError = "No extrema detected -- check the aperture and fringe contrast";
    return result;
  }

  auto fringes = scanline_extremum::FringeConstructor::constructFringes(
      scanlines, input.imageWidth, input.imageHeight, m_isVisible,
      m_params.fringeCenterAs, m_params.fringeStep, m_params.toleranceFactor,
      m_params.hasInternalObstruction);

  if (fringes.empty()) {
    m_lastError = "Extrema were detected but no continuous fringes could be constructed";
    return result;
  }

  const int minRowGap = minRowGapForDecimation(
      static_cast<int>(fringes.size()), input.imageWidth, input.imageHeight);

  result.reserve(fringes.size());
  m_lastFringeNumbers.reserve(fringes.size());
  for (const auto &fringe : fringes) {
    TracedLine line;
    line.reserve(fringe.points.size());
    for (const auto &p : fringe.points) {
      TracedPoint tp;
      tp.x = p.x;
      tp.y = p.y;
      tp.width = 0.0f;
      const int px = static_cast<int>(p.x + 0.5);
      const int py = static_cast<int>(p.y + 0.5);
      tp.intensity = (px >= 0 && px < m_grayImage.width() && py >= 0 &&
                      py < m_grayImage.height())
                         ? static_cast<float>(m_grayImage.pixel(px, py))
                         : 0.0f;
      line.push_back(tp);
    }
    result.push_back(decimateLine(line, minRowGap));
    m_lastFringeNumbers.push_back(fringe.number);
  }

  m_lastError.clear();
  return result;
}

}  // namespace digitqt::core::tracing
