#include "AddFiducialCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>

namespace digitqt::commands {

AddFiducialCommand::AddFiducialCommand(digitqt::core::Measurement &measurement, double imageX,
                                       double imageY, QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement) {
  // Выделяем id сразу здесь, а не в redo() -- если команду создали, но
  // так и не запушили в стек (отменённое взаимодействие), id просто
  // пропускается, что нормально: последовательность id не обязана быть
  // сплошной, важна только уникальность и стабильность.
  m_fiducial.id = measurement.fiducials().allocateId();
  m_fiducial.imageX = imageX;
  m_fiducial.imageY = imageY;
  setText(QCoreApplication::translate("AddFiducialCommand", "Add fiducial"));
}

void AddFiducialCommand::redo() {
  auto &fiducials = m_measurement.fiducials().fiducials();
  m_insertedIndex = fiducials.size();
  fiducials.push_back(m_fiducial);
}

void AddFiducialCommand::undo() {
  auto &fiducials = m_measurement.fiducials().fiducials();
  if (m_insertedIndex < fiducials.size())
    fiducials.erase(fiducials.begin() + static_cast<long>(m_insertedIndex));
}

}  // namespace digitqt::commands
