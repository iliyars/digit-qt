#pragma once

#include "core/NumberedFringeLine.h"

#include <QUndoCommand>
#include <vector>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Which edge-extrapolation operation produced `after` -- used only to
/// pick the undo-stack label text.
enum class FringeExtendDirection { Horizontal, Vertical, RemoveExtensions };

/**
 * @brief Replaces Measurement's traced-line set wholesale with the result
 * of core::extrapolateFringesHorizontally()/Vertically()/
 * removeFringeExtensions(), as a single undo step.
 *
 * Same "snapshot the whole vector, swap it on redo/undo" shape as
 * AddTracedLineCommand, generalized because all three edge-extrapolation
 * operations already re-run autoAssignFringeOrder() themselves and hand
 * back a complete replacement vector -- there's nothing operation-specific
 * left for the command to do beyond snapshotting and picking a label.
 */
class AutoExtendFringesCommand : public QUndoCommand {
public:
  AutoExtendFringesCommand(digitqt::core::Measurement &measurement,
                           std::vector<digitqt::core::NumberedFringeLine> after,
                           FringeExtendDirection direction, QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  std::vector<digitqt::core::NumberedFringeLine> m_before;
  std::vector<digitqt::core::NumberedFringeLine> m_after;
};

}  // namespace digitqt::commands
