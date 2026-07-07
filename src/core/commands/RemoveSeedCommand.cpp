#include "RemoveSeedCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>
#include <algorithm>

namespace digitqt::commands {

RemoveSeedCommand::RemoveSeedCommand(digitqt::core::Measurement &measurement,
                                     size_t index, QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement), m_index(index) {
  setText(QCoreApplication::translate("RemoveSeedCommand",
                                      "Remove trace seed point"));
}

void RemoveSeedCommand::redo() {
  auto &seeds = m_measurement.fringeTracing().seeds();
  if (m_index >= seeds.size())
    return;
  m_removed = seeds[m_index];
  m_hasRemoved = true;
  seeds.erase(seeds.begin() + static_cast<long>(m_index));
}

void RemoveSeedCommand::undo() {
  if (!m_hasRemoved)
    return;
  auto &seeds = m_measurement.fringeTracing().seeds();
  const size_t insertAt = std::min(m_index, seeds.size());
  seeds.insert(seeds.begin() + static_cast<long>(insertAt), m_removed);
}

} // namespace digitqt::commands
