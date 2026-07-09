#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QUndoCommand>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Removes one seed point by index. undo() re-inserts it at the same index.
class RemoveSeedCommand : public QUndoCommand {
public:
  RemoveSeedCommand(digitqt::core::Measurement &measurement, size_t index,
                    QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  size_t m_index;
  digitqt::core::tracing::SeedPoint m_removed;
  bool m_hasRemoved = false;
};

}  // namespace digitqt::commands
