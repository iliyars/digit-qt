#include "AddSeedsCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>

namespace digitqt::commands {

AddSeedsCommand::AddSeedsCommand(
    digitqt::core::Measurement &measurement,
    std::vector<digitqt::core::tracing::SeedPoint> seeds, QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement),
      m_seeds(std::move(seeds)) {
  setText(
      QCoreApplication::translate("AddSeedsCommand", "Auto-place seed points"));
}

void AddSeedsCommand::redo() {
  auto &seeds = m_measurement.fringeTracing().seeds();
  m_startIndex = seeds.size();
  seeds.insert(seeds.end(), m_seeds.begin(), m_seeds.end());
}

void AddSeedsCommand::undo() {
  auto &seeds = m_measurement.fringeTracing().seeds();
  const size_t count = m_seeds.size();
  if (m_startIndex + count > seeds.size())
    return;
  seeds.erase(seeds.begin() + static_cast<long>(m_startIndex),
              seeds.begin() + static_cast<long>(m_startIndex + count));
}

} // namespace digitqt::commands
