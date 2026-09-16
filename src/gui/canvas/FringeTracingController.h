#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QUndoStack>
#include <optional>
#include <vector>


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
  AddLineByPoints,
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

  void setMode(FringeEditMode mode);
  FringeEditMode mode() const { return m_mode; }

  void handlePress(const QPointF &pos, bool isPrimaryButton);
  void handleMove(const QPointF &pos);
  void handleRelease(const QPointF &pos);
  void deleteSelection();

  /// Scans one row of the image for intensity peaks and adds a seed at
  /// each one found (see core::findRowSeeds), as a single undo step.
  void autoPlaceSeeds();

  /// Synthesizes one new fringe line just beyond the leftmost traced line
  /// and one just beyond the rightmost, continuing each side's step to
  /// its nearest neighbor (see core::extrapolateFringesHorizontally()) --
  /// exactly one line per side per call, not a batch out to the aperture
  /// edge (repeat the call to add more). Single undo step, independent
  /// of extendFringesVertically(). No-op (sets lastError()) if there are
  /// fewer than 2 traced lines or nothing is added.
  void extendFringesHorizontally();

  /// Extends every traced line's two endpoints toward the aperture edge,
  /// following each end's local step (see
  /// core::extrapolateFringesVertically()). Single undo step, independent
  /// of extendFringesHorizontally(). Targets rows near the aperture pole
  /// that S1 tracing doesn't reach (see notes/phase-reconstruction.md).
  /// No-op (sets lastError()) if there are no traced lines or nothing is
  /// added.
  void extendFringesVertically();

  /// Strips every point/line added by extendFringesHorizontally()/
  /// Vertically(), restoring the traced lines to their pre-extension
  /// state (see core::removeFringeExtensions()). Single undo step.
  /// No-op (sets lastError()) if nothing is currently extended.
  void removeFringeExtensions();

  /// How many points past the aperture edge extendFringesVertically()
  /// adds once it crosses it (clamped to >= 1 -- less than one would
  /// defeat the whole point of deliberately overshooting the boundary,
  /// see FringeEdgeExtrapolation.h). Does not affect
  /// extendFringesHorizontally(), which always adds exactly one line per
  /// side per call. Defaults to 10; set from ParametersDock's Setup page.
  void setEdgeExtensionMargin(int margin);
  int edgeExtensionMargin() const { return m_edgeExtensionMargin; }

  /// Double-click: in AddLineByPoints mode, finalizes the line being
  /// collected (see finalizeLineByPoints()). Otherwise enters line-edit
  /// mode for the traced line under the cursor (any line, regardless of
  /// current FringeEditMode), or does nothing if no line is close enough.
  void handleDoubleClick(const QPointF &pos);

  /// Leaves line-edit mode (bound to Escape). No-op if not editing.
  void exitLineEditMode();

  /// Discards any in-progress AddLineByPoints collection (e.g. bound to
  /// the Escape key). No-op if nothing is being collected.
  void cancelPointCollection();

  /// Points collected so far for an in-progress AddLineByPoints line.
  /// Empty outside of that mode.
  const std::vector<QPointF> &lineBufferPreview() const { return m_pointBuffer; }

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

  /// The traced line selected as a whole in plain Select mode (single
  /// click on a line, as opposed to double-click's per-point edit mode).
  /// deleteSelection() removes it entirely when set.
  std::optional<size_t> selectedLineIndex() const { return m_selectedLineIndex; }

  /// True if a seed sits under pos, without changing selection. Used by
  /// ImageCanvas's unified select tool to decide whether a click belongs
  /// to this controller or to the boundary one.
  bool hasSeedAt(const QPointF &pos) const { return hitTestSeed(pos).has_value(); }

  /// True if a traced line sits under pos, without changing selection.
  /// Same purpose as hasSeedAt() for the unified select tool.
  bool hasLineAt(const QPointF &pos) const { return hitTestAnyLine(pos).has_value(); }

  /// Clears the current selection (e.g. because the unified select tool
  /// determined the click belongs to the other controller instead).
  void clearSelection();

  /// Re-emits seedsChanged()/tracedLinesChanged() -- undo/redo mutate
  /// Measurement::fringeTracing() directly via QUndoCommand::undo()/
  /// redo(), bypassing this controller entirely, so the view (which only
  /// listens to these signals) would otherwise go stale after Ctrl+Z/
  /// Ctrl+Shift+Z. See MainWindow's QUndoStack::indexChanged hookup.
  void notifyExternalChange();

  /// The point currently selected within the line under edit (set by
  /// clicking on it -- see handlePress). Deleted by deleteSelection()
  /// while a line is being edited.
  std::optional<size_t> selectedPointIndex() const { return m_selectedPointIndex; }

signals:
  void seedsChanged();
  void tracedLinesChanged();
  void selectionChanged();
  void lineEditModeChanged();
  void previewChanged();  // AddLineByPoints buffer changed -> view should redraw preview

  /// A manually-drawn line was just finalized (see finalizeLineByPoints())
  /// -- distinct from tracedLinesChanged(), which also fires for
  /// in-progress line-edit drags/inserts; listeners that only care about
  /// "a new line was just added by a single-shot tool" (e.g. MainWindow
  /// switching the active tool back to Select) should use this instead.
  void tracedLineAdded();

private:
  std::optional<size_t> hitTestSeed(const QPointF &pos) const;
  std::optional<size_t> hitTestAnyLine(const QPointF &pos) const;
  std::optional<size_t> hitTestPointInEditingLine(const QPointF &pos) const;

  void beginPointDrag(size_t pointIndex, const QPointF &pos);
  void updatePointDrag(const QPointF &pos);
  void commitPointDrag();
  void insertPointOnEditingLine(const QPointF &pos);
  void extendEditingLine(const QPointF &pos, bool atStart);
  void replaceEditingLinePoints(digitqt::core::tracing::TracedLine after);
  void deleteSelectedPoint();
  void finalizeLineByPoints();

  QUndoStack *m_undoStack;
  digitqt::core::Measurement *m_measurement = nullptr;
  digitqt::core::pipeline::Pipeline *m_pipeline = nullptr;
  FringeEditMode m_mode = FringeEditMode::AddSeed;
  std::optional<size_t> m_selection;
  std::optional<size_t> m_selectedLineIndex;
  QString m_lastError;
  int m_edgeExtensionMargin = 10;

  // AddLineByPoints collection state
  std::vector<QPointF> m_pointBuffer;

  // Line-edit mode state
  std::optional<size_t> m_editingLineIndex;
  std::optional<size_t> m_selectedPointIndex;
  bool m_draggingPoint = false;
  size_t m_dragPointIndex = 0;
  digitqt::core::tracing::TracedLine m_dragLineBefore;  // snapshot at drag start
};

}  // namespace digitqt::gui::canvas
