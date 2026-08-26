#include "BoundaryEditController.h"

#include "core/Measurement.h"
#include "core/ShapeCollectionAccess.h"
#include "core/commands/AddShapeCommand.h"
#include "core/commands/RemoveShapeCommand.h"
#include "core/commands/ReplaceShapeCommand.h"

#include <algorithm>
#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/geometry/Rectangle.h>
#include <cmath>
#include <limits>

namespace digitqt::gui::canvas {

using digitqt::commands::AddShapeCommand;
using digitqt::commands::RemoveShapeCommand;
using digitqt::commands::ReplaceShapeCommand;

namespace {

// Boundaries are drawn as outlines only (no fill -- see BoundaryItem's
// Qt::NoBrush), so hit-testing against the filled interior (shape::isInside)
// would treat almost the entire image as "on" a large aperture. Testing
// distance to the actual contour instead matches what's visually there,
// and keeps a big aperture from swallowing clicks meant for a seed point
// that happens to sit inside it.
constexpr double kHitTolerance = 10.0;

double distanceToContour(const aperture::Shape &shape, const QPointF &pos) {
  const auto contour = shape.getContour(3.0);
  if (contour.empty())
    return std::numeric_limits<double>::infinity();

  double best = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < contour.size(); ++i) {
    const QPointF a(contour[i].x, contour[i].y);
    const QPointF b(contour[(i + 1) % contour.size()].x, contour[(i + 1) % contour.size()].y);
    const QPointF ab = b - a;
    const double lenSq = QPointF::dotProduct(ab, ab);
    double t = 0.0;
    if (lenSq > 1e-9)
      t = std::clamp(QPointF::dotProduct(pos - a, ab) / lenSq, 0.0, 1.0);
    const QPointF proj = a + ab * t;
    const QPointF d = pos - proj;
    best = std::min(best, std::sqrt(QPointF::dotProduct(d, d)));
  }
  return best;
}

}  // namespace

BoundaryEditController::BoundaryEditController(QUndoStack *undoStack,
                                               QObject *parent)
    : QObject(parent), m_undoStack(undoStack) {}

void BoundaryEditController::setMeasurement(
    digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  m_selection.reset();
  m_creating = false;
  m_moving = false;
  m_resizingHandle = false;
  emit boundariesChanged();
  emit selectionChanged();
}

void BoundaryEditController::setMode(EditMode mode) {
  if (m_mode == mode)
    return;
  m_creating = false;
  m_moving = false;
  m_resizingHandle = false;
  m_pointBuffer.clear();
  m_mode = mode;
  emit previewChanged();
}

bool BoundaryEditController::isAddMode() const {
  return m_mode != EditMode::Select;
}

bool BoundaryEditController::isPointsMode() const {
  return m_mode == EditMode::AddExternalEllipseByPoints ||
         m_mode == EditMode::AddInternalEllipseByPoints;
}

aperture::TypeLimits BoundaryEditController::addModeType() const {
  switch (m_mode) {
    case EditMode::AddInternalEllipse:
    case EditMode::AddInternalRectangle:
    case EditMode::AddInternalEllipseByPoints:
      return aperture::TypeLimits::INTERNAL;
    default:
      return aperture::TypeLimits::EXTERNAL;
  }
}

bool BoundaryEditController::addModeIsEllipse() const {
  return m_mode == EditMode::AddExternalEllipse ||
         m_mode == EditMode::AddInternalEllipse;
}

std::unique_ptr<aperture::Shape> BoundaryEditController::buildShapeFromRect(
    const QRectF &rect) const {
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

  if (isPointsMode()) {
    m_pointBuffer.push_back(pos);
    emit previewChanged();
    return;
  }

  if (isAddMode()) {
    m_creating = true;
    m_createAnchor = pos;
    m_createCurrent = pos;
    emit previewChanged();
    return;
  }

  if (m_selection) {
    if (auto handleHit = hitTestSelectedHandle(pos)) {
      beginHandleDrag(*handleHit, pos);
      return;
    }
  }

  auto hit = hitTest(pos);
  if (!(hit == m_selection)) {
    m_selection = hit;
    emit selectionChanged();
  }
  if (hit)
    beginMoveDrag(*hit, pos);
}

void BoundaryEditController::clearSelection() {
  if (!m_selection)
    return;
  m_selection.reset();
  emit selectionChanged();
}

void BoundaryEditController::handleDoubleClick(const QPointF & /*pos*/) {
  // The double-click's own press was already appended to m_pointBuffer
  // by handlePress (Qt sends press+release+doubleClick+release for a
  // double-click, not two presses) -- just try to finalize with what's
  // already collected.
  if (!m_measurement || !isPointsMode())
    return;
  finalizePointsEllipse();
}

void BoundaryEditController::cancelPointCollection() {
  if (m_pointBuffer.empty())
    return;
  m_pointBuffer.clear();
  emit previewChanged();
}

void BoundaryEditController::finalizePointsEllipse() {
  if (m_pointBuffer.size() < 3)
    return;  // not enough points yet -- keep collecting

  // ApertureCore's FitEllipse solves a 5x5 linear system directly from
  // raw x^2/xy/y^2 sums, with no coordinate normalization. For points in
  // image-pixel space (e.g. centered around (800, 600) rather than near
  // the origin), that system becomes severely ill-conditioned and
  // produces a visibly wrong (usually too-small) ellipse. Fitting in
  // centroid-relative coordinates and shifting the result back avoids
  // this without touching ApertureCore itself.
  QPointF centroid(0.0, 0.0);
  for (const auto &p : m_pointBuffer)
    centroid += p;
  centroid /= static_cast<double>(m_pointBuffer.size());

  std::vector<aperture::Point> points;
  points.reserve(m_pointBuffer.size());
  for (const auto &p : m_pointBuffer)
    points.push_back(
        aperture::Point{p.x() - centroid.x(), p.y() - centroid.y()});

  const auto type = addModeType();
  auto ellipse = aperture::Ellipse::FitEllipse(points, type);

  m_pointBuffer.clear();
  emit previewChanged();

  if (!ellipse)
    return;

  ellipse->shiftX(centroid.x());
  ellipse->shiftY(centroid.y());

  m_undoStack->push(new AddShapeCommand(m_measurement->boundaries(), type,
                                        std::move(ellipse)));
  emit boundariesChanged();
}

void BoundaryEditController::handleMove(const QPointF &pos) {
  if (m_creating) {
    m_createCurrent = pos;
    emit previewChanged();
  } else if (m_resizingHandle) {
    updateHandleDrag(pos);
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
      return;  // ignore accidental clicks
    auto shape = buildShapeFromRect(rect);
    m_undoStack->push(new AddShapeCommand(m_measurement->boundaries(),
                                          addModeType(), std::move(shape)));
    emit boundariesChanged();
  } else if (m_resizingHandle) {
    commitHandleDrag();
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
  m_resizingHandle = false;
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
  const auto &boundaries = m_measurement->boundaries();

  // Internal boundaries are searched first: they are typically drawn on
  // top of, and nested inside, external ones.
  const auto &internalShapes = boundaries.getInternal();
  for (size_t i = internalShapes.size(); i-- > 0;) {
    if (distanceToContour(*internalShapes[i], pos) <= kHitTolerance)
      return Selection{aperture::TypeLimits::INTERNAL, i};
  }
  const auto &externalShapes = boundaries.getExternal();
  for (size_t i = externalShapes.size(); i-- > 0;) {
    if (distanceToContour(*externalShapes[i], pos) <= kHitTolerance)
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

const aperture::Shape *BoundaryEditController::selectedShape() const {
  if (!m_measurement || !m_selection)
    return nullptr;
  const auto &boundaries = m_measurement->boundaries();
  const auto &container = (m_selection->type == aperture::TypeLimits::EXTERNAL)
                              ? boundaries.getExternal()
                          : (m_selection->type == aperture::TypeLimits::INTERNAL)
                              ? boundaries.getInternal()
                              : boundaries.getApertures();
  if (m_selection->index >= container.size())
    return nullptr;
  return container[m_selection->index].get();
}

std::vector<aperture::HandleDesc> BoundaryEditController::resizeHandles() const {
  std::vector<aperture::HandleDesc> result;
  const aperture::Shape *shape = selectedShape();
  if (!shape)
    return result;

  std::vector<aperture::HandleDesc> all;
  shape->EnumerateHandles(all);
  for (auto &h : all) {
    // Move/Rotate are already covered by dragging the shape's contour
    // directly (see beginMoveDrag). Exactly 4 resize points, one per
    // length/width axis -- AxisResize for the ellipse's semi-axes,
    // EdgeResize for the rectangle's sides (corner handles are skipped:
    // they'd change both dimensions at once instead of one at a time).
    if (h.type == aperture::HandleType::AxisResize ||
        h.type == aperture::HandleType::EdgeResize)
      result.push_back(h);
  }
  return result;
}

std::optional<aperture::HandleDesc> BoundaryEditController::hitTestSelectedHandle(
    const QPointF &pos) const {
  const auto handles = resizeHandles();
  if (handles.empty())
    return std::nullopt;

  // Matches ImageCanvas's on-screen handle marker size (kHandleSize = 12)
  // plus a little slack, so the whole visible square is clickable.
  constexpr double kHandleHitTolerance = 8.0;
  std::optional<aperture::HandleDesc> best;
  double bestDist = kHandleHitTolerance;
  for (const auto &h : handles) {
    const double dx = pos.x() - h.localPos.x;
    const double dy = pos.y() - h.localPos.y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= bestDist) {
      bestDist = dist;
      best = h;
    }
  }
  return best;
}

void BoundaryEditController::beginHandleDrag(const aperture::HandleDesc &handle,
                                             const QPointF &pos) {
  const aperture::Shape *shape = selectedShape();
  if (!shape)
    return;

  m_resizingHandle = true;
  m_activeHandle = handle;
  m_handleDragAnchor = pos;
  m_resizeOriginal = shape->clone();
}

void BoundaryEditController::updateHandleDrag(const QPointF &pos) {
  if (!m_selection || !m_resizeOriginal)
    return;
  auto &container = digitqt::core::mutableContainer(m_measurement->boundaries(),
                                                    m_selection->type);
  if (m_selection->index >= container.size())
    return;

  aperture::DragContext drag;
  drag.handle = m_activeHandle;
  drag.dragStartWorld = {m_handleDragAnchor.x(), m_handleDragAnchor.y()};
  drag.dragCurrentWorld = {pos.x(), pos.y()};
  drag.deltaWorld = {pos.x() - m_handleDragAnchor.x(),
                     pos.y() - m_handleDragAnchor.y()};

  auto preview = m_resizeOriginal->clone();
  preview->ApplyHandleDrag(m_activeHandle, drag);
  container[m_selection->index] = std::move(preview);
  m_measurement->boundaries().notifyShapeModified();
  emit boundariesChanged();
}

void BoundaryEditController::commitHandleDrag() {
  if (!m_selection || !m_resizeOriginal) {
    m_resizingHandle = false;
    return;
  }
  auto &container = digitqt::core::mutableContainer(m_measurement->boundaries(),
                                                    m_selection->type);
  if (m_selection->index < container.size()) {
    auto after = container[m_selection->index]->clone();
    // Roll the live preview mutation back first; ReplaceShapeCommand's
    // redo() will (re)apply it, so the whole drag lands as one undo step.
    container[m_selection->index] = m_resizeOriginal->clone();
    m_undoStack->push(new ReplaceShapeCommand(
        m_measurement->boundaries(), m_selection->type, m_selection->index,
        std::move(m_resizeOriginal), std::move(after)));
  }
  m_resizingHandle = false;
  emit boundariesChanged();
}

}  // namespace digitqt::gui::canvas
