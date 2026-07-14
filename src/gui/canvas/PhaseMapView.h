#pragma once

#include "ColorLegendWidget.h"

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace digitqt::core {
class Measurement;
class PhaseMap;
}  // namespace digitqt::core

namespace digitqt::gui::canvas {

/**
 * @brief Отображение плотной скалярной карты поверх изображения:
 * исходное изображение снизу, поверх — тепловая карта и/или изолинии,
 * каждое включается независимо.
 *
 * Переиспользуется и для S2 (Phase Reconstruction, Measurement::
 * phaseMap(), единицы — номер полосы), и для S4 (Wavefront
 * Reconstruction, Measurement::wavefrontMap(), единицы — нанометры) —
 * см. setSource(). Ничего не считает сама -- только рисует то, что уже
 * лежит в соответствующей карте (см. "views never compute").
 */
class PhaseMapView : public QGraphicsView {
  Q_OBJECT
public:
  enum class Source { Phase, Wavefront };

  explicit PhaseMapView(QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);

  /// Какую карту показывать -- Measurement::phaseMap() (S2) или
  /// Measurement::wavefrontMap() (S4). По умолчанию Phase.
  void setSource(Source source);

  /// Перечитать текущую карту (см. setSource()) и перерисовать и
  /// тепловую карту, и изолинии.
  void refresh();

  void setHeatmapVisible(bool visible);
  void setIsolinesVisible(bool visible);
  void setIsolineStep(double step);
  double isolineStep() const { return m_isolineStep; }

protected:
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void rebuildHeatmap();
  void rebuildIsolines();
  void repositionLegend();
  const digitqt::core::PhaseMap *currentMap() const;

  digitqt::core::Measurement *m_measurement = nullptr;
  Source m_source = Source::Phase;

  QGraphicsScene m_scene;
  QGraphicsPixmapItem *m_backgroundItem = nullptr;
  QGraphicsPixmapItem *m_heatmapItem = nullptr;
  QGraphicsPathItem *m_isolinesItem = nullptr;
  ColorLegendWidget *m_legend = nullptr;

  bool m_heatmapVisible = true;
  bool m_isolinesVisible = false;
  double m_isolineStep = 1.0;
};

}  // namespace digitqt::gui::canvas
