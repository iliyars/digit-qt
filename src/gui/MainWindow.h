#pragma once

#include "canvas/BoundaryEditController.h"
#include "canvas/FringeTracingController.h"
#include "canvas/ImageCanvas.h"
#include "core/Measurement.h"
#include "core/pipeline/Pipeline.h"

#include <QMainWindow>
#include <memory>

class QLabel;
class QUndoStack;
class QStackedWidget;
class QToolBar;

namespace digitqt::gui {

class PipelineTreeDock;
class ParametersDock;
class NotImplementedPage;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);

private slots:
  void openImage();
  void updateStatusBar();
  void onStageSelected(digitqt::core::pipeline::StageId id);
  void runTracing();

private:
  void buildMenusAndToolbars();
  void buildLanguageMenu();
  void buildDocks();

  std::unique_ptr<digitqt::core::Measurement> m_measurement;
  std::unique_ptr<digitqt::core::pipeline::Pipeline> m_pipeline;

  QUndoStack *m_undoStack;
  digitqt::gui::canvas::BoundaryEditController *m_controller;
  digitqt::gui::canvas::FringeTracingController *m_fringeController;
  digitqt::gui::canvas::ImageCanvas *m_canvas;

  QStackedWidget *m_centralStack;
  NotImplementedPage *m_notImplementedPage;
  PipelineTreeDock *m_pipelineDock;
  ParametersDock *m_parametersDock;
  QToolBar *m_setupToolBar;

  QLabel *m_statusLabel;
};

}  // namespace digitqt::gui
