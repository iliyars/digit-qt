#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {

/**
 * @brief S2: Phase Reconstruction.
 *
 * Берёт пронумерованные линии полос (Measurement::fringeTracing()
 * ::tracedLines()) и строит плотную карту фазы (Measurement::phaseMap())
 * методом решения уравнения Лапласа (см. PhaseReconstructor). Ради
 * скорости решение считается на уменьшенной сетке (не больше
 * kMaxGridDimension по большей стороне), а не на полном разрешении
 * изображения -- для визуализации (тепловая карта/изолинии) этого более
 * чем достаточно.
 */
class PhaseReconstructionStage : public PipelineStage {
public:
  PhaseReconstructionStage() : PipelineStage(StageId::S2) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) override;
};

}  // namespace digitqt::core::pipeline
