#include "PhaseReconstructionStage.h"

#include "core/Measurement.h"
#include "core/pipeline/stages/phase_reconstruction/DllPhaseReconstructor.h"
#include "core/pipeline/stages/phase_reconstruction/PhasePluginLoading.h"

#include <algorithm>
#include <aperture/include/visibility/VisibilityChecker.h>
#include <memory>


namespace digitqt::core::pipeline {

namespace {
// Caps the solver grid's long side for speed. Keeps closely-spaced
// fringe lines from aliasing onto the same grid row/column, which would
// otherwise merge distinct fringe crossings in HorizontalSpline's
// per-row spline fit. Only applies to line-based methods -- image-based
// ones need real pixel intensities, not scaled line geometry, so they
// always run at native image resolution (IPhaseReconstructor::
// reconstruct() requires gridWidth/gridHeight == image dimensions for
// them, see each plugin's reconstruct()).
constexpr int kMaxGridDimension = 5000;
}

bool PhaseReconstructionStage::doCompute(digitqt::core::Measurement &measurement,
                                         std::string &errorMessage) {
  if (!measurement.hasImage()) {
    errorMessage = "No image loaded";
    return false;
  }

  const auto phaseAlgorithm = measurement.phaseReconstructionAlgorithm();
  const auto &customPluginPath = measurement.customPhaseReconstructorPluginPath();

  // Ни у одного из 3 методов сшивки фазы больше нет встроенной
  // реализации в core -- все вынесены в самодостаточные
  // DqtPhaseReconstructor C ABI-плагины (plugin-sdk/*_phase_plugin/, см.
  // историю в памяти qt_decoupling_and_plugin_abi). Отсутствие нужного
  // .dll -- настоящая ошибка, не повод для fallback (та же причина, что
  // и у трассировки -- держать встроенную копию "на всякий случай"
  // воспроизвело бы именно ту дублирующуюся реализацию, ради устранения
  // которой всё это делалось).
  //
  // needsLines решает, какую сетку готовить ниже -- из ABI v2
  // needsFringeLines кастомного плагина, либо (для известных 3) по enum.
  std::unique_ptr<digitqt::core::IPhaseReconstructor> reconstructor;
  bool needsLines = false;

  if (!customPluginPath.empty()) {
    std::string loadError;
    auto custom = digitqt::core::DllPhaseReconstructor::load(customPluginPath, loadError);
    if (!custom) {
      errorMessage = "Custom phase reconstructor plugin failed to load (" + customPluginPath +
                     "): " + loadError;
      return false;
    }
    needsLines = custom->needsFringeLines();
    reconstructor = std::move(custom);
  } else {
    needsLines = (phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::HorizontalSpline);
    reconstructor = digitqt::core::tryLoadPhaseReconstructorPlugin(phaseAlgorithm);
    if (!reconstructor) {
      errorMessage = "Phase reconstructor plugin not found or failed to load "
                     "(plugins/phase/ next to the executable) -- this algorithm has no "
                     "built-in implementation.";
      return false;
    }
  }

  aperture::VisibilityChecker checker(measurement.boundaries());

  // Разрешение решения, предикат видимости в этих координатах и линии
  // (только если needsLines) -- единственная часть, которая всё ещё
  // зависит от конкретной реализации. Дальше -- один и тот же вызов
  // IPhaseReconstructor::reconstruct(), что и убирает прежнее
  // дублирование (Fourier/Wavelet-ветка + отдельная HorizontalSpline-ветка).
  int gridWidth = 0;
  int gridHeight = 0;
  std::function<bool(int, int)> isVisible;
  std::vector<digitqt::core::NumberedFringeLine> lines;

  if (needsLines) {
    const auto &tracedLines = measurement.fringeTracing().tracedLines();
    if (tracedLines.empty()) {
      errorMessage = "No numbered fringe lines. Trace fringes first (Setup stage).";
      return false;
    }

    const int imgWidth = measurement.image().width();
    const int imgHeight = measurement.image().height();
    const int longSide = std::max(imgWidth, imgHeight);
    const double scale =
        (longSide > kMaxGridDimension) ? (static_cast<double>(kMaxGridDimension) / longSide) : 1.0;
    gridWidth = std::max(1, static_cast<int>(imgWidth * scale));
    gridHeight = std::max(1, static_cast<int>(imgHeight * scale));

    isVisible = [&checker, scale](int gx, int gy) {
      const double ix = gx / scale;
      const double iy = gy / scale;
      return checker.isVisible(aperture::Point{ix, iy});
    };

    // Пронумерованные линии -- в координатах сетки решения.
    lines.reserve(tracedLines.size());
    for (const auto &line : tracedLines) {
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
      lines.push_back(std::move(scaled));
    }
  } else {
    // Image-based методы (Fourier/Wavelet, либо сторонний плагин с
    // needsFringeLines=false) -- всегда в полном разрешении картинки,
    // линии не нужны (остаются пустыми, IPhaseReconstructor::
    // reconstruct() это допускает).
    gridWidth = measurement.image().width();
    gridHeight = measurement.image().height();
    isVisible = [&checker](int x, int y) {
      return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
    };
  }

  auto phaseMap =
      reconstructor->reconstruct(gridWidth, gridHeight, measurement.image(), isVisible, lines);

  if (phaseMap.isEmpty()) {
    errorMessage = reconstructor->lastError().empty() ? "Phase reconstruction failed"
                                                       : reconstructor->lastError();
    return false;
  }

  measurement.phaseMap() = std::move(phaseMap);
  return true;
}

}  // namespace digitqt::core::pipeline
