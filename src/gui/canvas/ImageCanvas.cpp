#include "ImageCanvas.h"

#include "core/Measurement.h"

#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPixmap>
#include <QScrollBar>
#include <QToolTip>
#include <QWheelEvent>
#include <cmath>

namespace digitqt::gui::canvas {

ImageCanvas::ImageCanvas(BoundaryEditController *boundaryController,
                         FringeTracingController *fringeController,
                         QWidget *parent)
    : QGraphicsView(parent),
      m_boundaryController(boundaryController),
      m_fringeController(fringeController) {
  setScene(&m_scene);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::NoDrag);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  m_previewItem = m_scene.addRect(QRectF(), QPen(Qt::DashLine));
  m_previewItem->setZValue(20.0);
  m_previewItem->hide();

  m_pointsPreviewItem = new QGraphicsPathItem();
  QPen pointsPen(QColor(255, 200, 0));
  pointsPen.setCosmetic(true);
  pointsPen.setWidth(2);
  pointsPen.setStyle(Qt::DotLine);
  m_pointsPreviewItem->setPen(pointsPen);
  m_pointsPreviewItem->setZValue(21.0);
  m_pointsPreviewItem->hide();
  m_scene.addItem(m_pointsPreviewItem);

  connect(m_boundaryController, &BoundaryEditController::boundariesChanged,
          this, &ImageCanvas::rebuildBoundaryItems);
  connect(m_boundaryController, &BoundaryEditController::previewChanged, this,
          &ImageCanvas::updatePreviewItem);
  connect(m_boundaryController, &BoundaryEditController::selectionChanged, this,
          &ImageCanvas::updateSelectionHighlight);

  connect(m_fringeController, &FringeTracingController::seedsChanged, this,
          &ImageCanvas::rebuildFringeItems);
  connect(m_fringeController, &FringeTracingController::tracedLinesChanged,
          this, &ImageCanvas::rebuildFringeItems);
  connect(m_fringeController, &FringeTracingController::selectionChanged, this,
          &ImageCanvas::updateFringeSelectionHighlight);
  connect(m_fringeController, &FringeTracingController::lineEditModeChanged,
          this, &ImageCanvas::updateLineEditOverlay);
}

void ImageCanvas::setMeasurement(digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  if (!m_pixmapItem) {
    m_pixmapItem = m_scene.addPixmap(QPixmap());
    m_pixmapItem->setZValue(0.0);
  }
  if (measurement && measurement->hasImage()) {
    m_pixmapItem->setPixmap(QPixmap::fromImage(measurement->image()));
    m_scene.setSceneRect(m_pixmapItem->boundingRect());
    fitImageToView();
  }
  rebuildBoundaryItems();
  rebuildFringeItems();
}

void ImageCanvas::fitImageToView() {
  if (m_pixmapItem)
    fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}

void ImageCanvas::setActiveController(ActiveController controller) {
  m_activeController = controller;
}

void ImageCanvas::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) {
    m_panning = true;
    m_lastPanPoint = event->pos();
    setCursor(Qt::ClosedHandCursor);
    return;  // right-click is pan-only, never a tool action
  }

  const QPointF pos = mapToScene(event->pos());
  const bool primary = event->button() == Qt::LeftButton;
  if (m_activeController == ActiveController::Boundary)
    m_boundaryController->handlePress(pos, primary);
  else
    m_fringeController->handlePress(pos, primary);
  QGraphicsView::mousePressEvent(event);
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *event) {
  if (m_panning) {
    const QPoint delta = event->pos() - m_lastPanPoint;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    m_lastPanPoint = event->pos();
  }

  const QPointF scenePos = mapToScene(event->pos());
  updateCursorInfoTooltip(scenePos, event->globalPosition().toPoint());

  if (m_activeController == ActiveController::Boundary)
    m_boundaryController->handleMove(scenePos);
  else
    m_fringeController->handleMove(scenePos);
  QGraphicsView::mouseMoveEvent(event);
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) {
    m_panning = false;
    setCursor(Qt::ArrowCursor);
    return;
  }

  if (m_activeController == ActiveController::Boundary)
    m_boundaryController->handleRelease(mapToScene(event->pos()));
  else
    m_fringeController->handleRelease(mapToScene(event->pos()));
  QGraphicsView::mouseReleaseEvent(event);
}

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
  const QPointF pos = mapToScene(event->pos());
  if (m_activeController == ActiveController::Boundary)
    m_boundaryController->handleDoubleClick(pos);
  else
    m_fringeController->handleDoubleClick(pos);
  QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageCanvas::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    if (m_activeController == ActiveController::Boundary)
      m_boundaryController->deleteSelection();
    else
      m_fringeController->deleteSelection();
    return;
  }
  if (event->key() == Qt::Key_Escape) {
    m_boundaryController->cancelPointCollection();
    m_fringeController->exitLineEditMode();
    return;
  }
  QGraphicsView::keyPressEvent(event);
}

void ImageCanvas::wheelEvent(QWheelEvent *event) {
  const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
  scale(factor, factor);
}

void ImageCanvas::setBoundariesVisible(bool visible) {
  m_boundariesVisible = visible;
  for (auto *item : m_boundaryItems)
    item->setVisible(visible);
  if (!visible) {
    m_previewItem->hide();
    m_pointsPreviewItem->hide();
  }
}

void ImageCanvas::setFringeTracingVisible(bool visible) {
  m_fringeVisible = visible;
  for (auto *item : m_seedItems)
    item->setVisible(visible);
  for (auto *item : m_lineItems)
    item->setVisible(visible);
}

void ImageCanvas::updateCursorInfoTooltip(const QPointF &scenePos,
                                          const QPoint &globalPos) {
  if (!m_measurement || !m_measurement->hasImage()) {
    QToolTip::hideText();
    return;
  }

  const auto &img = m_measurement->image();
  const int x = static_cast<int>(std::floor(scenePos.x()));
  const int y = static_cast<int>(std::floor(scenePos.y()));

  if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) {
    QToolTip::hideText();
    return;
  }

  const int intensity = qGray(img.pixel(x, y));
  const QString text =
      tr("x: %1, y: %2\nintensity: %3").arg(x).arg(y).arg(intensity);
  QToolTip::showText(globalPos, text, this);
}

void ImageCanvas::rebuildBoundaryItems() {
  for (auto *item : m_boundaryItems) {
    m_scene.removeItem(item);
    delete item;
  }
  m_boundaryItems.clear();

  if (!m_measurement)
    return;
  const auto &boundaries = m_measurement->boundaries();

  auto addAll = [&](const auto &shapes) {
    for (size_t i = 0; i < shapes.size(); ++i) {
      auto *item = new BoundaryItem(shapes[i].get(), i);
      item->setVisible(m_boundariesVisible);
      m_scene.addItem(item);
      m_boundaryItems.push_back(item);
    }
  };
  addAll(boundaries.getExternal());
  addAll(boundaries.getInternal());
  addAll(boundaries.getApertures());

  updateSelectionHighlight();
}

void ImageCanvas::updatePreviewItem() {
  auto rect = m_boundaryController->creationPreview();
  if (rect && m_boundariesVisible) {
    m_previewItem->setRect(*rect);
    m_previewItem->show();
  } else {
    m_previewItem->hide();
  }

  const auto &points = m_boundaryController->pointBufferPreview();
  if (!points.empty() && m_boundariesVisible) {
    QPainterPath path;
    path.moveTo(points.front());
    for (size_t i = 1; i < points.size(); ++i)
      path.lineTo(points[i]);
    for (const auto &p : points)
      path.addEllipse(p, 3.0, 3.0);
    m_pointsPreviewItem->setPath(path);
    m_pointsPreviewItem->show();
  } else {
    m_pointsPreviewItem->hide();
  }
}

void ImageCanvas::updateSelectionHighlight() {
  auto sel = m_boundaryController->selection();
  for (auto *item : m_boundaryItems) {
    const bool isSelected = sel && item->shapeType() == sel->type &&
                            item->shapeIndex() == sel->index;
    item->setSelectedStyle(isSelected);
  }
}

void ImageCanvas::rebuildFringeItems() {
  for (auto *item : m_seedItems) {
    m_scene.removeItem(item);
    delete item;
  }
  m_seedItems.clear();
  for (auto *item : m_lineItems) {
    m_scene.removeItem(item);
    delete item;
  }
  m_lineItems.clear();

  if (!m_measurement)
    return;
  const auto &tracingData = m_measurement->fringeTracing();

  const auto &seeds = tracingData.seeds();
  for (size_t i = 0; i < seeds.size(); ++i) {
    auto *item = new SeedItem(seeds[i].x, seeds[i].y, i);
    item->setVisible(m_fringeVisible);
    m_scene.addItem(item);
    m_seedItems.push_back(item);
  }

  const auto &lines = tracingData.tracedLines();
  for (size_t i = 0; i < lines.size(); ++i) {
    auto *item = new TracedLineItem(lines[i], i);
    item->setVisible(m_fringeVisible);
    m_scene.addItem(item);
    m_lineItems.push_back(item);
  }

  updateFringeSelectionHighlight();
  updateLineEditOverlay();
}

void ImageCanvas::updateFringeSelectionHighlight() {
  auto sel = m_fringeController->selection();
  for (auto *item : m_seedItems)
    item->setSelectedStyle(sel && item->seedIndex() == *sel);
}

void ImageCanvas::updateLineEditOverlay() {
  for (auto *item : m_lineVertexItems) {
    m_scene.removeItem(item);
    delete item;
  }
  m_lineVertexItems.clear();

  for (auto *item : m_lineItems)
    item->setEditing(false);

  const auto editingIndex = m_fringeController->editingLineIndex();
  if (!editingIndex || !m_measurement)
    return;

  if (*editingIndex < m_lineItems.size())
    m_lineItems[*editingIndex]->setEditing(true);

  const auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*editingIndex >= lines.size())
    return;

  const auto selectedPoint = m_fringeController->selectedPointIndex();
  constexpr double kHandleRadius = 3.0;
  const auto &linePoints = lines[*editingIndex];
  for (size_t i = 0; i < linePoints.size(); ++i) {
    const auto &point = linePoints[i];
    const bool isSelected = selectedPoint && *selectedPoint == i;
    auto *handle = new QGraphicsEllipseItem(
        point.x - kHandleRadius, point.y - kHandleRadius, kHandleRadius * 2,
        kHandleRadius * 2);
    handle->setPen(QPen(Qt::white, 1));
    handle->setBrush(isSelected ? QColor(255, 120, 0) : QColor(0, 150, 255));
    handle->setZValue(isSelected ? 41.0 : 40.0);
    m_scene.addItem(handle);
    m_lineVertexItems.push_back(handle);
  }
}

}  // namespace digitqt::gui::canvas
