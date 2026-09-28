#include "RemoveFiducialCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>
#include <algorithm>

namespace digitqt::commands {

RemoveFiducialCommand::RemoveFiducialCommand(digitqt::core::Measurement &measurement, size_t index,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement), m_index(index) {
  setText(QCoreApplication::translate("RemoveFiducialCommand", "Remove fiducial"));
}

void RemoveFiducialCommand::redo() {
  auto &fiducials = m_measurement.fiducials().fiducials();
  if (m_index >= fiducials.size())
    return;
  m_removed = fiducials[m_index];
  m_hasRemoved = true;
  fiducials.erase(fiducials.begin() + static_cast<long>(m_index));
}

void RemoveFiducialCommand::undo() {
  if (!m_hasRemoved)
    return;
  auto &fiducials = m_measurement.fiducials().fiducials();
  const size_t insertAt = std::min(m_index, fiducials.size());
  fiducials.insert(fiducials.begin() + static_cast<long>(insertAt), m_removed);
}

}  // namespace digitqt::commands
