#pragma once
#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QCheckBox;
class QPlainTextEdit;
namespace digitqt::core {
class Measurement;
namespace pipeline {
class Pipeline;
}
}  // namespace digitqt::core
namespace digitqt::gui {
// Presentation and user intent only. MainWindow owns loading, edits and jobs.
class PsiWorkflowPanel : public QWidget {
  Q_OBJECT
public:
  explicit PsiWorkflowPanel(QWidget *parent = nullptr);
  void setMeasurement(digitqt::core::Measurement *measurement,
                      digitqt::core::pipeline::Pipeline *pipeline, const QStringList &paths = {});
  void refresh();
  void setWarnings(const QStringList &warnings);
signals:
  void openRequested();
  void autoApertureRequested();
  void drawApertureRequested();
  void editApertureRequested();
  void frameSelected(int index);
  void calculateRequested();
  void sourceRequested();
  void phaseRequested();
  void wavefrontRequested();
  void analysisRequested();
  void exportMapRequested();
  void exportReportRequested();

private:
  digitqt::core::Measurement *m_measurement = nullptr;
  digitqt::core::pipeline::Pipeline *m_pipeline = nullptr;
  QLabel *m_summary;
  QLabel *m_next;
  QLabel *m_quality;
  QListWidget *m_frames;
  QWidget *m_actions;
  QWidget *m_results;
  QCheckBox *m_fullFrame;
  QPushButton *m_calculate;
  QPushButton *m_phase;
  QPushButton *m_wavefront;
  QPushButton *m_analysis;
  QPushButton *m_exportMap;
  QPushButton *m_exportReport;
  QPlainTextEdit *m_warnings;
};
}  // namespace digitqt::gui
