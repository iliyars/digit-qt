#pragma once

#include "HeightPalette.h"

#include "ColorLegendWidget.h"
#include "core/PhaseMap.h"
#include "core/ApertureSamples.h"

#include <QString>
#include <QWidget>

namespace digitqt::core {
class Measurement;
class PhaseMap;
}  // namespace digitqt::core

namespace digitqt::gui::canvas {

/**
 * @brief Простой поворачиваемый 3D-график поверхности -- остатка после
 * S5 (Modal Analysis: Measurement::modalAnalysis().residual), без OpenGL
 * и без модуля Qt Data Visualization -- изометрическая проекция от руки
 * через QPainter, с сортировкой четырёхугольников сетки по глубине
 * (painter's algorithm).
 *
 * Перетаскивание мышью меняет азимут/угол наклона, колесо — масштаб.
 * Ничего не считает сама -- только рисует то, что уже лежит в
 * Measurement::modalAnalysis() (см. "views never compute").
 */
class Surface3DView : public QWidget {
  Q_OBJECT
public:
  void setHeightPalette(HeightPalette palette);
  explicit Surface3DView(QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);

  /// Перечитать Measurement::modalAnalysis().residual и перерисовать.
  void refresh();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  HeightPalette m_palette = HeightPalette::Plasma;
  digitqt::core::Measurement *m_measurement = nullptr;
  // GUI-owned snapshot: background fitting may replace the measurement result.
  digitqt::core::PhaseMap m_map;
  digitqt::core::ApertureGeometry m_geometry;
  QString m_unitSuffix = QStringLiteral(" nm");

  double m_azimuth = 0.0;     // радианы, поворот вокруг вертикальной оси
  double m_elevation = 0.55;  // радианы, наклон камеры
  double m_zoom = 1.0;

  bool m_dragging = false;
  QPoint m_lastMousePos;

  ColorLegendWidget *m_legend = nullptr;
};

}  // namespace digitqt::gui::canvas
