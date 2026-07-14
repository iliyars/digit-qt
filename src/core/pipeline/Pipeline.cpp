#include "Pipeline.h"

#include "core/pipeline/NotYetImplementedStage.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"
#include "core/pipeline/stages/PhaseReconstructionStage.h"
#include "core/pipeline/stages/SetupStage.h"
#include "core/pipeline/stages/WavefrontReconstructionStage.h"

#include <algorithm>
#include <stdexcept>

namespace digitqt::core::pipeline {

size_t Pipeline::indexOf(StageId id) {
  const auto it = std::find(kCanonicalOrder.begin(), kCanonicalOrder.end(), id);
  if (it == kCanonicalOrder.end())
    throw std::logic_error("Pipeline::indexOf: unknown StageId");
  return static_cast<size_t>(std::distance(kCanonicalOrder.begin(), it));
}

Pipeline::Pipeline() {
  for (size_t i = 0; i < kCanonicalOrder.size(); ++i) {
    const StageId id = kCanonicalOrder[i];
    if (id == StageId::Setup)
      m_stages[i] = std::make_unique<SetupStage>();
    else if (id == StageId::S2)
      m_stages[i] = std::make_unique<PhaseReconstructionStage>();
    else if (id == StageId::S4)
      m_stages[i] = std::make_unique<WavefrontReconstructionStage>();
    else if (id == StageId::S5)
      m_stages[i] = std::make_unique<ModalAnalysisStage>();
    else
      m_stages[i] = std::make_unique<NotYetImplementedStage>(id);
  }
}

PipelineStage &Pipeline::stage(StageId id) {
  return *m_stages[indexOf(id)];
}
const PipelineStage &Pipeline::stage(StageId id) const {
  return *m_stages[indexOf(id)];
}

void Pipeline::invalidateFrom(StageId id) {
  for (size_t i = indexOf(id); i < m_stages.size(); ++i)
    m_stages[i]->invalidate();
}

}  // namespace digitqt::core::pipeline
