#include "BoundaryEditController.h"

#include "core/Measurement.h"
#include "core/ShapeCollectionAccess.h"
#include "core/commands/AddShapeCommand.h"
#include "core/commands/RemoveShapeCommand.h"
#include "core/commands/ReplaceShapeCommand.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/geometry/Rectangle.h>

#include <algorithm>

namespace digitqt::gui::canvas {

using digitqt::commands::AddShapeCommand;
using digitqt::commands::RemoveShapeCommand;
using digitqt::commands::ReplaceShapeCommand;

BoundaryEditController::BoundaryEditController(QUndoStack *undoStack,
                                               QObject *parent)
    : QObject(parent), m_undoStack(undoStack) {}

void BoundaryEditController::setMeasurement(
    digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  m_selection.reset();
  m_creating = false;
  m_moving = false;
  emit boundariesChanged();
  emit selectionChanged();
}

void BoundaryEditController::setMode(EditMode mode) {
  if (m_mode == mode)
    return;
  m_creating = false;
  m_moving = false;
  m_mode = mode;
  emit previewChanged();
}

bool BoundaryEditController::isAddMode() const {
  return m_mode != EditMode::Select;
}

aperture::TypeLimits BoundaryEditController::addModeType() const {
  switch (m_mode) {
  case EditMode::AddInternalEllipse:
  case EditMode::AddInternalRectangle:
    return aperture::TypeLimits::INTERNAL;
  default:
    return aperture::TypeLimits::EXTERNAL;
  }
}

bool BoundaryEditController::addModeIsEllipse() const {
  return m_mode == EditMode::AddExternalEllipse ||
         m_mode == EditMode::AddInternalEllipse;
}

std::unique_ptr<aperture::Shape>
BoundaryEditController::buildShapeFromRect(const QRectF &rect) const {
  const double cx = rect.center().x();
  const double cy = rect.center().y();
  const double halfW = std::max(rect.width() / 2.0, 1.0);
  const double halfH = std::max(rect.height() / 2.0, 1.0);

  if (addModeIsEllipse())
    return std::make_unique<aperture::Ellipse>(halfW, halfH, cx, cy);
  return std::make_unique<aperture::Rectangle>(rect.width(), rect.height(), cx,
                                               cy);
}

void BoundaryEditController::handlePress(const QPointF &pos,
                                         bool isPrimaryButton) {
  if (!m_measurement || !isPrimaryButton)
    return;

  if (isAddMode()) {
    m_creating = true;
    m_createAnchor = pos;
    m_createCurrent = pos;
    emit previewChanged();
    return;
  }

  auto hit = hitTest(pos);
  if (!(hit == m_selection)) {
    m_selection = hit;
    emit selectionChanged();
  }
  if (hit)
    beginMoveDrag(*hit, pos);
}

void BoundaryEditController::handleMove(const QPointF &pos) {
  if (m_creating) {
    m_createCurrent = pos;
    emit previewChanged();
  } else if (m_moving) {
    updateMoveDrag(pos);
  }
}

void BoundaryEditController::handleRelease(const QPointF &pos) {
  if (m_creating) {
    m_creating = false;
    const QRectF rect = QRectF(m_createAnchor, pos).normalized();
    emit previewChanged();
    if (rect.width() < 2.0 || rect.height() < 2.0)
      return; // ignore accidental clicks
    auto shape = buildShapeFromRect(rect);
    m_undoStack->push(new AddShapeCommand(m_measurement->boundaries(),
                                          addModeType(), std::move(shape)));
    emit boundariesChanged();
  } else if (m_moving) {
    commitMoveDrag();
  }
}

void BoundaryEditController::deleteSelection() {
  if (!m_measurement || !m_selection)
    return;
  m_undoStack->push(new RemoveShapeCommand(
      m_measurement->boundaries(), m_selection->type, m_selection->index));
  m_selection.reset();
  emit boundariesChanged();
  emit selectionChanged();
}

std::optional<QRectF> BoundaryEditController::creationPreview() const {
  if (!m_creating)
    return std::nullopt;
  return QRectF(m_createAnchor, m_createCurrent).normalized();
}

std::optional<BoundaryEditController::Selection>
BoundaryEditController::hitTest(const QPointF &pos) const {
  if (!m_measurement)
    return std::nullopt;
  const aperture::Point p{pos.x(), pos.y()};
  const auto &boundaries = m_measurement->boundaries();

  // Internal boundaries are searched first: they are typically drawn on
  // top of, and nested inside, external ones.
  const auto &internalShapes = boundaries.getInternal();
  for (size_t i = internalShapes.size(); i-- > 0;) {
    if (internalShapes[i]->isInside(p))
      return Selection{aperture::TypeLimits::INTERNAL, i};
  }
  const auto &externalShapes = boundaries.getExternal();
  for (size_t i = externalShapes.size(); i-- > 0;) {
    if (externalShapes[i]->isInside(p))
      return Selection{aperture::TypeLimits::EXTERNAL, i};
  }
  return std::nullopt;
}

void BoundaryEditController::beginMoveDrag(const Selection &sel,
                                           const QPointF &pos) {
  const auto &container = (sel.type == aperture::TypeLimits::EXTERNAL)
                              ? m_measurement->boundaries().getExternal()
                              : m_measurement->boundaries().getInternal();
  if (sel.index >= container.size())
    return;

  m_moving = true;
  m_moveAnchor = pos;
  m_moveOriginal = container[sel.index]->clone();
}

void BoundaryEditController::updateMoveDrag(const QPointF &pos) {
  if (!m_selection || !m_moveOriginal)
    return;
  auto &container = digitqt::core::mutableContainer(m_measurement->boundaries(),
                                                    m_selection->type);
  if (m_selection->index >= container.size())
    return;

  const double dx = pos.x() - m_moveAnchor.x();
  const double dy = pos.y() - m_moveAnchor.y();

  auto preview = m_moveOriginal->clone();
  preview->shiftX(dx);
  preview->shiftY(dy);
  container[m_selection->index] = std::move(preview);
  m_measurement->boundaries().notifyShapeModified();
  emit boundariesChanged();
}

void BoundaryEditController::commitMoveDrag() {
  if (!m_selection || !m_moveOriginal) {
    m_moving = false;
    return;
  }
  auto &container = digitqt::core::mutableContainer(m_measurement->boundaries(),
                                                    m_selection->type);
  if (m_selection->index < container.size()) {
    auto after = container[m_selection->index]->clone();
    // Roll the live preview mutation back first; ReplaceShapeCommand's
    // redo() will (re)apply it, so the whole drag lands as one undo step.
    container[m_selection->index] = m_moveOriginal->clone();
    m_undoStack->push(new ReplaceShapeCommand(
        m_measurement->boundaries(), m_selection->type, m_selection->index,
        std::move(m_moveOriginal), std::move(after)));
  }
  m_moving = false;
  emit boundariesChanged();
}

} // namespace digitqt::gui::canvas
