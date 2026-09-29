#pragma once

#include "core/Bitmap.h"
#include "core/NumberedFringeLine.h"
#include "core/PhaseMap.h"

#include <functional>
#include <string>
#include <vector>

namespace digitqt::core {

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
