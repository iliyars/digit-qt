#include "WavefrontReconstructionStage.h"

#include "core/Measurement.h"

namespace digitqt::core::pipeline {

bool WavefrontReconstructionStage::doCompute(digitqt::core::Measurement &measurement,
                                             QString &errorMessage) {
  const auto &phase = measurement.phaseMap();
  if (phase.isEmpty()) {
    errorMessage = QStringLiteral("No phase map. Run Phase Reconstruction (S2) first.");
    return false;
  }

  if (measurement.wavelengthNm() <= 0.0) {
    errorMessage = QStringLiteral("Wavelength must be positive");
    return false;
  }

  // Двойной проход (тест на отражение): свет идёт через неровность
  // поверхности туда и обратно, поэтому одна и та же высота поверхности
  // вносит вдвое больший набег фазы -- значит высота = порядок × λ/2.
  // Одинарный проход (тест на просвет): измеренная величина УЖЕ прямо
  // аберрация волнового фронта, без этого дополнительного вдвое -- высота
  // = порядок × λ. См. Measurement::isDoublePass().
  const double heightPerOrder =
      measurement.isDoublePass() ? measurement.wavelengthNm() / 2 : measurement.wavelengthNm();

  PhaseMap wavefront(phase.width(), phase.height());
  for (int y = 0; y < phase.height(); ++y) {
    for (int x = 0; x < phase.width(); ++x) {
      if (phase.hasValue(x, y))
        wavefront.setValue(x, y, phase.value(x, y) * heightPerOrder);
    }
  }

  measurement.wavefrontMap() = std::move(wavefront);
  return true;
}

}  // namespace digitqt::core::pipeline
