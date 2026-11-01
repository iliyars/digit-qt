#pragma once

#include "core/pipeline/PipelineStage.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace digitqt::core::pipeline {

/// Canonical order the spec defines the stages in; also the
/// dependency/invalidation order.
inline constexpr std::array<StageId, 7> kCanonicalOrder = {
    StageId::Setup, StageId::S2,  StageId::S4,
    StageId::S4b,   StageId::S5, StageId::S6, StageId::S7,
};

/**
 * @brief Owns one PipelineStage per canonical stage and enforces the
 * "no implicit recomputation" rule from the master specification:
 * changing something upstream marks everything at or after it Stale, but
 * nothing recomputes until the user explicitly asks for it.
 */
class Pipeline {
public:
  Pipeline();

  PipelineStage &stage(StageId id);
  const PipelineStage &stage(StageId id) const;

  /// Call after a stage's inputs/parameters changed (e.g. boundaries
  /// edited for S0a). Marks that stage and everything canonically after
  /// it Stale. Does NOT recompute anything.
  void invalidateFrom(StageId id);

  /// Detect parameter and geometry edits, including edits through mutable references
  /// and undo commands. Call on the GUI thread after edits, never during compute().
  void synchronizeInputs(const digitqt::core::Measurement &measurement);

  /// Invalidates downstream results on every attempt and rejects stale prerequisites.
  bool compute(StageId id, digitqt::core::Measurement &measurement);

  bool hasOutdatedResults() const;

private:
  static size_t indexOf(StageId id);

  std::array<std::unique_ptr<PipelineStage>, kCanonicalOrder.size()> m_stages;
  struct Inputs {
    uint64_t sourceRevision;
    std::array<size_t, 5> fingerprints;  // Setup, lines, S2, S4, S5
  };
  std::optional<Inputs> m_inputs;
};

}  // namespace digitqt::core::pipeline
