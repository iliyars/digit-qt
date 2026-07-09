#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {

/**
 * @brief Setup: raw image + aperture masking + reference markers +
 * geometric calibration + fringe tracing (spec's S0/S0a/S0b/S0c/S1),
 * combined into one stage.
 *
 * Boundaries and seed points are just live-edited data on Measurement
 * (no compute() involved -- see BoundaryEditController /
 * FringeTracingController). compute() currently runs the fringe tracer
 * over the placed seed points; once markers are implemented, fitting the
 * pixel->physical calibration from them will happen here too.
 */
class SetupStage : public PipelineStage {
public:
  SetupStage() : PipelineStage(StageId::Setup) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement,
                 QString &errorMessage) override;
};

}  // namespace digitqt::core::pipeline
