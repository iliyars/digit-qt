#include "Pipeline.h"

#include "core/Measurement.h"
#include "core/pipeline/NotYetImplementedStage.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"
#include "core/pipeline/stages/PhaseReconstructionStage.h"
#include "core/pipeline/stages/SetupStage.h"
#include "core/pipeline/stages/WavefrontReconstructionStage.h"
#include "core/pipeline/stages/phase_reconstruction/PhasePluginLoading.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace digitqt::core::pipeline {

namespace {
template <typename T>
void appendHash(size_t &hash, const T &value) {
  hash ^= std::hash<T>{}(value) + static_cast<size_t>(0x9e3779b9) + (hash << 6) + (hash >> 2);
}

std::array<size_t, 5> inputFingerprints(const Measurement &m) {
  std::array<size_t, 5> hashes{};
  auto &setup = hashes[0];
  appendHash(setup, m.boundaries().getVersion());
  const auto &tracing = m.fringeTracing();
  appendHash(setup, static_cast<int>(tracing.algorithm()));
  appendHash(setup, static_cast<int>(tracing.fringeCenterMode()));
  appendHash(setup, tracing.seeds().size());
  for (const auto &seed : tracing.seeds()) {
    appendHash(setup, seed.x);
    appendHash(setup, seed.y);
  }
  auto &lines = hashes[1];
  appendHash(lines, tracing.tracedLines().size());
  for (const auto &line : tracing.tracedLines()) {
    appendHash(lines, line.order);
    appendHash(lines, line.points.size());
    for (const auto &p : line.points) {
      appendHash(lines, p.x);
      appendHash(lines, p.y);
    }
  }
  auto &phase = hashes[2];
  appendHash(phase, static_cast<int>(m.phaseReconstructionAlgorithm()));
  appendHash(phase, m.customPhaseReconstructorPluginPath());
  appendHash(phase, m.phaseShifting().nominalStepDeg());
  appendHash(phase, m.phaseShifting().refineShifts());
  appendHash(phase, m.phaseShifting().compensateTilt());
  appendHash(hashes[3], m.wavelengthNm());
  appendHash(hashes[3], m.isDoublePass());
  auto &modal = hashes[4];
  appendHash(modal, static_cast<int>(m.modalFitMethod()));
  appendHash(modal, static_cast<int>(m.polynomialBasis()));
  appendHash(modal, m.edgeErosionPixels());
  const auto &terms = m.modalTermSelection();
  for (bool enabled :
       {terms.tilt, terms.defocus, terms.astigmatism, terms.coma, terms.trefoil, terms.spherical})
    appendHash(modal, enabled);
  // Zernike normalization and isolines only change presentation.
  return hashes;
}
}  // namespace

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

void Pipeline::synchronizeInputs(const Measurement &measurement) {
  const Inputs current{measurement.sourceRevision(), inputFingerprints(measurement)};
  if (m_inputs) {
    if (m_inputs->sourceRevision != current.sourceRevision) {
      for (auto &stage : m_stages)
        stage->reset();
    } else {
      constexpr std::array<StageId, 5> affected = {StageId::Setup, StageId::S2, StageId::S2,
                                                   StageId::S4, StageId::S5};
      for (size_t i = 0; i < affected.size(); ++i)
        if (m_inputs->fingerprints[i] != current.fingerprints[i])
          invalidateFrom(i == 0 && measurement.imageIsPlaceholder() ? StageId::S2 : affected[i]);
      // Edited/imported numbered lines are an explicit new Setup result. They do
      // not have to be overwritten by a tracer before S2 can consume them.
      if (m_inputs->fingerprints[1] != current.fingerprints[1] &&
          !measurement.fringeTracing().tracedLines().empty())
        stage(StageId::Setup).markComputed();
    }
  }
  m_inputs = current;
}

bool Pipeline::compute(StageId id, Measurement &measurement) {
  synchronizeInputs(measurement);
  for (size_t i = indexOf(id) + 1; i < m_stages.size(); ++i)
    m_stages[i]->invalidate();

  auto requireCurrent = [&](StageId prerequisite) {
    const auto status = stage(prerequisite).status();
    if (status != StageStatus::Stale && status != StageStatus::Error)
      return true;
    stage(id).setError("Upstream result " + shortName(prerequisite) +
                       " is out of date or failed. Recompute it first.");
    return false;
  };
  if (id == StageId::S4 && !requireCurrent(StageId::S2))
    return false;
  if (id == StageId::S5 && !requireCurrent(StageId::S4))
    return false;
  bool needsLines = false;
  if (id == StageId::S2) {
    needsLines = measurement.phaseReconstructionAlgorithm() ==
                 PhaseReconstructionAlgorithm::HorizontalSpline;
    if (!measurement.customPhaseReconstructorPluginPath().empty()) {
      const auto plugin =
          probePhaseReconstructorPlugin(measurement.customPhaseReconstructorPluginPath());
      needsLines = !plugin || plugin->needsFringeLines;
    }
    if (needsLines && !requireCurrent(StageId::Setup))
      return false;
  }
  const bool success = stage(id).compute(measurement);
  if (success && id == StageId::S2 && !needsLines)
    stage(StageId::Setup).markComputed();
  // Setup may have replaced the lines, which also invalidates S2.
  synchronizeInputs(measurement);
  return success;
}

bool Pipeline::hasOutdatedResults() const {
  return std::any_of(m_stages.begin(), m_stages.end(), [](const auto &stage) {
    return stage->status() == StageStatus::Stale || stage->status() == StageStatus::Error;
  });
}

}  // namespace digitqt::core::pipeline
