#pragma once

#include "core/NumberedFringeLine.h"
#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <vector>

namespace digitqt::core {

/// Which IFringeTracer implementation the Setup stage's compute() should
/// use. A user-facing choice (see ParametersDock's algorithm combo box),
/// not itself undoable -- like the current edit-mode/tool selection.
enum class TracerAlgorithm {
  SequentialTracking,  // SequentialFringeTracker: classic STEP.C-derived step tracer (FTM)
  StructureTensor,     // StructureTensorTracker: gradient structure-tensor ridge tracker (FTM)
  ScanlineExtremum,    // ScanlineExtremumTracker: global, row-by-row extrema (FTM)
  BinaryThinning,  // BinaryThinningTracker: adaptive threshold + Zhang-Suen skeletonization (FBM)
};

/// Which kind of fringe center ScanlineExtremumTracker should look for.
/// Not used by SequentialFringeTracker (it follows whichever ridge the
/// seed point lands on).
enum class FringeCenterMode {
  Max,     // bright fringe centers only
  Min,     // dark fringe centers only
  MinMax,  // both, with Red/Black alternation enforced between neighbors
};

/**
 * @brief Live, editable data for S1 (fringe tracing).
 *
 * Holds the seed points the user has placed (interactively, via
 * FringeTracingController) and the numbered centerlines produced by the
 * last time the stage was computed (see NumberedFringeLine). Lives
 * directly on Measurement, the same way boundaries() does -- per
 * "Measurement owns all data".
 */
class FringeTracingData {
public:
  std::vector<tracing::SeedPoint> &seeds() { return m_seeds; }
  const std::vector<tracing::SeedPoint> &seeds() const { return m_seeds; }

  std::vector<NumberedFringeLine> &tracedLines() { return m_tracedLines; }
  const std::vector<NumberedFringeLine> &tracedLines() const { return m_tracedLines; }

  TracerAlgorithm algorithm() const { return m_algorithm; }
  void setAlgorithm(TracerAlgorithm algorithm) { m_algorithm = algorithm; }

  FringeCenterMode fringeCenterMode() const { return m_fringeCenterMode; }
  void setFringeCenterMode(FringeCenterMode mode) { m_fringeCenterMode = mode; }

  void clear() {
    m_seeds.clear();
    m_tracedLines.clear();
  }

private:
  std::vector<tracing::SeedPoint> m_seeds;
  std::vector<NumberedFringeLine> m_tracedLines;
  TracerAlgorithm m_algorithm = TracerAlgorithm::SequentialTracking;
  FringeCenterMode m_fringeCenterMode = FringeCenterMode::MinMax;
};

}  // namespace digitqt::core
