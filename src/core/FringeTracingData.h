#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <vector>

namespace digitqt::core {
/**
 * @brief Live, editable data for S1 (fringe tracing).
 *
 * Holds the seed points the user has placed (interactively, via
 * FringeTracingController) and the centerlines produced by the last time
 * the stage was computed. Lives directly on Measurement, the same way
 * boundaries() does -- per "Measurement owns all data".
 */
class FringeTracingData {
public:
  std::vector<tracing::SeedPoint> &seeds() { return m_seeds; }
  const std::vector<tracing::SeedPoint> &seeds() const { return m_seeds; }

  std::vector<tracing::TracedLine> &tracedLines() { return m_tracedLines; }
  const std::vector<tracing::TracedLine> &tracedLines() const {
    return m_tracedLines;
  }

  void clear() {
    m_seeds.clear();
    m_tracedLines.clear();
  }

private:
  std::vector<tracing::SeedPoint> m_seeds;
  std::vector<tracing::TracedLine> m_tracedLines;
};

} // namespace digitqt::core
