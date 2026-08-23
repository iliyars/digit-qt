#include "RemoveTracedLineCommand.h"

#include "core/FringeOrdering.h"
#include "core/Measurement.h"

#include <QCoreApplication>

namespace digitqt::commands {

RemoveTracedLineCommand::RemoveTracedLineCommand(digitqt::core::Measurement &measurement,
                                                 size_t lineIndex, QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement) {
  setText(QCoreApplication::translate("RemoveTracedLineCommand", "Delete traced fringe"));

  m_before = measurement.fringeTracing().tracedLines();

  m_after = m_before;
  if (lineIndex < m_after.size())
    m_after.erase(m_after.begin() + static_cast<long>(lineIndex));
  digitqt::core::autoAssignFringeOrder(m_after);
}

void RemoveTracedLineCommand::redo() {
  m_measurement.fringeTracing().tracedLines() = m_after;
}

void RemoveTracedLineCommand::undo() {
  m_measurement.fringeTracing().tracedLines() = m_before;
}

}  // namespace digitqt::commands
