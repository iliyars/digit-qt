#pragma once

#include "BoundaryEditController.h"
#include "BoundaryItem.h"
#include "FringeTracingController.h"
#include "SeedItem.h"
#include "TracedLineItem.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <vector>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui::canvas {

/// Which controller currently owns pointer/keyboard interaction on the
/// canvas. Exactly one is "listening" at a time, chosen by MainWindow
/// based on which pipeline stage is selected.
enum class ActiveController {
  Boundary,
  FringeTracing,
};

/**
 * @brief Qt-facing view: displays the current Measurement's image and its
 * overlays (boundaries, fringe-tracing seeds/lines), and forwards
 * pointer/keyboard events to whichever controller is currently active.
 *
 * Contains no decision logic of its own (per "views never compute").
 */
class ImageCanvas : public QGraphicsView {
  Q_OBJECT
public:
  ImageCanvas(BoundaryEditController *boundaryController,
              FringeTracingController *fringeController,
              QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);
  void fitImageToView();

  void setActiveController(ActiveController controller);
  ActiveController activeController() const { return m_activeController; }

  /// S0/Setup shows just the image; Setup shows it with boundaries on
  /// top; S1 shows it with seeds/traced lines on top instead.
  void setBoundariesVisible(bool visible);
  void setFringeTracingVisible(bool visible);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private slots:
  void rebuildBoundaryItems();
  void updatePreviewItem();
  void updateSelectionHighlight();
  void rebuildFringeItems();
  void updateFringeSelectionHighlight();
  void updateLineEditOverlay();

private:
  void updateCursorInfoTooltip(const QPointF &scenePos,
                               const QPoint &globalPos);
  BoundaryEditController *m_boundaryController;
  FringeTracingController *m_fringeController;
  ActiveController m_activeController = ActiveController::Boundary;
  digitqt::core::Measurement *m_measurement = nullptr;

  QGraphicsScene m_scene;
  QGraphicsPixmapItem *m_pixmapItem = nullptr;
  QGraphicsRectItem *m_previewItem = nullptr;
  QGraphicsPathItem *m_pointsPreviewItem = nullptr;
  std::vector<BoundaryItem *> m_boundaryItems;
  bool m_boundariesVisible = true;

  std::vector<SeedItem *> m_seedItems;
  std::vector<TracedLineItem *> m_lineItems;
  bool m_fringeVisible = true;
  std::vector<QGraphicsEllipseItem *>
      m_lineVertexItems;  // handles for the line under edit

  // Right-button pan (separate from the left-button tool interactions)
  bool m_panning = false;
  QPoint m_lastPanPoint;
};

}  // namespace digitqt::gui::canvas
