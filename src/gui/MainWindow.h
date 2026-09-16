#pragma once

#include "canvas/BoundaryEditController.h"
#include "canvas/FringeTracingController.h"
#include "canvas/ImageCanvas.h"
#include "canvas/PhaseMapView.h"
#include "canvas/Surface3DView.h"
#include "core/Measurement.h"
#include "core/pipeline/Pipeline.h"

#include <QList>
#include <QMainWindow>
#include <functional>
#include <memory>


class QLabel;
class QProgressBar;
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
  void importMtr();
  void updateStatusBar();
  void onStageSelected(digitqt::core::pipeline::StageId id);
  void runTracing();
  void computePhase();
  // `then` (if given) runs after this stage's own success/failure
  // handling and view refresh -- see importMtr(), which chains
  // computeWavefront() -> computeModalAnalysis() this way instead of
  // firing both at once (each now runs on a background thread, so
  // firing both immediately would race S5 reading wavefrontMap()
  // before S4 finished writing it).
  void computeWavefront(std::function<void()> then = {});
  void computeModalAnalysis(std::function<void()> then = {});

private:
  void buildMenusAndToolbars();
  void buildLanguageMenu();
  void buildDocks();

  /// Runs `compute` on a background thread (QtConcurrent) and shows an
  /// indeterminate busy indicator + `busyText` in the status bar while
  /// it runs. Disables the UI listed in m_disableWhileBusy for the
  /// duration (compute() implementations only touch Measurement, never
  /// Qt GUI state, but nothing else may touch that same Measurement
  /// concurrently -- see PipelineStage.h's "Measurement owns all data"
  /// rule). `onDone(result)` runs on the GUI thread once finished, with
  /// the UI already re-enabled.
  void runComputeInBackground(const QString &busyText, std::function<bool()> compute,
                              std::function<void(bool)> onDone);
  void setUiBusy(bool busy, const QString &text = {});

  std::unique_ptr<digitqt::core::Measurement> m_measurement;
  std::unique_ptr<digitqt::core::pipeline::Pipeline> m_pipeline;

  QUndoStack *m_undoStack;
  digitqt::gui::canvas::BoundaryEditController *m_controller;
  digitqt::gui::canvas::FringeTracingController *m_fringeController;
  digitqt::gui::canvas::ImageCanvas *m_canvas;
  digitqt::gui::canvas::PhaseMapView *m_phaseMapView;
  digitqt::gui::canvas::Surface3DView *m_surface3DView;
  digitqt::gui::canvas::PhaseMapView
      *m_modalPhaseMapView;  // 2D-вид остатка на странице S5, рядом с 3D

  QStackedWidget *m_centralStack;
  NotImplementedPage *m_notImplementedPage;
  PipelineTreeDock *m_pipelineDock;
  ParametersDock *m_parametersDock;
  QToolBar *m_setupToolBar;
  QToolBar *m_phaseToolBar;
  QToolBar *m_wavefrontToolBar;
  QToolBar *m_modalToolBar;

  QLabel *m_statusLabel;
  QProgressBar *m_busyIndicator;
  /// Widgets disabled while a background compute is running -- built
  /// once at the end of the constructor. Deliberately excludes the
  /// status bar itself (m_busyIndicator lives there and must stay
  /// visible/enabled).
  QList<QWidget *> m_disableWhileBusy;
};

}  // namespace digitqt::gui
