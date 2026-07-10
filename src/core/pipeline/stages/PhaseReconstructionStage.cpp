#include "PhaseReconstructionStage.h"

#include "core/Measurement.h"
#include "core/pipeline/stages/phase_reconstruction/PhaseReconstructor.h"

#include <algorithm>
#include <aperture/include/visibility/VisibilityChecker.h>


namespace digitqt::core::pipeline {

namespace {
constexpr int kMaxGridDimension = 400;
}

bool PhaseReconstructionStage::doCompute(digitqt::core::Measurement &measurement,
                                         QString &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = QStringLiteral("No image loaded");
    return false;
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
