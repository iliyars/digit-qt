#pragma once

#include "core/NumberedFringeLine.h"
#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QUndoCommand>
#include <vector>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/**
 * @brief Adds a manually-drawn fringe centerline (see
 * FringeTracingController's AddLineByPoints mode) to Measurement's traced
 * lines, alongside whatever the tracer already produced.
 *
 * Re-runs autoAssignFringeOrder() over the whole set so the new line's
 * left-to-right position among the existing ones is reflected immediately
 * -- that pass can shift other (non-manually-numbered) lines' order too,
 * so this snapshots the full "before" vector at construction and restores
 * it wholesale on undo, rather than just removing the appended line.
 */
class AddTracedLineCommand : public QUndoCommand {
public:
  AddTracedLineCommand(digitqt::core::Measurement &measurement,
                       digitqt::core::tracing::TracedLine points,
                       QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  std::vector<digitqt::core::NumberedFringeLine> m_before;
  std::vector<digitqt::core::NumberedFringeLine> m_after;
};

}  // namespace digitqt::commands
