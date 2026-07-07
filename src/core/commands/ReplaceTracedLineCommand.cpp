#include "ReplaceTracedLineCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>

namespace digitqt::commands {
ReplaceTracedLineCommand::ReplaceTracedLineCommand(
    digitqt::core::Measurement &measurement, size_t lineIndex,
    digitqt::core::tracing::TracedLine before,
    digitqt::core::tracing::TracedLine after, QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement), m_lineIndex(lineIndex),
      m_before(std::move(before)), m_after(std::move(after)) {
  setText(QCoreApplication::translate("ReplaceTracedLineCommand",
                                      "Edit traced fringe"));
}

void ReplaceTracedLineCommand::redo() {
  auto &lines = m_measurement.fringeTracing().tracedLines();
  if (m_lineIndex >= lines.size())
    return;
  lines[m_lineIndex] = m_after;
}

void ReplaceTracedLineCommand::undo() {
  auto &lines = m_measurement.fringeTracing().tracedLines();
  if (m_lineIndex >= lines.size())
    return;
  lines[m_lineIndex] = m_before;
}
} // namespace digitqt::commands
