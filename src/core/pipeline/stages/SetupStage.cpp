#include "SetupStage.h"

#include "core/FringeOrdering.h"
#include "core/Measurement.h"
#include "core/pipeline/stages/fringe_tracing/BinaryThinningTracker.h"
#include "core/pipeline/stages/fringe_tracing/ScanlineExtremumTracker.h"
#include "core/pipeline/stages/fringe_tracing/StructureTensorTracker.h"
#include "core/pipeline/stages/fringe_tracing/TracerPluginLoading.h"

#include <aperture/include/visibility/VisibilityChecker.h>
#include <memory>

namespace digitqt::core::pipeline {

bool SetupStage::doCompute(digitqt::core::Measurement &measurement, std::string &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = "No image loaded";
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
    errorMessage = "No seed points placed. Click on the image to add one.";
    return false;
  }

  // Bridge our (multi-shape) aperture to the tracer's isVisible(x,y)
  // predicate -- see IFringeTracer's contract.
  aperture::VisibilityChecker checker(measurement.boundaries());
  auto isVisible = [&checker](int x, int y) {
    return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
  };

  // Плагин предпочтительнее встроенной реализации, если он есть рядом с
  // exe (<каталог exe>/plugins/tracers/<algo>.dll) -- иначе молча падаем
  // на встроенный класс (см. tryLoadTracerPlugin()). Плагин при этом
  // полностью неотличим для остального SetupStage от встроенного
  // трекера -- один и тот же IFringeTracer* дальше по коду.
  //
  // Исключение -- SequentialTracking: его реализация целиком вынесена в
  // sequential_fringe_tracker_plugin (см. AskUserQuestion-решение в
  // qt_decoupling_and_plugin_abi.md), встроенной копии в core больше нет.
  // Если плагина нет -- это настоящая ошибка, а не повод для fallback.
  std::unique_ptr<tracing::IFringeTracer> tracer = tracing::tryLoadTracerPlugin(algorithm);
  if (!tracer && algorithm == digitqt::core::TracerAlgorithm::SequentialTracking) {
    errorMessage =
        "Sequential Fringe Tracking plugin not found "
        "(plugins/tracers/sequential_fringe_tracker.dll) -- this algorithm has no "
        "built-in fallback.";
    return false;
  }
  if (!tracer) {
    switch (algorithm) {
      case digitqt::core::TracerAlgorithm::SequentialTracking:
        break;  // unreachable -- handled above
      case digitqt::core::TracerAlgorithm::StructureTensor:
        tracer = std::make_unique<tracing::StructureTensorTracker>();
        break;
      case digitqt::core::TracerAlgorithm::ScanlineExtremum:
        tracer = std::make_unique<tracing::ScanlineExtremumTracker>();
        break;
      case digitqt::core::TracerAlgorithm::BinaryThinning:
        tracer = std::make_unique<tracing::BinaryThinningTracker>();
        break;
    }
  }

  // Параметры передаются единообразно всем трекерам (встроенным и
  // плагинам) через generic setParam() -- алгоритмы, которым ключ не
  // нужен, просто его игнорируют (см. IFringeTracer::setParam()). Так
  // SetupStage не должен знать конкретный тип tracer, что и позволяет
  // плагину быть неотличимым от встроенного.
  switch (tracingData.fringeCenterMode()) {
    case digitqt::core::FringeCenterMode::Max:
      tracer->setParam("fringeCenterMode", "max");
      break;
    case digitqt::core::FringeCenterMode::Min:
      tracer->setParam("fringeCenterMode", "min");
      break;
    case digitqt::core::FringeCenterMode::MinMax:
      tracer->setParam("fringeCenterMode", "minmax");
      break;
  }
  tracer->setParam("hasInternalObstruction",
                   measurement.boundaries().getInternal().empty() ? "0" : "1");

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

  // Some algorithms (ScanlineExtremumTracker, both built-in and plugin)
  // already compute a real, globally-consistent fringe number per line
  // (FringeConstructor's chain propagation from a single main-scanline
  // seed) -- use it directly instead of the generic mean-X fallback,
  // which would silently discard it and get curved or
  // obstruction-interrupted fringes wrong. lastFringeOrders() is empty
  // for tracers that don't compute one (the default, see
  // IFringeTracer::lastFringeOrders()).
  const auto orders = tracer->lastFringeOrders();
  if (orders.size() == numberedLines.size()) {
    for (size_t i = 0; i < numberedLines.size(); ++i) {
      if (!numberedLines[i].orderIsManual)
        numberedLines[i].order = orders[i];
    }
  } else {
    digitqt::core::autoAssignFringeOrder(numberedLines);
  }
  tracingData.tracedLines() = std::move(numberedLines);

  if (tracingData.tracedLines().empty()) {
    errorMessage = tracer->lastError().empty() ? "Tracing produced no lines"
                                               : tracer->lastError();
    return false;
  }

  return true;
}

}  // namespace digitqt::core::pipeline
