#include "SetFringeOrderCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>

namespace digitqt::commands {

SetFringeOrderCommand::SetFringeOrderCommand(digitqt::core::Measurement &measurement,
                                             size_t lineIndex, double newOrder,
                                             QUndoCommand *parent)
    : QUndoCommand(parent),
      m_measurement(measurement),
      m_lineIndex(lineIndex),
      m_newOrder(newOrder) {
  setText(QCoreApplication::translate("SetFringeOrderCommand", "Set fringe order"));
}

void SetFringeOrderCommand::redo() {
  auto &lines = m_measurement.fringeTracing().tracedLines();
  if (m_lineIndex >= lines.size())
    return;
  m_previousOrder = lines[m_lineIndex].order;
  m_previousWasManual = lines[m_lineIndex].orderIsManual;
  lines[m_lineIndex].order = m_newOrder;
  lines[m_lineIndex].orderIsManual = true;
}

void SetFringeOrderCommand::undo() {
  auto &lines = m_measurement.fringeTracing().tracedLines();
  if (m_lineIndex >= lines.size())
    return;
  lines[m_lineIndex].order = m_previousOrder;
  lines[m_lineIndex].orderIsManual = m_previousWasManual;
}

}  // namespace digitqt::commands
