#pragma once

#include <QUndoCommand>
#include <vector>

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Adds several seed points at once (e.g. auto-placement) as a single
/// undo step, instead of one command per point.
class AddSeedsCommand : public QUndoCommand {
public:
  AddSeedsCommand(digitqt::core::Measurement &measurement,
                  std::vector<digitqt::core::tracing::SeedPoint> seeds,
                  QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  std::vector<digitqt::core::tracing::SeedPoint> m_seeds;
  size_t m_startIndex = 0;
};

} // namespace digitqt::commands
