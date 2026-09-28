#pragma once

#include "core/Fiducial.h"

#include <QUndoCommand>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Добавляет один репер по позиции в СКИ. id выделяется один раз, в
/// конструкторе -- redo()/undo() просто вставляют/убирают один и тот же
/// зафиксированный Fiducial, так что id не меняется между отменой и
/// повтором действия (не выделяется заново на повторном redo()).
class AddFiducialCommand : public QUndoCommand {
public:
  AddFiducialCommand(digitqt::core::Measurement &measurement, double imageX, double imageY,
                     QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

  int fiducialId() const { return m_fiducial.id; }

private:
  digitqt::core::Measurement &m_measurement;
  digitqt::core::Fiducial m_fiducial;
  size_t m_insertedIndex = 0;
};
}  // namespace digitqt::commands
