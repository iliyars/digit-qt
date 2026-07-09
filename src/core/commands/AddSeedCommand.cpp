#include "AddSeedCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>

namespace digitqt::commands {

AddSeedCommand::AddSeedCommand(digitqt::core::Measurement &measurement,
                               digitqt::core::tracing::SeedPoint seed,
                               QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement), m_seed(seed) {
  setText(
      QCoreApplication::translate("AddSeedCommand", "Add trace seed point"));
}

void AddSeedCommand::redo() {
  auto &seeds = m_measurement.fringeTracing().seeds();
  m_insertedIndex = seeds.size();
  seeds.push_back(m_seed);
}

void AddSeedCommand::undo() {
  auto &seeds = m_measurement.fringeTracing().seeds();
  if (m_insertedIndex < seeds.size())
    seeds.erase(seeds.begin() + static_cast<long>(m_insertedIndex));
}
}  // namespace digitqt::commands
