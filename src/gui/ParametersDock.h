#pragma once

#include "core/pipeline/PipelineStageId.h"

#include <QDockWidget>


class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QWidget;

namespace digitqt::core {
class Measurement;
}
namespace digitqt::gui::canvas {
class FringeTracingController;
class PhaseMapView;
}  // namespace digitqt::gui::canvas

namespace digitqt::gui {

/**
 * @brief Right-hand dock: parameters for the currently selected stage.
 *
 * Today this shows the fringe-tracing algorithm picker, a fringe-order
 * editor (active only while a traced line is being edited on the
 * canvas), plus a short read-only info blurb per stage ("not
 * implemented" for anything past Setup). As each stage gets a real
 * implementation, its page here grows into actual editable parameter
 * widgets -- this dock is the seam where that happens, not MainWindow.
 */
class ParametersDock : public QDockWidget {
  Q_OBJECT
public:
  explicit ParametersDock(QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);
  void setFringeController(digitqt::gui::canvas::FringeTracingController *controller);
  void setPhaseMapView(digitqt::gui::canvas::PhaseMapView *view);
  void setStage(digitqt::core::pipeline::StageId id);

  /// Re-reads whatever the current stage's page depends on (e.g.
  /// boundary counts) and refreshes the displayed text.
  void refresh();

private slots:
  void onAlgorithmChanged(int index);
  void onFringeCenterModeChanged(int index);
  void onFringeOrderSpinChanged(double value);
  void onWavelengthChanged(double value);
  void onIsolineStepChanged(double value);
  void refreshOrderEditor();

private:
  digitqt::core::Measurement *m_measurement = nullptr;
  digitqt::gui::canvas::FringeTracingController *m_fringeController = nullptr;
  digitqt::gui::canvas::PhaseMapView *m_phaseMapView = nullptr;
  digitqt::core::pipeline::StageId m_currentStage = digitqt::core::pipeline::StageId::Setup;

  QComboBox *m_algorithmCombo;
  QComboBox *m_fringeCenterCombo;

  QWidget *m_orderEditorRow;
  QDoubleSpinBox *m_orderSpin;

  QWidget *m_wavelengthRow;
  QDoubleSpinBox *m_wavelengthSpin;

  QWidget *m_isolineStepRow;
  QDoubleSpinBox *m_isolineStepSpin;

  QLabel *m_label;
};

}  // namespace digitqt::gui
