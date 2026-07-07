#pragma once

#include <QUndoCommand>

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Adds one seed point for S1 (fringe tracing). undo() removes exactly
/// the seed that was inserted.
class AddSeedCommand : public QUndoCommand {
public:
  AddSeedCommand(digitqt::core::Measurement &measurement,
                 digitqt::core::tracing::SeedPoint seed,
                 QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  digitqt::core::tracing::SeedPoint m_seed;
  size_t m_insertedIndex = 0;
};

} // namespace digitqt::commands
