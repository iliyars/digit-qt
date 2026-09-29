#include "ScanlineExtremumTracker.h"

#include "FringeConstructor.h"
#include "RedCenterDetector.h"

#include <algorithm>
#include <cmath>

namespace scanline_extremum_plugin {

namespace {

// Порт из core::tracing::ScanlineExtremumTracker.cpp -- см. там же
// объяснение (легаси CreateZAPSections()/PutDotsOnZAPSections() не
// эмитили точку на каждую сканированную строку, а RedCenterDetector/
// FringeConstructor здесь эмитят; децимация повторяет плотность легаси).
int minRowGapForDecimation(int fringeCount, int imageWidth, int imageHeight) {
  if (fringeCount < 1)
    fringeCount = 1;
  if (imageWidth < 1 || imageHeight < 1)
    return 1;
  const double nSections = std::max(2.0, 2.0 * fringeCount * imageHeight / imageWidth);
  const int gap = static_cast<int>(std::ceil(imageHeight / (nSections - 1.0)));
  return std::max(gap, 1);
}

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
      m_params.fringeCenterAs = FringeCenterMode::Max;
    else if (value == "min")
      m_params.fringeCenterAs = FringeCenterMode::Min;
    else if (value == "minmax")
      m_params.fringeCenterAs = FringeCenterMode::MinMax;
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

bool ScanlineExtremumTracker::initialize(const uint8_t *pixels, int width, int height,
                                         std::function<bool(int, int)> isVisible) {
  if (!pixels || width <= 0 || height <= 0) {
    m_lastError = "Empty image";
    return false;
  }
  m_pixelStorage.assign(pixels, pixels + static_cast<size_t>(width) * height);
  m_image = m_pixelStorage.data();
  m_width = width;
  m_height = height;
  m_isVisible = std::move(isVisible);
  m_lastError.clear();
  return true;
}

std::vector<TracedLine> ScanlineExtremumTracker::extract(
    const std::vector<SeedPoint> & /*seeds*/) {
  std::vector<TracedLine> result;
  m_lastFringeNumbers.clear();

  if (!m_image) {
    m_lastError = "Tracer not initialized. Call initialize() first.";
    return result;
  }

  DigitizationInput input;
  input.bitmapData = m_image;
  input.imageWidth = m_width;
  input.imageHeight = m_height;
  input.bytesPerLine = m_width;
  input.isVisible = m_isVisible;
  input.fringeCenterAs = m_params.fringeCenterAs;
  input.fringeStep = m_params.fringeStep;
  input.toleranceFactor = m_params.toleranceFactor;

  auto scanlines = RedCenterDetector::detectExtrema(input);
  if (scanlines.empty()) {
    m_lastError = "No extrema detected -- check the aperture and fringe contrast";
    return result;
  }

  auto fringes = FringeConstructor::constructFringes(
      scanlines, input.imageWidth, input.imageHeight, m_isVisible, m_params.fringeCenterAs,
      m_params.fringeStep, m_params.toleranceFactor, m_params.hasInternalObstruction);

  if (fringes.empty()) {
    m_lastError = "Extrema were detected but no continuous fringes could be constructed";
    return result;
  }

  const int minRowGap =
      minRowGapForDecimation(static_cast<int>(fringes.size()), input.imageWidth, input.imageHeight);

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
      tp.intensity = (px >= 0 && px < m_width && py >= 0 && py < m_height)
                         ? static_cast<float>(m_image[py * m_width + px])
                         : 0.0f;
      line.push_back(tp);
    }
    result.push_back(decimateLine(line, minRowGap));
    m_lastFringeNumbers.push_back(fringe.number);
  }

  m_lastError.clear();
  return result;
}

}  // namespace scanline_extremum_plugin
