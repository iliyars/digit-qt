#include "ImageCanvas.h"

#include "core/Measurement.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QWheelEvent>

namespace digitqt::gui::canvas {

ImageCanvas::ImageCanvas(BoundaryEditController *controller, QWidget *parent)
    : QGraphicsView(parent), m_controller(controller) {
  setScene(&m_scene);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::NoDrag);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  m_previewItem = m_scene.addRect(QRectF(), QPen(Qt::DashLine));
  m_previewItem->setZValue(20.0);
  m_previewItem->hide();

  connect(m_controller, &BoundaryEditController::boundariesChanged, this,
          &ImageCanvas::rebuildBoundaryItems);
  connect(m_controller, &BoundaryEditController::previewChanged, this,
          &ImageCanvas::updatePreviewItem);
  connect(m_controller, &BoundaryEditController::selectionChanged, this,
          &ImageCanvas::updateSelectionHighlight);
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
}

void ImageCanvas::fitImageToView() {
  if (m_pixmapItem)
    fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}

void ImageCanvas::mousePressEvent(QMouseEvent *event) {
  m_controller->handlePress(mapToScene(event->pos()),
                            event->button() == Qt::LeftButton);
  QGraphicsView::mousePressEvent(event);
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *event) {
  m_controller->handleMove(mapToScene(event->pos()));
  QGraphicsView::mouseMoveEvent(event);
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent *event) {
  m_controller->handleRelease(mapToScene(event->pos()));
  QGraphicsView::mouseReleaseEvent(event);
}

void ImageCanvas::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    m_controller->deleteSelection();
    return;
  }
  QGraphicsView::keyPressEvent(event);
}

void ImageCanvas::wheelEvent(QWheelEvent *event) {
  const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
  scale(factor, factor);
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
  auto rect = m_controller->creationPreview();
  if (rect) {
    m_previewItem->setRect(*rect);
    m_previewItem->show();
  } else {
    m_previewItem->hide();
  }
}

void ImageCanvas::updateSelectionHighlight() {
  auto sel = m_controller->selection();
  for (auto *item : m_boundaryItems) {
    const bool isSelected = sel && item->shapeType() == sel->type &&
                            item->shapeIndex() == sel->index;
    item->setSelectedStyle(isSelected);
  }
}

} // namespace digitqt::gui::canvas
