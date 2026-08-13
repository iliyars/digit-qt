#include "PhaseReconstructionStage.h"

#include "core/Measurement.h"
#include "core/pipeline/stages/phase_reconstruction/FourierPhaseExtractor.h"
#include "core/pipeline/stages/phase_reconstruction/PhaseReconstructor.h"
#include "core/pipeline/stages/phase_reconstruction/WaveletPhaseExtractor.h"

#include <algorithm>
#include <aperture/include/visibility/VisibilityChecker.h>


namespace digitqt::core::pipeline {

namespace {
// Caps the solver grid's long side for speed. Keeps closely-spaced
// fringe lines from aliasing onto the same grid row/column, which would
// otherwise merge distinct fringe crossings in PhaseReconstructor's
// per-row spline fit. Only applies to HorizontalSpline -- FourierPhase
// Extractor needs real pixel intensities, not scaled line geometry, so
// it always runs at native image resolution.
constexpr int kMaxGridDimension = 5000;
}

bool PhaseReconstructionStage::doCompute(digitqt::core::Measurement &measurement,
                                         QString &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = QStringLiteral("No image loaded");
    return false;
  }

  const auto phaseAlgorithm = measurement.phaseReconstructionAlgorithm();
  if (phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::FourierTransform ||
      phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::WaveletTransform) {
    aperture::VisibilityChecker checker(measurement.boundaries());
    auto isVisible = [&checker](int x, int y) {
      return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
    };

    bool ok = false;
    QString extractError;
    digitqt::core::PhaseMap phaseMap;
    if (phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::FourierTransform) {
      FourierPhaseExtractor extractor;
      auto result = extractor.extract(measurement.image(), isVisible);
      ok = result.ok;
      extractError = result.errorMessage;
      phaseMap = std::move(result.phaseMap);
    } else {
      WaveletPhaseExtractor extractor;
      auto result = extractor.extract(measurement.image(), isVisible);
      ok = result.ok;
      extractError = result.errorMessage;
      phaseMap = std::move(result.phaseMap);
    }

    if (!ok) {
      errorMessage = extractError.isEmpty() ? QStringLiteral("Phase reconstruction failed")
                                            : extractError;
      return false;
    }

    measurement.phaseMap() = std::move(phaseMap);
    return true;
  }

  const auto &lines = measurement.fringeTracing().tracedLines();
  if (lines.empty()) {
    errorMessage = QStringLiteral("No numbered fringe lines. Trace fringes first (Setup stage).");
    return false;
  }

  const int imgWidth = measurement.image().width();
  const int imgHeight = measurement.image().height();
  const int longSide = std::max(imgWidth, imgHeight);

  const double scale =
      (longSide > kMaxGridDimension) ? (static_cast<double>(kMaxGridDimension) / longSide) : 1.0;
  const int gridWidth = std::max(1, static_cast<int>(imgWidth * scale));
  const int gridHeight = std::max(1, static_cast<int>(imgHeight * scale));

  aperture::VisibilityChecker checker(measurement.boundaries());
  auto isVisibleGrid = [&checker, scale](int gx, int gy) {
    const double ix = gx / scale;
    const double iy = gy / scale;
    return checker.isVisible(aperture::Point{ix, iy});
  };

  // Пронумерованные линии -- в координатах сетки решения.
  std::vector<digitqt::core::NumberedFringeLine> scaledLines;
  scaledLines.reserve(lines.size());
  for (const auto &line : lines) {
    digitqt::core::NumberedFringeLine scaled;
    scaled.order = line.order;
    scaled.orderIsManual = line.orderIsManual;
    scaled.points.reserve(line.points.size());
    for (const auto &p : line.points) {
      tracing::TracedPoint sp = p;
      sp.x = p.x * scale;
      sp.y = p.y * scale;
      scaled.points.push_back(sp);
    }
    scaledLines.push_back(std::move(scaled));
  }

  PhaseReconstructor reconstructor;
  auto phaseMap = reconstructor.reconstruct(gridWidth, gridHeight, isVisibleGrid, scaledLines);

  if (phaseMap.isEmpty()) {
    errorMessage = reconstructor.lastError().isEmpty()
                       ? QStringLiteral("Phase reconstruction failed")
                       : reconstructor.lastError();
    return false;
  }

  measurement.phaseMap() = std::move(phaseMap);
  return true;
}

}  // namespace digitqt::core::pipeline
