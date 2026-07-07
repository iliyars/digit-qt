#include "SetupStage.h"

#include "core/Measurement.h"
#include "core/pipeline/stages/fringe_tracing/SeedStepTracer.h"

#include <aperture/include/visibility/VisibilityChecker.h>

namespace digitqt::core::pipeline {

bool SetupStage::doCompute(digitqt::core::Measurement &measurement,
                           QString &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = QStringLiteral("No image loaded");
    return false;
  }

  auto &tracingData = measurement.fringeTracing();
  if (tracingData.seeds().empty()) {
    errorMessage =
        QStringLiteral("No seed points placed. Click on the image to add one.");
    return false;
  }

  // Bridge our (multi-shape) aperture to the tracer's isVisible(x,y)
  // predicate -- see IFringeTracer's contract.
  aperture::VisibilityChecker checker(measurement.boundaries());
  auto isVisible = [&checker](int x, int y) {
    return checker.isVisible(
        aperture::Point{static_cast<double>(x), static_cast<double>(y)});
  };

  tracing::SeedStepTracer tracer;
  if (!tracer.initialize(measurement.image(), isVisible)) {
    errorMessage = tracer.lastError();
    return false;
  }

  auto lines = tracer.extract(tracingData.seeds());
  tracingData.tracedLines() = std::move(lines);

  if (tracingData.tracedLines().empty()) {
    errorMessage = tracer.lastError().isEmpty()
                       ? QStringLiteral("Tracing produced no lines")
                       : tracer.lastError();
    return false;
  }

  return true;
}

} // namespace digitqt::core::pipeline
