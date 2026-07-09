#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {

/**
 * @brief Placeholder for canonical stages that don't have a real
 * implementation yet (currently: S0, S0b, S0c, S1..S7).
 *
 * compute() always fails with a clear message, so the pipeline tree/UI has
 * something real to show and report before each stage is actually built --
 * rather than the tree silently having "missing" nodes.
 */
class NotYetImplementedStage : public PipelineStage {
public:
  explicit NotYetImplementedStage(StageId id) : PipelineStage(id) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement,
                 QString &errorMessage) override;
};
}  // namespace digitqt::core::pipeline
