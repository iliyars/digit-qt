#pragma once

#include <QDockWidget>

#include "core/pipeline/PipelineStageId.h"

class QLabel;

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui {

/**
 * @brief Right-hand dock: parameters for the currently selected stage.
 *
 * Today this only shows a short read-only blurb per stage (boundary counts
 * for S0a, "not implemented" otherwise). As each stage gets a real
 * implementation, its page here grows into actual editable parameter
 * widgets -- this dock is the seam where that happens, not MainWindow.
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

private:
  digitqt::core::Measurement *m_measurement = nullptr;
  digitqt::core::pipeline::StageId m_currentStage =
      digitqt::core::pipeline::StageId::S0;
  QLabel *m_label;
};

} // namespace digitqt::gui
