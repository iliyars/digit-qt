#include "SetupStage.h"

#include "core/FringeOrdering.h"
#include "core/Measurement.h"
#include "core/pipeline/stages/fringe_tracing/TracerPluginLoading.h"
#include "core/pipeline/stages/phase_reconstruction/DllPhaseReconstructor.h"

#include <aperture/include/visibility/VisibilityChecker.h>
#include <memory>

namespace digitqt::core::pipeline {

namespace {

// Нужна ли для выбранного метода сшивки фазы трассировка полос (S1),
// прежде чем что-либо в этой функции запускать. Для одного из 3
// известных алгоритмов -- по enum, как и раньше. Для явно выбранного
// стороннего плагина (см. Measurement::customPhaseReconstructorPluginPath())
// -- спрашиваем сам плагин (ABI v2, DqtPhasePluginInfo::needsFringeLines),
// а не гадаем: хост ничего не знает заранее про чужой плагин. Загрузка
// здесь -- только ради этого одного флага; PhaseReconstructionStage.cpp
// загружает тот же .dll заново для самого расчёта (см. там же, почему
// это не проблема -- Win32Library не выгружает DLL, повторный
// LoadLibraryW на уже отображённый файл дёшев).
bool phaseReconstructionNeedsFringeLines(const digitqt::core::Measurement &measurement) {
  const auto &customPath = measurement.customPhaseReconstructorPluginPath();
  if (!customPath.empty()) {
    std::string loadError;
    auto custom = digitqt::core::DllPhaseReconstructor::load(customPath, loadError);
    // Если кастомный плагин не загрузился вообще -- решение всё равно
    // примет PhaseReconstructionStage (там это настоящая ошибка), здесь
    // же безопаснее по умолчанию трассировать, чем молча пропустить S1.
    return !custom || custom->needsFringeLines();
  }
  return measurement.phaseReconstructionAlgorithm() ==
         digitqt::core::PhaseReconstructionAlgorithm::HorizontalSpline;
}

}  // namespace

bool SetupStage::doCompute(digitqt::core::Measurement &measurement, std::string &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = "No image loaded";
    return false;
  }

  // Методы Фурье- и вейвлет-анализа полос (и любой сторонний плагин,
  // заявивший needsFringeLines=false) работают прямо по изображению +
  // маске апертуры, им не нужны пронумерованные линии -- трассировка
  // здесь просто не запускается.
  if (!phaseReconstructionNeedsFringeLines(measurement)) {
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

  // Ни у одного из 4 алгоритмов трассировки больше нет встроенной
  // реализации в core -- все они полностью вынесены в самодостаточные
  // DqtFringeTracer C ABI-плагины (plugin-sdk/*_tracker_plugin/, см.
  // историю в памяти qt_decoupling_and_plugin_abi). Отсутствие нужного
  // .dll рядом с exe -- настоящая ошибка, не повод для fallback (держать
  // встроенную копию "на всякий случай" воспроизвело бы именно ту
  // дублирующуюся реализацию, ради устранения которой всё это делалось).
  std::unique_ptr<tracing::IFringeTracer> tracer = tracing::tryLoadTracerPlugin(algorithm);
  if (!tracer) {
    errorMessage = "Tracer plugin not found or failed to load "
                   "(plugins/tracers/ next to the executable) -- this algorithm has no "
                   "built-in implementation.";
    return false;
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

  // Some algorithms (ScanlineExtremumTracker's plugin) already compute a
  // real, globally-consistent fringe number per line (FringeConstructor's
  // chain propagation from a single main-scanline seed) -- use it
  // directly instead of the generic mean-X fallback, which would
  // silently discard it and get curved or obstruction-interrupted
  // fringes wrong. lastFringeOrders() is empty for tracers that don't
  // compute one (the default, see IFringeTracer::lastFringeOrders()).
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
