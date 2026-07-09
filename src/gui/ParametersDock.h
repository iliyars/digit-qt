#pragma once

#include "core/pipeline/PipelineStageId.h"

#include <QDockWidget>

class QLabel;
class QComboBox;

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui {

/**
 * @brief Right-hand dock: parameters for the currently selected stage.
 *
 * Today this shows the fringe-tracing algorithm picker plus a short
 * read-only info blurb per stage ("not implemented" for anything past
 * Setup). As each stage gets a real implementation, its page here grows
 * into actual editable parameter widgets -- this dock is the seam where
 * that happens, not MainWindow.
 */
class ParametersDock : public QDockWidget {
  Q_OBJECT
public:
  explicit ParametersDock(QWidget *parent = nullptr);

  void setMeasurement(digitqt::core::Measurement *measurement);
  void setStage(digitqt::core::pipeline::StageId id);

  /// Re-reads whatever the current stage's page depends on (e.g.
  /// boundary counts) and refreshes the displayed text.
  void refresh();

private slots:
  void onAlgorithmChanged(int index);
  void onFringeCenterModeChanged(int index);

private:
  digitqt::core::Measurement *m_measurement = nullptr;
  digitqt::core::pipeline::StageId m_currentStage =
      digitqt::core::pipeline::StageId::Setup;
  QComboBox *m_algorithmCombo;
  QComboBox *m_fringeCenterCombo;
  QLabel *m_label;
};

}  // namespace digitqt::gui
