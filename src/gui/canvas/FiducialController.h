#pragma once

#include <QObject>
#include <QPointF>
#include <QUndoStack>
#include <optional>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui::canvas {

enum class FiducialEditMode {
  Select,
  AddFiducial,
};

/**
 * @brief Interprets pointer interaction for реперы: placing, selecting,
 * deleting. Deliberately the simplest of the three canvas controllers --
 * a fiducial is just a point, no drag/move/resize (unlike boundaries) and
 * no line-editing sub-mode (unlike fringe tracing). Mirrors the seed-only
 * subset of FringeTracingController's shape.
 */
class FiducialController : public QObject {
  Q_OBJECT
public:
  explicit FiducialController(QUndoStack *undoStack, QObject *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);

  void setMode(FiducialEditMode mode) { m_mode = mode; }
  FiducialEditMode mode() const { return m_mode; }

  void handlePress(const QPointF &pos, bool isPrimaryButton);
  // No drag/resize in v1 -- present only so ImageCanvas's dispatch can
  // call all three controllers uniformly without special-casing this one.
  void handleMove(const QPointF & /*pos*/) {}
  void handleRelease(const QPointF & /*pos*/) {}

  void deleteSelection();

  std::optional<size_t> selection() const { return m_selection; }

  /// True if a fiducial sits under pos, without changing selection. Used
  /// by ImageCanvas's unified select tool to decide whether a click
  /// belongs to this controller or one of the other two.
  bool hasFiducialAt(const QPointF &pos) const { return hitTest(pos).has_value(); }

  /// Clears the current selection (e.g. because the unified select tool
  /// determined the click belongs to a different controller).
  void clearSelection();

  /// Re-emits fiducialsChanged() -- undo/redo mutate Measurement::
  /// fiducials() directly via QUndoCommand::undo()/redo(), bypassing this
  /// controller, so the view would otherwise go stale after Ctrl+Z. See
  /// MainWindow's QUndoStack::indexChanged hookup.
  void notifyExternalChange() { emit fiducialsChanged(); }

signals:
  void fiducialsChanged();
  void selectionChanged();

private:
  std::optional<size_t> hitTest(const QPointF &pos) const;

  QUndoStack *m_undoStack;
  digitqt::core::Measurement *m_measurement = nullptr;
  FiducialEditMode m_mode = FiducialEditMode::Select;
  std::optional<size_t> m_selection;
};

}  // namespace digitqt::gui::canvas
