#include "PhaseReconstructionStage.h"

#include "core/Measurement.h"
#include "core/pipeline/stages/phase_reconstruction/FourierPhaseExtractor.h"
#include "core/pipeline/stages/phase_reconstruction/PhasePluginLoading.h"
#include "core/pipeline/stages/phase_reconstruction/PhaseReconstructor.h"
#include "core/pipeline/stages/phase_reconstruction/WaveletPhaseExtractor.h"

#include <algorithm>
#include <aperture/include/visibility/VisibilityChecker.h>
#include <memory>


namespace digitqt::core::pipeline {

namespace {
// Caps the solver grid's long side for speed. Keeps closely-spaced
// fringe lines from aliasing onto the same grid row/column, which would
// otherwise merge distinct fringe crossings in HorizontalSpline's
// per-row spline fit. Only applies to HorizontalSpline -- Fourier/
// Wavelet need real pixel intensities, not scaled line geometry, so they
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
  aperture::VisibilityChecker checker(measurement.boundaries());

  // Разрешение решения, предикат видимости в этих координатах и линии
  // (только для HorizontalSpline) -- единственная часть, которая всё
  // ещё зависит от конкретного алгоритма. Дальше -- один и тот же вызов
  // IPhaseReconstructor::reconstruct() для всех трёх, что и убирает
  // прежнее дублирование (Fourier/Wavelet-ветка + отдельная
  // HorizontalSpline-ветка).
  int gridWidth = 0;
  int gridHeight = 0;
  std::function<bool(int, int)> isVisible;
  std::vector<digitqt::core::NumberedFringeLine> lines;

  if (phaseAlgorithm == digitqt::core::PhaseReconstructionAlgorithm::HorizontalSpline) {
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
    // Fourier/Wavelet -- всегда в полном разрешении картинки, линии не
    // нужны (остаются пустыми, IPhaseReconstructor::reconstruct() это
    // допускает).
    gridWidth = measurement.image().width();
    gridHeight = measurement.image().height();
    isVisible = [&checker](int x, int y) {
      return checker.isVisible(aperture::Point{static_cast<double>(x), static_cast<double>(y)});
    };
  }

  // Плагин предпочтительнее встроенной реализации, если он есть рядом с
  // exe (<каталог exe>/plugins/phase/<algo>.dll) -- иначе молча падаем
  // на встроенный класс (в отличие от трассировки, где built-in больше
  // нет вообще -- здесь ещё есть, см. PhasePluginLoading.cpp).
  std::unique_ptr<digitqt::core::IPhaseReconstructor> reconstructor =
      digitqt::core::tryLoadPhaseReconstructorPlugin(phaseAlgorithm);
  if (!reconstructor) {
    switch (phaseAlgorithm) {
      case digitqt::core::PhaseReconstructionAlgorithm::HorizontalSpline:
        reconstructor = std::make_unique<PhaseReconstructor>();
        break;
      case digitqt::core::PhaseReconstructionAlgorithm::FourierTransform:
        reconstructor = std::make_unique<FourierPhaseExtractor>();
        break;
      case digitqt::core::PhaseReconstructionAlgorithm::WaveletTransform:
        reconstructor = std::make_unique<WaveletPhaseExtractor>();
        break;
    }
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
