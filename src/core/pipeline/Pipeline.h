#pragma once

#include "PipelineStage.h"

#include <array>
#include <memory>

namespace digitqt::core::pipeline {

/// Canonical order the spec defines the stages in; also the
/// dependency/invalidation order.
inline constexpr std::array<StageId, 12> kCanonicalOrder = {
    StageId::S0,  StageId::S0a, StageId::S0b, StageId::S0c,
    StageId::S1,  StageId::S2,  StageId::S3,  StageId::S4,
    StageId::S4b, StageId::S5,  StageId::S6,  StageId::S7,
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

private:
  static size_t indexOf(StageId id);

  std::array<std::unique_ptr<PipelineStage>, kCanonicalOrder.size()> m_stages;
};

} // namespace digitqt::core::pipeline
