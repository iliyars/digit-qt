#include "AutoExtendFringesCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>
#include <utility>

namespace digitqt::commands {

AutoExtendFringesCommand::AutoExtendFringesCommand(
    digitqt::core::Measurement &measurement,
    std::vector<digitqt::core::NumberedFringeLine> after, FringeExtendDirection direction,
    QUndoCommand *parent)
    : QUndoCommand(parent), m_measurement(measurement), m_after(std::move(after)) {
  m_before = measurement.fringeTracing().tracedLines();

  switch (direction) {
    case FringeExtendDirection::Horizontal:
      setText(QCoreApplication::translate("AutoExtendFringesCommand",
                                          "Extend fringes to aperture edge (width)"));
      break;
    case FringeExtendDirection::Vertical:
      setText(QCoreApplication::translate("AutoExtendFringesCommand",
                                          "Extend fringes to aperture edge (height)"));
      break;
    case FringeExtendDirection::RemoveExtensions:
      setText(QCoreApplication::translate("AutoExtendFringesCommand",
                                          "Remove fringe extensions"));
      break;
  }
}

void AutoExtendFringesCommand::redo() {
  m_measurement.fringeTracing().tracedLines() = m_after;
}

void AutoExtendFringesCommand::undo() {
  m_measurement.fringeTracing().tracedLines() = m_before;
}

}  // namespace digitqt::commands
