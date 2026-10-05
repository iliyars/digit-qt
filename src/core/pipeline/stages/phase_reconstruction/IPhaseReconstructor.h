#pragma once

#include "core/Bitmap.h"
#include "core/NumberedFringeLine.h"
#include "core/PhaseMap.h"

#include <functional>
#include <string>
#include <vector>

namespace digitqt::core {

/**
 * @brief Общий контракт методов сшивки фазы (S2).
 *
 * Как и у IFringeTracer, в core не осталось ни одной встроенной
 * реализации -- все 3 метода самодостаточны и живут только как
 * DqtPhaseReconstructor C ABI-плагины, загружаются
 * PhasePluginLoading.h в рантайме. Отсутствие нужного .dll --
 * настоящая ошибка от PhaseReconstructionStage, не fallback:
 *   - HorizontalSplinePhaseReconstructor (plugin-sdk/horizontal_spline_phase_plugin/):
 *     построчная кубическая сплайн-интерполяция по пронумерованным
 *     линиям полос, порт WavefrontFromContoursSolver_HorizontalSpline.
 *   - FourierPhaseExtractor (plugin-sdk/fourier_phase_plugin/):
 *     метод Фурье-анализа (Такеда, 1982), прямо по пикселям картинки.
 *   - WaveletPhaseExtractor (plugin-sdk/wavelet_phase_plugin/):
 *     непрерывный вейвлет-анализ (Zhong & Weng, 2004), тоже прямо по
 *     пикселям.
 */
class IPhaseReconstructor {
public:
  virtual ~IPhaseReconstructor() = default;

  /// gridWidth/gridHeight -- разрешение, в котором нужно посчитать карту
  /// фазы. image -- пиксели источника (Fourier/Wavelet ДОЛЖНЫ получить
  /// gridWidth==image.width()/gridHeight==image.height() и вернут ошибку
  /// иначе; HorizontalSpline пиксели не трогает вообще, только dimensions
  /// самого image ей и не нужны -- у неё есть отдельная сетка).
  /// isVisible -- предикат в координатах (gridWidth x gridHeight).
  /// lines -- пронумерованные линии, тоже в координатах (gridWidth x
  /// gridHeight); Fourier/Wavelet их игнорируют.
  virtual PhaseMap reconstruct(int gridWidth, int gridHeight,
                                               const digitqt::core::Bitmap &image,
                                               std::function<bool(int, int)> isVisible,
                                               const std::vector<NumberedFringeLine> &lines) = 0;
  virtual std::string name() const = 0;
  virtual const std::string &lastError() const = 0;
};

}  // namespace digitqt::core
