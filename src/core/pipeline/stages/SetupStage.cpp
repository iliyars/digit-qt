#include "SetupStage.h"

#include "core/FringeOrdering.h"
#include "core/Measurement.h"
#include "core/pipeline/stages/fringe_tracing/BinaryThinningTracker.h"
#include "core/pipeline/stages/fringe_tracing/ScanlineExtremumTracker.h"
#include "core/pipeline/stages/fringe_tracing/SequentialFringeTracker.h"
#include "core/pipeline/stages/fringe_tracing/StructureTensorTracker.h"

#include <aperture/include/visibility/VisibilityChecker.h>
#include <memory>

namespace digitqt::core::pipeline {

bool SetupStage::doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = QStringLiteral("No image loaded");
    return false;
  }

  // Методы Фурье- и вейвлет-анализа полос (S2) работают прямо по
  // изображению + маске апертуры, им не нужны пронумерованные линии --
  // трассировка здесь просто не запускается.
  const auto phaseAlgorithm = measurement.phaseReconstructionAlgorithm();
  if (phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::FourierTransform ||
      phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::WaveletTransform) {
    measurement.fringeTracing().tracedLines().clear();
    return true;
  }

  auto &tracingData = measurement.fringeTracing();
  const auto algorithm = tracingData.algorithm();
  const bool needsSeeds = (algorithm == digitqt::core::TracerAlgorithm::SequentialTracking ||
                           algorithm == digitqt::core::TracerAlgorithm::StructureTensor);

  if (needsSeeds && tracingData.seeds().empty()) {
    errorMessage = QStringLiteral("No seed points placed. Click on the image to add one.");
    return false;
  }

  // Bridge our (multi-shape) aperture to the tracer's isVisible(x,y)
  // predicate -- see IFringeTracer's contract.
  aperture::VisibilityChecker checker(measurement.boundaries());
  auto isVisible = [&checker](int x, int y) {
    return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
  };

  std::unique_ptr<tracing::IFringeTracer> tracer;
  // Set only for TracerAlgorithm::ScanlineExtremum -- lets us read back its
  // FringeConstructor-computed fringe numbers after extract() without
  // widening the shared IFringeTracer contract for every other algorithm.
  tracing::ScanlineExtremumTracker *scanlineTracerRaw = nullptr;
  switch (algorithm) {
    case digitqt::core::TracerAlgorithm::SequentialTracking:
      tracer = std::make_unique<tracing::SequentialFringeTracker>();
      break;
    case digitqt::core::TracerAlgorithm::StructureTensor:
      tracer = std::make_unique<tracing::StructureTensorTracker>();
      break;
    case digitqt::core::TracerAlgorithm::ScanlineExtremum: {
      auto scanlineTracer = std::make_unique<tracing::ScanlineExtremumTracker>();

      tracing::ScanlineExtremumTracker::Params params;
      switch (tracingData.fringeCenterMode()) {
        case digitqt::core::FringeCenterMode::Max:
          params.fringeCenterAs = tracing::scanline_extremum::FringeCenterMode::Max;
          break;
        case digitqt::core::FringeCenterMode::Min:
          params.fringeCenterAs = tracing::scanline_extremum::FringeCenterMode::Min;
          break;
        case digitqt::core::FringeCenterMode::MinMax:
          params.fringeCenterAs = tracing::scanline_extremum::FringeCenterMode::MinMax;
          break;
      }
      params.hasInternalObstruction = !measurement.boundaries().getInternal().empty();
      scanlineTracer->setParams(params);

      scanlineTracerRaw = scanlineTracer.get();
      tracer = std::move(scanlineTracer);
      break;
    }
    case digitqt::core::TracerAlgorithm::BinaryThinning:
      tracer = std::make_unique<tracing::BinaryThinningTracker>();
      break;
  }

  if (!tracer->initialize(measurement.image(), isVisible)) {
    errorMessage = tracer->lastError();
    return false;
  }

  auto lines = tracer->extract(tracingData.seeds());

  std::vector<digitqt::core::NumberedFringeLine> numberedLines;
  numberedLines.reserve(lines.size());
  for (auto &line : lines) {
    digitqt::core::NumberedFringeLine numbered;
    numbered.points = std::move(line);
    numberedLines.push_back(std::move(numbered));
  }

  // ScanlineExtremumTracker already computed a real, globally-consistent
  // fringe number per line (FringeConstructor's chain propagation from a
  // single main-scanline seed) -- use it directly instead of the generic
  // mean-X fallback, which would silently discard it and get curved or
  // obstruction-interrupted fringes wrong.
  if (scanlineTracerRaw != nullptr) {
    const auto &numbers = scanlineTracerRaw->lastFringeNumbers();
    if (numbers.size() == numberedLines.size()) {
      for (size_t i = 0; i < numberedLines.size(); ++i) {
        if (!numberedLines[i].orderIsManual)
          numberedLines[i].order = numbers[i];
      }
    } else {
      digitqt::core::autoAssignFringeOrder(numberedLines);
    }
  } else {
    digitqt::core::autoAssignFringeOrder(numberedLines);
  }
  tracingData.tracedLines() = std::move(numberedLines);

  if (tracingData.tracedLines().empty()) {
    errorMessage = tracer->lastError().isEmpty() ? QStringLiteral("Tracing produced no lines")
                                                 : tracer->lastError();
    return false;
  }

  return true;
}

}  // namespace digitqt::core::pipeline
