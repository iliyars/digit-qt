#pragma once

#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <vector>

#include "BoundaryEditController.h"
#include "BoundaryItem.h"

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui::canvas {

/**
 * @brief Qt-facing view: displays the current Measurement's image and its
 * boundaries, and forwards pointer/keyboard events to BoundaryEditController.
 *
 * Contains no decision logic of its own (per "views never compute").
 */
class ImageCanvas : public QGraphicsView {
  Q_OBJECT
public:
  explicit ImageCanvas(BoundaryEditController *controller,
                       QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);
  void fitImageToView();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private slots:
  void rebuildBoundaryItems();
  void updatePreviewItem();
  void updateSelectionHighlight();

private:
  BoundaryEditController *m_controller;
  digitqt::core::Measurement *m_measurement = nullptr;

  QGraphicsScene m_scene;
  QGraphicsPixmapItem *m_pixmapItem = nullptr;
  QGraphicsRectItem *m_previewItem = nullptr;
  std::vector<BoundaryItem *> m_boundaryItems;
};

} // namespace digitqt::gui::canvas
