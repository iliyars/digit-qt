#include "AddTracedLineCommand.h"

#include "core/FringeOrdering.h"
#include "core/Measurement.h"

#include <utility>

namespace digitqt::commands {

AddTracedLineCommand::AddTracedLineCommand(digitqt::core::Measurement &measurement,
                                           digitqt::core::tracing::TracedLine points,
                                           QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement) {
  m_before = measurement.fringeTracing().tracedLines();

  m_after = m_before;
  digitqt::core::NumberedFringeLine line;
  line.points = std::move(points);
  m_after.push_back(std::move(line));
  digitqt::core::autoAssignFringeOrder(m_after);
}

void AddTracedLineCommand::redo() {
  m_measurement.fringeTracing().tracedLines() = m_after;
}

void AddTracedLineCommand::undo() {
  m_measurement.fringeTracing().tracedLines() = m_before;
}

}  // namespace digitqt::commands
