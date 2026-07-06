#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {
/**
 * @brief S0a: Aperture & Visibility Masking.
 *
 * For now this is a thin placeholder: the boundary shapes it needs already
 * live directly on Measurement (see Measurement::boundaries()) and are
 * edited interactively via BoundaryEditController. "Computing" this stage
 * will eventually mean building a VisibilityMask from that ShapeCollection
 * (ApertureCore already has VisibilityMaskBuilder for this) -- deferred
 * until S1 (fringe tracing) actually needs to consume that mask.
 */
class ApertureMaskingStage : public PipelineStage {
public:
  ApertureMaskingStage() : PipelineStage(StageId::S0a) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement,
                 QString &errorMessage) override;
};

} // namespace digitqt::core::pipeline
