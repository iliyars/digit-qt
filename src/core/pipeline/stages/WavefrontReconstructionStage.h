#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {
/**
 * @brief S4: Wavefront Reconstruction.
 *
 * Переводит карту фазы (Measurement::phaseMap(), в единицах номера
 * полосы) в физическую высоту волнового фронта (Measurement::
 * wavefrontMap(), в нанометрах), по формуле:
 *
 *   высота = номер_полосы × λ / 2
 *
 * Коэффициент 2 — потому что при отражении свет проходит неровность
 * поверхности дважды (туда и обратно). λ берётся из Measurement::
 * wavelengthNm().
 */
class WavefrontReconstructionStage : public PipelineStage {
public:
  WavefrontReconstructionStage() : PipelineStage(StageId::S4) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement, QString &errorMessage);
};

}  // namespace digitqt::core::pipeline
