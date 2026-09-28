#pragma once

#include "core/Fiducial.h"

#include <QUndoCommand>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Удаляет один репер по индексу. undo() вставляет его обратно на то же
/// место, с тем же id (id никогда не переназначаются при undo/redo).
class RemoveFiducialCommand : public QUndoCommand {
public:
  RemoveFiducialCommand(digitqt::core::Measurement &measurement, size_t index,
                        QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  size_t m_index;
  digitqt::core::Fiducial m_removed;
  bool m_hasRemoved = false;
};

}  // namespace digitqt::commands
