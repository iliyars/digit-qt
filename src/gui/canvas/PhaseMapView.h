#pragma once

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui::canvas {

/**
 * @brief Отображение S2 (Phase Reconstruction): исходное изображение
 * снизу, поверх — тепловая карта фазы и/или изолинии, каждое включается
 * независимо.
 *
 * Ничего не считает сама -- только рисует то, что уже лежит в
 * Measurement::phaseMap() (см. "views never compute").
 */
class PhaseMapView : public QGraphicsView {
  Q_OBJECT
public:
  explicit PhaseMapView(QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);

  /// Перечитать Measurement::phaseMap() и перерисовать и тепловую
  /// карту, и изолинии.
  void refresh();

  void setHeatmapVisible(bool visible);
  void setIsolinesVisible(bool visible);
  void setIsolineStep(double step);

protected:
  void wheelEvent(QWheelEvent *event) override;

private:
  void rebuildHeatmap();
  void rebuildIsolines();

  digitqt::core::Measurement *m_measurement = nullptr;

  QGraphicsScene m_scene;
  QGraphicsPixmapItem *m_backgroundItem = nullptr;
  QGraphicsPixmapItem *m_heatmapItem = nullptr;
  QGraphicsPathItem *m_isolinesItem = nullptr;

  bool m_heatmapVisible = true;
  bool m_isolinesVisible = false;
  double m_isolineStep = 1.0;
};

}  // namespace digitqt::gui::canvas
