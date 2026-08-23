#pragma once

#include "core/NumberedFringeLine.h"

#include <QUndoCommand>
#include <vector>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/**
 * @brief Removes one traced fringe line (by index) entirely.
 *
 * Like AddTracedLineCommand, snapshots the full "before" vector at
 * construction and re-runs autoAssignFringeOrder() on the "after" vector,
 * so the remaining lines' order values stay consistent once one is
 * removed from the middle; undo restores the exact original vector.
 */
class RemoveTracedLineCommand : public QUndoCommand {
public:
  RemoveTracedLineCommand(digitqt::core::Measurement &measurement, size_t lineIndex,
                          QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  std::vector<digitqt::core::NumberedFringeLine> m_before;
  std::vector<digitqt::core::NumberedFringeLine> m_after;
};

}  // namespace digitqt::commands
