#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QUndoStack>
#include <optional>


namespace digitqt::core {
class Measurement;
}
namespace digitqt::core::pipeline {
class Pipeline;
}

namespace digitqt::gui::canvas {

enum class FringeEditMode {
  Select,
  AddSeed,
};

/**
 * @brief Interprets pointer interaction for S1 (fringe tracing): placing
 * seed points, selecting/deleting them, running the tracer, and editing
 * an already-traced line (move/add points).
 *
 * Mirrors BoundaryEditController's shape: decisions live here; ImageCanvas
 * only forwards events and renders whatever Measurement::fringeTracing()
 * contains.
 *
 * Line editing is a distinct sub-mode, entered by double-clicking a
 * traced line (regardless of the current FringeEditMode -- it targets the
 * line under the cursor, not "the current tool") and exited with Escape:
 *   - click near an existing point on the line under edit -> drag to move it
 *   - click elsewhere on the line -> inserts a new point there
 *   - Escape -> leaves edit mode
 */
class FringeTracingController : public QObject {
  Q_OBJECT
public:
  explicit FringeTracingController(QUndoStack *undoStack, QObject *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);
  void setPipeline(digitqt::core::pipeline::Pipeline *pipeline);

  void setMode(FringeEditMode mode) { m_mode = mode; }
  FringeEditMode mode() const { return m_mode; }

  void handlePress(const QPointF &pos, bool isPrimaryButton);
  void handleMove(const QPointF &pos);
  void handleRelease(const QPointF &pos);
  void deleteSelection();

  /// Scans one row of the image for intensity peaks and adds a seed at
  /// each one found (see core::findRowSeeds), as a single undo step.
  void autoPlaceSeeds();

  /// Double-click: enters line-edit mode for the traced line under the
  /// cursor (any line, regardless of current FringeEditMode), or does
  /// nothing if no line is close enough.
  void handleDoubleClick(const QPointF &pos);

  /// Leaves line-edit mode (bound to Escape). No-op if not editing.
  void exitLineEditMode();

  /// Manually sets the fringe order for the line under edit (or any
  /// line by index). Marks it as manually numbered so a subsequent
  /// auto-numbering pass won't overwrite it.
  void setLineOrder(size_t lineIndex, double newOrder);

  /// Runs the Setup stage's compute() (fringe tracing) against the
  /// current seeds. Returns true on success; on failure, see lastError().
  bool runTracing();
  const QString &lastError() const { return m_lastError; }

  std::optional<size_t> selection() const { return m_selection; }
  std::optional<size_t> editingLineIndex() const { return m_editingLineIndex; }

  /// The point currently selected within the line under edit (set by
  /// clicking on it -- see handlePress). Deleted by deleteSelection()
  /// while a line is being edited.
  std::optional<size_t> selectedPointIndex() const { return m_selectedPointIndex; }

signals:
  void seedsChanged();
  void tracedLinesChanged();
  void selectionChanged();
  void lineEditModeChanged();

private:
  std::optional<size_t> hitTestSeed(const QPointF &pos) const;
  std::optional<size_t> hitTestAnyLine(const QPointF &pos) const;
  std::optional<size_t> hitTestPointInEditingLine(const QPointF &pos) const;

  void beginPointDrag(size_t pointIndex, const QPointF &pos);
  void updatePointDrag(const QPointF &pos);
  void commitPointDrag();
  void insertPointOnEditingLine(const QPointF &pos);
  void deleteSelectedPoint();

  QUndoStack *m_undoStack;
  digitqt::core::Measurement *m_measurement = nullptr;
  digitqt::core::pipeline::Pipeline *m_pipeline = nullptr;
  FringeEditMode m_mode = FringeEditMode::AddSeed;
  std::optional<size_t> m_selection;
  QString m_lastError;

  // Line-edit mode state
  std::optional<size_t> m_editingLineIndex;
  std::optional<size_t> m_selectedPointIndex;
  bool m_draggingPoint = false;
  size_t m_dragPointIndex = 0;
  digitqt::core::tracing::TracedLine m_dragLineBefore;  // snapshot at drag start
};

}  // namespace digitqt::gui::canvas
