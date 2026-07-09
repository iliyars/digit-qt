#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QUndoStack>
#include <aperture/include/geometry/Shape.h>
#include <aperture/include/visibility/TypeLimits.h>
#include <optional>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui::canvas {

enum class EditMode {
  Select,
  AddExternalEllipse,
  AddExternalRectangle,
  AddInternalEllipse,
  AddInternalRectangle,
  AddExternalEllipseByPoints,
  AddInternalEllipseByPoints,
};

/**
 * @brief Interprets pointer interaction on the image canvas and turns it
 * into undoable mutations of Measurement::boundaries().
 *
 * Knows nothing about QGraphicsView/QGraphicsScene -- ImageCanvas is a thin
 * Qt adapter around it. This mirrors the "Views never compute" rule from
 * the master specification: all decisions (what shape to create,
 * hit-testing, commands) live here; ImageCanvas only forwards events and
 * renders whatever boundaries() contains.
 */
class BoundaryEditController : public QObject {
  Q_OBJECT
public:
  explicit BoundaryEditController(QUndoStack *undoStack,
                                  QObject *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);
  digitqt::core::Measurement *measurement() const { return m_measurement; }

  void setMode(EditMode mode);
  EditMode mode() const { return m_mode; }

  // Pointer events, in scene/world coordinates (i.e. image pixel coordinates).
  void handlePress(const QPointF &pos, bool isPrimaryButton);
  void handleMove(const QPointF &pos);
  void handleRelease(const QPointF &pos);
  void deleteSelection();

  /// Double-click finalizes an in-progress ellipse-by-points (see
  /// AddExternalEllipseByPoints / AddInternalEllipseByPoints). No-op in
  /// any other mode.
  void handleDoubleClick(const QPointF &pos);

  /// Discards any in-progress ellipse-by-points collection (e.g. bound
  /// to the Escape key). No-op if nothing is being collected.
  void cancelPointCollection();

  // Live drag-to-create rectangle preview (Add* modes); empty if not dragging.
  std::optional<QRectF> creationPreview() const;

  /// Points collected so far for an in-progress ellipse-by-points.
  /// Empty outside of AddExternalEllipseByPoints/AddInternalEllipseByPoints.
  const std::vector<QPointF> &pointBufferPreview() const {
    return m_pointBuffer;
  }

  struct Selection {
    aperture::TypeLimits type;
    size_t index;

    friend bool operator==(const Selection &a, const Selection &b) {
      return a.type == b.type && a.index == b.index;
    }
  };
  std::optional<Selection> selection() const { return m_selection; }

signals:
  void boundariesChanged();  // ShapeCollection content changed -> view should
                             // re-render
  void
  previewChanged();  // drag preview changed -> view should redraw rubber band
  void selectionChanged();

private:
  bool isAddMode() const;
  bool isPointsMode() const;
  aperture::TypeLimits addModeType() const;
  bool addModeIsEllipse() const;

  std::unique_ptr<aperture::Shape> buildShapeFromRect(const QRectF &rect) const;
  void finalizePointsEllipse();
  std::optional<Selection> hitTest(const QPointF &pos) const;

  void beginMoveDrag(const Selection &sel, const QPointF &pos);
  void updateMoveDrag(const QPointF &pos);
  void commitMoveDrag();

  QUndoStack *m_undoStack;
  digitqt::core::Measurement *m_measurement = nullptr;
  EditMode m_mode = EditMode::Select;

  // Add-mode drag state
  bool m_creating = false;
  QPointF m_createAnchor;
  QPointF m_createCurrent;

  // Ellipse-by-points collection state
  std::vector<QPointF> m_pointBuffer;

  // Select-mode drag (move) state
  bool m_moving = false;
  QPointF m_moveAnchor;
  std::unique_ptr<aperture::Shape> m_moveOriginal;  // clone taken at drag start

  std::optional<Selection> m_selection;
};

}  // namespace digitqt::gui::canvas
