#pragma once

#include <QString>

#include "PipelineStageId.h"

namespace digitqt::core {
class Measurement;
}

namespace digitqt::core::pipeline {

enum class StageStatus {
  NotComputed, // never run
  Stale,       // was computed, but an upstream stage changed since
  Computed,    // up to date
  Error,       // last compute() attempt failed
};

/**
 * @brief One node of the canonical processing pipeline (S0..S7).
 *
 * A stage owns no data itself -- results live on Measurement (per the
 * "Measurement owns all data" rule). A stage only knows:
 *   - its identity (id())
 *   - whether it currently needs (re)computing (status())
 *   - how to compute itself from Measurement's current state (compute())
 *
 * Per the master spec's History/Undo rules, compute() is only ever called
 * explicitly (e.g. a "Compute" button in the parameters panel) -- never
 * implicitly as a side effect of a parameter edit or an upstream change.
 * An upstream change instead just marks this stage Stale (see
 * Pipeline::invalidateFrom), so the UI can show it needs re-running.
 */
class PipelineStage {
public:
  explicit PipelineStage(StageId id) : m_id(id) {}
  virtual ~PipelineStage() = default;

  StageId id() const { return m_id; }
  StageStatus status() const { return m_status; }

  /// Marks this single stage as needing to be recomputed. Pipeline is
  /// responsible for propagating this to downstream stages.
  void invalidate() {
    if (m_status == StageStatus::Computed)
      m_status = StageStatus::Stale;
  }

  /**
   * @brief Runs the stage's algorithm against the given Measurement,
   * using whatever parameters have been set on this stage.
   * @return true and sets status() to Computed on success; false and
   * sets status() to Error (see errorMessage()) on failure.
   */
  bool compute(digitqt::core::Measurement &measurement);

  const QString &errorMessage() const { return m_errorMessage; }

protected:
  // Subclasses implement the actual algorithm. Returning false sets
  // status() to Error automatically.
  virtual bool doCompute(digitqt::core::Measurement &measurement,
                         QString &errorMessage) = 0;

private:
  StageId m_id;
  StageStatus m_status = StageStatus::NotComputed;
  QString m_errorMessage;
};

} // namespace digitqt::core::pipeline
