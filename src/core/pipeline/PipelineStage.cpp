#include "PipelineStage.h"

#include "core/Measurement.h"

namespace digitqt::core::pipeline {

bool PipelineStage::compute(digitqt::core::Measurement &measurement) {
  std::string error;
  const bool success = doCompute(measurement, error);
  if (success) {
    if (m_id == StageId::S2)
      measurement.recordPhaseParameters();
    m_status = StageStatus::Computed;
    m_errorMessage.clear();
  } else {
    m_status = StageStatus::Error;
    m_errorMessage = error;
  }
  return success;
}

}  // namespace digitqt::core::pipeline
