#include "ScanlineExtremumTracker.h"

#include "core/pipeline/stages/fringe_tracing/scanline_extremum/FringeConstructor.h"
#include "core/pipeline/stages/fringe_tracing/scanline_extremum/RedCenterDetector.h"

#include <QColor>

namespace digitqt::core::tracing {

bool ScanlineExtremumTracker::initialize(
    const QImage &image, std::function<bool(int, int)> isVisible) {
  if (image.isNull()) {
    m_lastError = QStringLiteral("Empty image");
    return false;
  }

  m_grayImage = image.convertToFormat(QImage::Format_Grayscale8);
  m_isVisible = std::move(isVisible);
  m_lastError.clear();
  return true;
}

std::vector<TracedLine> ScanlineExtremumTracker::extract(
    const std::vector<SeedPoint> & /*seeds*/) {
  // Global algorithm -- seeds are not used, see IFringeTracer's contract.
  std::vector<TracedLine> result;

  if (m_grayImage.isNull()) {
    m_lastError =
        QStringLiteral("Tracer not initialized. Call initialize() first.");
    return result;
  }

  scanline_extremum::DigitizationInput input;
  input.bitmapData = m_grayImage.constBits();
  input.imageWidth = m_grayImage.width();
  input.imageHeight = m_grayImage.height();
  input.bytesPerLine = static_cast<int>(m_grayImage.bytesPerLine());
  input.isVisible = m_isVisible;
  input.fringeCenterAs = m_params.fringeCenterAs;
  input.fringeStep = m_params.fringeStep;
  input.toleranceFactor = m_params.toleranceFactor;

  auto scanlines = scanline_extremum::RedCenterDetector::detectExtrema(input);
  if (scanlines.empty()) {
    m_lastError = QStringLiteral(
        "No extrema detected -- check the aperture and fringe contrast");
    return result;
  }

  auto fringes = scanline_extremum::FringeConstructor::constructFringes(
      scanlines, input.imageWidth, input.imageHeight, m_isVisible,
      m_params.fringeCenterAs, m_params.fringeStep, m_params.toleranceFactor);

  if (fringes.empty()) {
    m_lastError = QStringLiteral(
        "Extrema were detected but no continuous fringes could be constructed");
    return result;
  }

  result.reserve(fringes.size());
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
                         ? static_cast<float>(qGray(m_grayImage.pixel(px, py)))
                         : 0.0f;
      line.push_back(tp);
    }
    result.push_back(std::move(line));
  }

  m_lastError.clear();
  return result;
}

}  // namespace digitqt::core::tracing
