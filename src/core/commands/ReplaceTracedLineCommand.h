#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QUndoCommand>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/**
 * @brief Replaces one traced fringe line (by index) with an edited copy.
 *
 * Used for both moving a point (drag a vertex handle) and inserting a new
 * point (click on the line between two existing points) -- both are just
 * "the line's point vector changed", so one command covers either.
 */
class ReplaceTracedLineCommand : public QUndoCommand {
public:
  ReplaceTracedLineCommand(digitqt::core::Measurement &measurement,
                           size_t lineIndex,
                           digitqt::core::tracing::TracedLine before,
                           digitqt::core::tracing::TracedLine after,
                           QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  size_t m_lineIndex;
  digitqt::core::tracing::TracedLine m_before;
  digitqt::core::tracing::TracedLine m_after;
};
}  // namespace digitqt::commands
