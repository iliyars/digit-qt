#include "MainWindow.h"

#include "gui/FiducialReferenceDialog.h"
#include "gui/IconFactory.h"
#include "gui/NotImplementedPage.h"
#include "gui/ParametersDock.h"
#include "gui/PipelineTreeDock.h"
#include "core/FiducialTransformFit.h"
#include "core/ImagePadding.h"
#include "formats/FormatRegistry.h"
#include "formats/IdfFile.h"
#include "io/FrnExporter.h"
#include "io/ImageLoader.h"
#include "io/ModalReportExporter.h"
#include "io/MtrExporter.h"

#include <QActionGroup>
#include <QCollator>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QUndoStack>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>

namespace digitqt::gui {

using digitqt::core::pipeline::StageId;
using digitqt::gui::canvas::ActiveController;
using digitqt::gui::canvas::EditMode;
using digitqt::gui::canvas::FiducialEditMode;
using digitqt::gui::canvas::FringeEditMode;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_measurement(std::make_unique<digitqt::core::Measurement>()),
      m_pipeline(std::make_unique<digitqt::core::pipeline::Pipeline>()),
      m_undoStack(new QUndoStack(this)),
      m_controller(new digitqt::gui::canvas::BoundaryEditController(m_undoStack, this)),
      m_fringeController(new digitqt::gui::canvas::FringeTracingController(m_undoStack, this)),
      m_fiducialController(new digitqt::gui::canvas::FiducialController(m_undoStack, this)) {
  setWindowTitle(tr("DigitQt — Interferogram Processing"));
  resize(1400, 900);

  // --- Central view: one page for image-based stages (Setup, S1),
  // a shared placeholder page for everything else. ---
  m_centralStack = new QStackedWidget(this);
  m_canvas = new digitqt::gui::canvas::ImageCanvas(m_controller, m_fringeController,
                                                   m_fiducialController, this);
  m_phaseMapView = new digitqt::gui::canvas::PhaseMapView(this);
  m_surface3DView = new digitqt::gui::canvas::Surface3DView(this);
  m_modalPhaseMapView = new digitqt::gui::canvas::PhaseMapView(this);
  m_notImplementedPage = new NotImplementedPage(this);

  // S5 page: 3D residual (left) + 2D heatmap/isolines of the same
  // residual (right), side by side -- matches the reference tool's
  // "3D | 2D contour" layout.
  auto *modalSplitter = new QSplitter(Qt::Horizontal, this);
  modalSplitter->addWidget(m_surface3DView);
  modalSplitter->addWidget(m_modalPhaseMapView);
  modalSplitter->setStretchFactor(0, 1);
  modalSplitter->setStretchFactor(1, 1);

  m_centralStack->addWidget(m_canvas);        // index 0: Setup
  m_centralStack->addWidget(m_phaseMapView);  // index 1: S2/S4 (Phase/Wavefront)
  m_centralStack->addWidget(modalSplitter);   // index 2: S5 (Modal Analysis, 3D + 2D residual)
  m_centralStack->addWidget(m_notImplementedPage);  // index 3: everything else
  setCentralWidget(m_centralStack);

  m_statusLabel = new QLabel(this);
  statusBar()->addPermanentWidget(m_statusLabel);

  m_busyIndicator = new QProgressBar(this);
  m_busyIndicator->setRange(0, 0);  // indeterminate ("marquee") style
  m_busyIndicator->setMaximumWidth(120);
  m_busyIndicator->setTextVisible(false);
  m_busyIndicator->setVisible(false);
  statusBar()->addPermanentWidget(m_busyIndicator);

  connect(m_controller, &digitqt::gui::canvas::BoundaryEditController::boundariesChanged, this,
          &MainWindow::updateStatusBar);
  connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::seedsChanged, this,
          &MainWindow::updateStatusBar);
  connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::tracedLinesChanged,
          this, &MainWindow::updateStatusBar);
  connect(m_fiducialController, &digitqt::gui::canvas::FiducialController::fiducialsChanged, this,
          &MainWindow::updateStatusBar);

  buildMenusAndToolbars();
  buildLanguageMenu();
  buildDocks();

  m_disableWhileBusy = {menuBar(),         m_setupToolBar,    m_phaseToolBar,
                        m_wavefrontToolBar, m_modalToolBar,    m_pipelineDock,
                        m_parametersDock,   m_centralStack};

  m_controller->setMeasurement(m_measurement.get());
  m_fringeController->setMeasurement(m_measurement.get());
  m_fringeController->setPipeline(m_pipeline.get());
  m_fiducialController->setMeasurement(m_measurement.get());
  m_canvas->setMeasurement(m_measurement.get());
  m_phaseMapView->setMeasurement(m_measurement.get());
  m_surface3DView->setMeasurement(m_measurement.get());
  m_modalPhaseMapView->setMeasurement(m_measurement.get());
  m_modalPhaseMapView->setSource(digitqt::gui::canvas::PhaseMapView::Source::Residual);
  m_pipelineDock->setPipeline(m_pipeline.get());
  m_parametersDock->setMeasurement(m_measurement.get());
  m_parametersDock->setFringeController(m_fringeController);
  m_parametersDock->setPhaseMapView(m_phaseMapView);
  onStageSelected(StageId::Setup);
  updateStatusBar();
}

void MainWindow::buildMenusAndToolbars() {
  // --- File menu ---
  auto *fileMenu = menuBar()->addMenu(tr("&File"));
  auto *openAction = fileMenu->addAction(tr("&Open Image..."), this, &MainWindow::openImage);
  auto *openSeriesAction = fileMenu->addAction(tr("Open Phase-&Shifting Series..."), this,
                                               &MainWindow::openPhaseShiftingSeries);
  openSeriesAction->setToolTip(
      tr("Load 3 or more frames of the same interferogram taken with different phase "
         "shifts, for the Phase Shifting (PSI) method"));
  auto *importAction = fileMenu->addAction(tr("&Import..."), this, &MainWindow::importFile);
  importAction->setToolTip(
      tr("Load a WinFringe .mtr wavefront matrix (straight into Phase/Wavefront/Modal "
         "Analysis) or a .frn file with aperture shapes and numbered fringes (into Setup)"));
  auto *exportMtrAction = fileMenu->addAction(tr("Export &Wavefront (.mtr)..."));
  connect(exportMtrAction, &QAction::triggered, this, [this] {
    if (m_measurement->wavefrontMap().isEmpty()) {
      QMessageBox::warning(this, tr("Export"), tr("No wavefront map yet -- run S4 first."));
      return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Wavefront"), QString(),
                                                      tr("WinFringe Matrix (*.mtr)"));
    if (path.isEmpty())
      return;

    // WinFringe ожидает волны (Units=WAV), у нас wavefrontMap() в нанометрах.
    const auto &waves = m_measurement->phaseMap();
    // const auto &wf = m_measurement->wavefrontMap();
    // digitqt::core::PhaseMap waves(wf.width(), wf.height());
    // for (int y = 0; y < wf.height(); ++y)
    //   for (int x = 0; x < wf.width(); ++x)
    //     if (wf.hasValue(x, y))
    //       waves.setValue(x, y, wf.value(x, y) / m_measurement->wavelengthNm());

    QString err;
    if (!digitqt::core::io::writeMtrFile(path, waves, err))
      QMessageBox::warning(this, tr("Export"), tr("Export failed:\n%1").arg(err));
  });
  auto *exportFrnAction = fileMenu->addAction(tr("Export &Fringes (.frn)..."));
  connect(exportFrnAction, &QAction::triggered, this, [this] {
    const auto &lines = m_measurement->fringeTracing().tracedLines();
    if (lines.empty()) {
      QMessageBox::warning(this, tr("Export"), tr("No numbered fringe lines yet -- trace fringes first."));
      return;
    }
    const bool hasSynthetic = std::any_of(lines.begin(), lines.end(),
                                          [](const auto &line) { return line.isSynthetic(); });
    if (hasSynthetic) {
      QMessageBox::information(
          this, tr("Export"),
          tr("Some fringe lines contain auto-generated (not measured) points added by "
             "\"Extend fringes to aperture edge\" -- the exported data in those regions "
             "is an estimate, not a direct measurement."));
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Fringes"), QString(),
                                                       tr("WinFringe Interferogram (*.frn)"));
    if (path.isEmpty())
      return;

    QString err;
    if (!digitqt::core::io::writeFrnFile(
            path, m_measurement->boundaries(), lines, m_measurement->image().width(),
            m_measurement->image().height(),
            QFileInfo(QString::fromStdString(m_measurement->imagePath())).fileName(), err))
      QMessageBox::warning(this, tr("Export"), tr("Export failed:\n%1").arg(err));
  });
  auto *exportReportAction = fileMenu->addAction(tr("Export &Report (.txt)..."));
  exportReportAction->setToolTip(
      tr("Save the S5 (Polynomial / Modal Analysis) aberrations, residual and RMS/PV/Strehl "
         "as a plain text report"));
  connect(exportReportAction, &QAction::triggered, this, [this] {
    if (m_measurement->modalAnalysis().isEmpty()) {
      QMessageBox::warning(this, tr("Export"),
                           tr("No S5 (Polynomial / Modal Analysis) result yet -- fit aberrations "
                              "first."));
      return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Report"), QString(),
                                                       tr("Text Report (*.txt)"));
    if (path.isEmpty())
      return;

    QString err;
    if (!digitqt::core::io::writeModalReport(path, *m_measurement, err))
      QMessageBox::warning(this, tr("Export"), tr("Export failed:\n%1").arg(err));
  });
  openAction->setShortcut(QKeySequence::Open);
  fileMenu->addSeparator();
  fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

  // --- Edit menu ---
  auto *editMenu = menuBar()->addMenu(tr("&Edit"));
  auto *undoAction = m_undoStack->createUndoAction(this, tr("&Undo"));
  undoAction->setShortcut(QKeySequence::Undo);
  auto *redoAction = m_undoStack->createRedoAction(this, tr("&Redo"));
  redoAction->setShortcut(QKeySequence::Redo);
  editMenu->addAction(undoAction);
  editMenu->addAction(redoAction);

  // QUndoCommand::undo()/redo() mutate Measurement directly and have no
  // way to emit Qt signals themselves, so an undo/redo triggered from
  // here (Ctrl+Z/Ctrl+Shift+Z, or these menu actions) bypasses the
  // controllers entirely -- without this, the canvas would never learn
  // the data changed and would keep showing stale boundaries/seeds/lines.
  // indexChanged() fires for every push()/undo()/redo()/clear(), so this
  // also re-fires after a normal push (redundant with the controller's
  // own emit right after push(), but harmless -- rebuilding view items
  // twice from the same data is a no-op visually).
  connect(m_undoStack, &QUndoStack::indexChanged, this, [this](int) {
    m_controller->notifyExternalChange();
    m_fringeController->notifyExternalChange();
    m_fiducialController->notifyExternalChange();
  });

  // --- Setup toolbar: boundaries + fringe tracing, all in one place ---
  // (both are part of the single merged "Setup" pipeline stage/screen)
  auto *toolBar = addToolBar(tr("Setup"));
  m_setupToolBar = toolBar;
  toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  toolBar->setIconSize(QSize(22, 22));
  toolBar->setMovable(false);

  auto *modeGroup = new QActionGroup(this);
  modeGroup->setExclusive(true);

  using digitqt::gui::icons::cursorIcon;
  using digitqt::gui::icons::seedIcon;
  using digitqt::gui::icons::shapeIcon;

  const QColor externalColor(0, 190, 60);
  const QColor internalColor(220, 40, 40);

  auto addBoundaryModeAction = [&](const QIcon &icon, const QString &tooltip, EditMode mode,
                                   bool checked = false) {
    auto *action = toolBar->addAction(icon, tooltip);
    action->setToolTip(tooltip);
    action->setCheckable(true);
    action->setChecked(checked);
    modeGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, mode] {
      m_canvas->setActiveController(ActiveController::Boundary);
      m_controller->setMode(mode);
    });
    return action;
  };

  auto addFringeModeAction = [&](const QIcon &icon, const QString &tooltip, FringeEditMode mode) {
    auto *action = toolBar->addAction(icon, tooltip);
    action->setToolTip(tooltip);
    action->setCheckable(true);
    modeGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, mode] {
      m_canvas->setActiveController(ActiveController::FringeTracing);
      m_fringeController->setMode(mode);
    });
    return action;
  };

  auto addFiducialModeAction = [&](const QIcon &icon, const QString &tooltip,
                                   FiducialEditMode mode) {
    auto *action = toolBar->addAction(icon, tooltip);
    action->setToolTip(tooltip);
    action->setCheckable(true);
    modeGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, mode] {
      m_canvas->setActiveController(ActiveController::Fiducial);
      m_fiducialController->setMode(mode);
    });
    return action;
  };

  // --- Unified select tool: clicking a boundary selects/moves it,
  // clicking a seed selects it instead -- no need to pick which kind of
  // element you're about to click beforehand (see ImageCanvas's
  // resolveUnifiedSelection, which hit-tests to decide). ---
  auto *selectAction = toolBar->addAction(cursorIcon(), tr("Select / move / delete"));
  selectAction->setToolTip(
      tr("Select / move / delete a boundary or a seed point (click one directly)"));
  selectAction->setCheckable(true);
  selectAction->setChecked(true);
  modeGroup->addAction(selectAction);
  auto activateSelectTool = [this] {
    m_canvas->setActiveController(ActiveController::Boundary);
    m_controller->setMode(EditMode::Select);
    m_fringeController->setMode(FringeEditMode::Select);
    m_fiducialController->setMode(FiducialEditMode::Select);
  };
  connect(selectAction, &QAction::triggered, this, activateSelectTool);
  // The action starts pre-checked (it's the default tool), which does NOT
  // emit triggered() -- apply the mode sync once up front so unified
  // select actually works before the user has toggled to another tool
  // and back.
  activateSelectTool();
  toolBar->addSeparator();

  // Every Setup-toolbar tool is single-shot: once it has actually placed
  // or removed something, hand control back to Select instead of leaving
  // an Add-mode active for the next click. boundariesChanged()/
  // seedsChanged() fire exactly at that point (shape/seed added, moved,
  // or deleted -- see BoundaryEditController/FringeTracingController), so
  // hooking them here also covers move/delete for free; those already
  // only happen while Select is active, so switching to it again is a
  // harmless no-op.
  auto switchToSelectTool = [this, selectAction, activateSelectTool] {
    // Never hijack routing away from the fringe controller while a line
    // is being edited (double-click sub-mode, exited only via Escape --
    // see FringeTracingController) -- seedsChanged()/tracedLinesChanged()
    // also fire for every undo-stack push made while editing (inserting/
    // dragging/deleting a point), and activateSelectTool() would
    // otherwise switch ImageCanvas's active controller to Boundary mid-edit.
    if (m_fringeController->editingLineIndex())
      return;
    selectAction->setChecked(true);
    activateSelectTool();
  };
  connect(m_controller, &digitqt::gui::canvas::BoundaryEditController::boundariesChanged, this,
          switchToSelectTool);
  connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::seedsChanged, this,
          switchToSelectTool);
  connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::tracedLineAdded,
          this, switchToSelectTool);
  connect(m_fiducialController, &digitqt::gui::canvas::FiducialController::fiducialsChanged, this,
          switchToSelectTool);

  // --- Boundaries (aperture) ---
  addBoundaryModeAction(shapeIcon(/*ellipse=*/true, externalColor, Qt::SolidLine),
                        tr("Add external boundary (ellipse)"), EditMode::AddExternalEllipse);
  addBoundaryModeAction(shapeIcon(/*ellipse=*/false, externalColor, Qt::SolidLine),
                        tr("Add external boundary (rectangle)"), EditMode::AddExternalRectangle);
  addBoundaryModeAction(shapeIcon(/*ellipse=*/true, internalColor, Qt::DashLine),
                        tr("Add internal boundary (ellipse)"), EditMode::AddInternalEllipse);
  addBoundaryModeAction(shapeIcon(/*ellipse=*/false, internalColor, Qt::DashLine),
                        tr("Add internal boundary (rectangle)"), EditMode::AddInternalRectangle);
  toolBar->addSeparator();
  addBoundaryModeAction(digitqt::gui::icons::pointsEllipseIcon(externalColor),
                        tr("Add external boundary (ellipse by points: click around the "
                           "perimeter, double-click to finish)"),
                        EditMode::AddExternalEllipseByPoints);
  addBoundaryModeAction(digitqt::gui::icons::pointsEllipseIcon(internalColor),
                        tr("Add internal boundary (ellipse by points: click around the "
                           "perimeter, double-click to finish)"),
                        EditMode::AddInternalEllipseByPoints);

  auto *autoApertureAction = toolBar->addAction(
      digitqt::gui::icons::shapeIcon(/*ellipse=*/true, externalColor, Qt::DotLine),
      tr("Auto Detect Aperture"));
  autoApertureAction->setToolTip(
      tr("Automatically find the aperture from the image's fringe pattern and set it as "
         "the external boundary (replaces any existing one)"));
  connect(autoApertureAction, &QAction::triggered, this, [this] {
    m_controller->autoDetectAperture();
    if (!m_controller->lastError().isEmpty()) {
      QMessageBox::warning(this, tr("Auto Detect Aperture"), m_controller->lastError());
    }
  });
  toolBar->addSeparator();

  // --- Fringe tracing (seed points + run tracer) ---
  addFringeModeAction(seedIcon(), tr("Add seed point (click on a fringe)"),
                      FringeEditMode::AddSeed);
  addFringeModeAction(
      digitqt::gui::icons::lineByPointsIcon(),
      tr("Add fringe line manually (click along the fringe, double-click to finish)"),
      FringeEditMode::AddLineByPoints);

  auto *autoSeedAction =
      toolBar->addAction(digitqt::gui::icons::autoSeedIcon(), tr("Auto-place seeds"));
  autoSeedAction->setToolTip(
      tr("Automatically place seeds along one row at the fringes' intensity peaks"));
  connect(autoSeedAction, &QAction::triggered, this,
          [this] { m_fringeController->autoPlaceSeeds(); });

  auto *extendWidthAction = toolBar->addAction(
      style()->standardIcon(QStyle::SP_ToolBarHorizontalExtensionButton),
      tr("Extend fringes to aperture edge (width)"));
  extendWidthAction->setToolTip(
      tr("Synthesize fringe lines from the leftmost/rightmost traced line out to the "
         "aperture's left/right edge, spaced by the step to its nearest neighbor. Not a "
         "measurement -- see the dashed line style"));
  connect(extendWidthAction, &QAction::triggered, this, [this] {
    m_fringeController->extendFringesHorizontally();
    if (!m_fringeController->lastError().isEmpty())
      QMessageBox::warning(this, tr("Fringe Tracing"), m_fringeController->lastError());
  });

  auto *extendHeightAction = toolBar->addAction(
      style()->standardIcon(QStyle::SP_ToolBarVerticalExtensionButton),
      tr("Extend fringes to aperture edge (height)"));
  extendHeightAction->setToolTip(
      tr("Extend every traced line's endpoints toward the aperture's top/bottom edge, "
         "following each end's local step. Not a measurement -- see the dashed line style"));
  connect(extendHeightAction, &QAction::triggered, this, [this] {
    m_fringeController->extendFringesVertically();
    if (!m_fringeController->lastError().isEmpty())
      QMessageBox::warning(this, tr("Fringe Tracing"), m_fringeController->lastError());
  });

  auto *removeExtensionsAction = toolBar->addAction(
      style()->standardIcon(QStyle::SP_DialogResetButton), tr("Remove fringe extensions"));
  removeExtensionsAction->setToolTip(
      tr("Strip every line/point added by \"Extend fringes to aperture edge\" (width and "
         "height), restoring the traced lines to their pre-extension state"));
  connect(removeExtensionsAction, &QAction::triggered, this, [this] {
    m_fringeController->removeFringeExtensions();
    if (!m_fringeController->lastError().isEmpty())
      QMessageBox::warning(this, tr("Fringe Tracing"), m_fringeController->lastError());
  });

  toolBar->addSeparator();

  // --- Fiducials (реперы) ---
  addFiducialModeAction(digitqt::gui::icons::fiducialIcon(), tr("Add fiducial"),
                        FiducialEditMode::AddFiducial);

  auto *editFiducialRefsAction = toolBar->addAction(
      style()->standardIcon(QStyle::SP_FileDialogDetailedView),
      tr("Edit fiducial reference positions..."));
  editFiducialRefsAction->setToolTip(
      tr("Enter the known true position for each fiducial (e.g. from a calibration target)"));
  connect(editFiducialRefsAction, &QAction::triggered, this, [this] {
    digitqt::gui::FiducialReferenceDialog dialog(m_measurement.get(), m_undoStack, this);
    dialog.exec();
  });

  auto *fitRegistrationAction = toolBar->addAction(
      style()->standardIcon(QStyle::SP_DialogApplyButton), tr("Fit registration (G2)"));
  fitRegistrationAction->setToolTip(
      tr("Fit an affine transform (СКИ -> СКОС) from fiducials with a reference position, and "
         "report the fit quality"));
  connect(fitRegistrationAction, &QAction::triggered, this, [this] {
    auto fit = digitqt::core::fitFiducialTransform(m_measurement->fiducials().fiducials());
    m_measurement->setFiducialTransformFit(fit);
    if (!fit.ok) {
      QMessageBox::warning(this, tr("Fit Registration"), QString::fromStdString(fit.errorMessage));
      return;
    }
    QString report = tr("Used %1 fiducials\nRMS error: %2\nMax error: %3\n\n"
                        "Per-fiducial residuals:\n")
                         .arg(fit.usedCount)
                         .arg(fit.rmsError, 0, 'f', 4)
                         .arg(fit.maxError, 0, 'f', 4);
    for (const auto &r : fit.residuals)
      report += tr("  #%1: %2\n").arg(r.fiducialId).arg(r.residual, 0, 'f', 4);
    QMessageBox::information(this, tr("Fit Registration"), report);
  });

  toolBar->addSeparator();

  // --- Shared actions: Delete (whichever tool is active) and Trace ---
  auto *deleteAction =
      toolBar->addAction(style()->standardIcon(QStyle::SP_TrashIcon), tr("Delete selected"));
  deleteAction->setToolTip(tr("Delete the currently selected boundary or seed point"));
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this, [this] {
    if (m_canvas->activeController() == ActiveController::Boundary)
      m_controller->deleteSelection();
    else if (m_canvas->activeController() == ActiveController::FringeTracing)
      m_fringeController->deleteSelection();
    else
      m_fiducialController->deleteSelection();
  });

  auto *traceAction =
      toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Trace fringes"));
  traceAction->setToolTip(tr("Run the fringe tracer on all seed points"));
  connect(traceAction, &QAction::triggered, this, [this, switchToSelectTool] {
    runTracing();
    switchToSelectTool();
  });

  // --- Phase toolbar (S2: heatmap/isolines display + compute) ---
  auto *phaseToolBar = addToolBar(tr("Phase"));
  m_phaseToolBar = phaseToolBar;
  phaseToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  phaseToolBar->setIconSize(QSize(22, 22));
  phaseToolBar->setMovable(false);

  auto *heatmapAction =
      phaseToolBar->addAction(digitqt::gui::icons::heatmapIcon(), tr("Show heatmap"));
  heatmapAction->setCheckable(true);
  heatmapAction->setChecked(true);
  connect(heatmapAction, &QAction::toggled, m_phaseMapView,
          &digitqt::gui::canvas::PhaseMapView::setHeatmapVisible);
  connect(heatmapAction, &QAction::toggled, m_modalPhaseMapView,
          &digitqt::gui::canvas::PhaseMapView::setHeatmapVisible);

  auto *isolinesAction =
      phaseToolBar->addAction(digitqt::gui::icons::isolinesIcon(), tr("Show isolines"));
  isolinesAction->setCheckable(true);
  isolinesAction->setChecked(false);
  connect(isolinesAction, &QAction::toggled, m_phaseMapView,
          &digitqt::gui::canvas::PhaseMapView::setIsolinesVisible);
  connect(isolinesAction, &QAction::toggled, m_modalPhaseMapView,
          &digitqt::gui::canvas::PhaseMapView::setIsolinesVisible);

  phaseToolBar->addSeparator();

  auto *computePhaseAction =
      phaseToolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Compute phase"));
  computePhaseAction->setToolTip(tr("Reconstruct the phase map from the numbered fringe lines"));
  connect(computePhaseAction, &QAction::triggered, this, &MainWindow::computePhase);

  // --- Wavefront toolbar (S4: same heatmap/isolines toggles, own
  // Compute button -- see the shared QAction objects below, which keep
  // the toggle state in sync between both toolbars automatically) ---
  auto *wavefrontToolBar = addToolBar(tr("Wavefront"));
  m_wavefrontToolBar = wavefrontToolBar;
  wavefrontToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  wavefrontToolBar->setIconSize(QSize(22, 22));
  wavefrontToolBar->setMovable(false);

  wavefrontToolBar->addAction(heatmapAction);
  wavefrontToolBar->addAction(isolinesAction);
  wavefrontToolBar->addSeparator();

  auto *computeWavefrontAction = wavefrontToolBar->addAction(
      style()->standardIcon(QStyle::SP_MediaPlay), tr("Compute wavefront"));
  computeWavefrontAction->setToolTip(
      tr("Convert the phase map to physical wavefront height (needs S2 first)"));
  connect(computeWavefrontAction, &QAction::triggered, this, [this] { computeWavefront(); });

  // --- Modal Analysis toolbar (S5: same heatmap/isolines toggles as
  // S2/S4, controlling the 2D residual view next to the 3D one; plus
  // Compute -- the 3D view has its own built-in mouse rotate/zoom) ---
  auto *modalToolBar = addToolBar(tr("Modal Analysis"));
  m_modalToolBar = modalToolBar;
  modalToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  modalToolBar->setIconSize(QSize(22, 22));
  modalToolBar->setMovable(false);

  modalToolBar->addAction(heatmapAction);
  modalToolBar->addAction(isolinesAction);
  modalToolBar->addSeparator();

  auto *computeModalAction =
      modalToolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Fit aberrations"));
  computeModalAction->setToolTip(
      tr("Fit piston/tilt/defocus/astigmatism and subtract them (needs S4 first)"));
  connect(computeModalAction, &QAction::triggered, this, [this] { computeModalAnalysis(); });
}

void MainWindow::buildLanguageMenu() {
  auto *langMenu = menuBar()->addMenu(tr("&Language"));
  auto *group = new QActionGroup(this);
  group->setExclusive(true);

  QSettings settings;
  const QString current =
      settings.value(QStringLiteral("language"), QStringLiteral("en")).toString();

  auto addLanguage = [&](const QString &code, const QString &label) {
    auto *action = langMenu->addAction(label);
    action->setCheckable(true);
    action->setChecked(current == code);
    group->addAction(action);
    connect(action, &QAction::triggered, this, [this, code] {
      QSettings settings;
      settings.setValue(QStringLiteral("language"), code);
      QMessageBox::information(
          this, tr("Language changed"),
          tr("Please restart DigitQt for the language change to take effect."));
    });
  };

  addLanguage(QStringLiteral("en"), tr("English"));
  addLanguage(QStringLiteral("ru"), tr("Русский"));
}

void MainWindow::buildDocks() {
  m_pipelineDock = new PipelineTreeDock(this);
  addDockWidget(Qt::LeftDockWidgetArea, m_pipelineDock);
  connect(m_pipelineDock, &PipelineTreeDock::stageSelected, this, &MainWindow::onStageSelected);

  m_parametersDock = new ParametersDock(this);
  addDockWidget(Qt::RightDockWidgetArea, m_parametersDock);

  auto *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(m_pipelineDock->toggleViewAction());
  viewMenu->addAction(m_parametersDock->toggleViewAction());
}

void MainWindow::onStageSelected(StageId id) {
  const bool isSetup = (id == StageId::Setup);
  const bool isPhase = (id == StageId::S2);
  const bool isWavefront = (id == StageId::S4);
  const bool isModal = (id == StageId::S5);

  if (isSetup)
    m_centralStack->setCurrentIndex(0);
  else if (isPhase || isWavefront)
    m_centralStack->setCurrentIndex(1);
  else if (isModal)
    m_centralStack->setCurrentIndex(2);
  else
    m_centralStack->setCurrentIndex(3);

  if (!isSetup && !isPhase && !isWavefront && !isModal)
    m_notImplementedPage->setStage(id);

  if (isPhase)
    m_phaseMapView->setSource(digitqt::gui::canvas::PhaseMapView::Source::Phase);
  else if (isWavefront)
    m_phaseMapView->setSource(digitqt::gui::canvas::PhaseMapView::Source::Wavefront);

  m_setupToolBar->setVisible(isSetup);
  m_phaseToolBar->setVisible(isPhase);
  m_wavefrontToolBar->setVisible(isWavefront);
  m_modalToolBar->setVisible(isModal);

  m_parametersDock->setStage(id);
}

void MainWindow::runTracing() {
  runComputeInBackground(
      tr("Tracing fringes..."), [this] { return m_fringeController->runTracing(); },
      [this](bool ok) {
        if (!ok) {
          QMessageBox::warning(this, tr("Fringe Tracing"),
                               tr("Tracing failed:\n%1").arg(m_fringeController->lastError()));
        }
        updateStatusBar();
      });
}

void MainWindow::computePhase() {
  runComputeInBackground(
      tr("Reconstructing phase..."),
      [this] { return m_pipeline->stage(StageId::S2).compute(*m_measurement); },
      [this](bool ok) {
        if (!ok) {
          QMessageBox::warning(this, tr("Phase Reconstruction"),
                               tr("Phase reconstruction failed:\n%1")
                                   .arg(QString::fromStdString(
                                       m_pipeline->stage(StageId::S2).errorMessage())));
        } else if (m_measurement->customPhaseReconstructorPluginPath().empty() &&
                   m_measurement->phaseReconstructionAlgorithm() ==
                       digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting) {
          showPhaseShiftingReport();
        }
        m_phaseMapView->refresh();
        updateStatusBar();
      });
}

void MainWindow::computeWavefront(std::function<void()> then) {
  runComputeInBackground(
      tr("Computing wavefront..."),
      [this] { return m_pipeline->stage(StageId::S4).compute(*m_measurement); },
      [this, then](bool ok) {
        if (!ok) {
          QMessageBox::warning(this, tr("Wavefront Reconstruction"),
                               tr("Wavefront reconstruction failed:\n%1")
                                   .arg(QString::fromStdString(
                                       m_pipeline->stage(StageId::S4).errorMessage())));
        }
        m_phaseMapView->refresh();
        updateStatusBar();
        if (then)
          then();
      });
}

void MainWindow::computeModalAnalysis(std::function<void()> then) {
  runComputeInBackground(
      tr("Fitting aberrations..."),
      [this] { return m_pipeline->stage(StageId::S5).compute(*m_measurement); },
      [this, then](bool ok) {
        if (!ok) {
          QMessageBox::warning(this, tr("Modal Analysis"),
                               tr("Modal analysis failed:\n%1")
                                   .arg(QString::fromStdString(
                                       m_pipeline->stage(StageId::S5).errorMessage())));
        }
        m_surface3DView->refresh();
        m_modalPhaseMapView->refresh();
        updateStatusBar();
        if (then)
          then();
      });
}

void MainWindow::runComputeInBackground(const QString &busyText, std::function<bool()> compute,
                                        std::function<void(bool)> onDone) {
  setUiBusy(true, busyText);
  auto *watcher = new QFutureWatcher<bool>(this);
  connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, onDone] {
    const bool ok = watcher->result();
    watcher->deleteLater();
    setUiBusy(false);
    onDone(ok);
  });
  watcher->setFuture(QtConcurrent::run(std::move(compute)));
}

void MainWindow::setUiBusy(bool busy, const QString &text) {
  for (QWidget *w : m_disableWhileBusy)
    w->setEnabled(!busy);
  m_busyIndicator->setVisible(busy);
  if (busy)
    m_statusLabel->setText(text);
  else
    updateStatusBar();  // restore the normal status-bar text
}

void MainWindow::openImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open Interferogram Image"), QString(),
      tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*)"));
  if (path.isEmpty())
    return;

  const auto result = digitqt::io::loadImage(path);
  if (!result.ok()) {
    QMessageBox::warning(this, tr("Open Image"),
                         tr("Failed to load image:\n%1").arg(result.errorMessage));
    return;
  }
  m_measurement->setImage(digitqt::core::padImageBackground(result.image), path.toStdString());
  resetViewsToMeasurement();
  updateStatusBar();
}

namespace {

/// Пути кадров из файла-описания серии (.idf) -- в порядке, записанном в
/// файле, относительно его папки. declaredSize -- размер кадра из файла.
bool readIdfFramePaths(const QString &idfPath, QStringList &framePaths, QSize &declaredSize,
                       QString &error) {
  QFile file(idfPath);
  if (!file.open(QIODevice::ReadOnly)) {
    error = file.errorString();
    return false;
  }
  const QByteArray bytes = file.readAll();
  digitqt::formats::IdfDescription idf;
  std::string parseError;
  if (!digitqt::formats::parseIdf(std::string_view(bytes.constData(), bytes.size()), idf,
                                  parseError)) {
    error = QString::fromStdString(parseError);
    return false;
  }

  const QDir dir = QFileInfo(idfPath).absoluteDir();
  framePaths.clear();
  for (const std::string &name : idf.frameFiles) {
    // Программы, писавшие .idf, -- виндовые, имена в локальной кодировке.
    const QString fileName = QString::fromLocal8Bit(name.c_str());
    QString path = dir.filePath(fileName);
    if (!QFileInfo::exists(path)) {
      // Регистр в .idf бывает другой ("tilt-B_0.bmp" при Tilt-B_0.bmp):
      // на Windows это неважно, а фильтр имён QDir регистр не учитывает и
      // на других ОС.
      const QStringList matches = dir.entryList({fileName}, QDir::Files);
      if (matches.isEmpty()) {
        error = MainWindow::tr("Frame \"%1\" listed in the series file is not found in %2")
                    .arg(fileName, QDir::toNativeSeparators(dir.path()));
        return false;
      }
      path = dir.filePath(matches.front());
    }
    framePaths << path;
  }
  declaredSize = QSize(idf.width, idf.height);
  return true;
}

}  // namespace

void MainWindow::openPhaseShiftingSeries() {
  QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Open Phase-Shifting Series (a series file or all frames)"), QString(),
      tr("Series or frames (*.idf *.png *.jpg *.jpeg *.bmp *.tif *.tiff);;"
         "Interferogram series description (*.idf);;"
         "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*)"));
  if (paths.isEmpty())
    return;

  QSize declaredSize;
  if (paths.size() == 1 && QFileInfo(paths.front()).suffix().compare(
                               QStringLiteral("idf"), Qt::CaseInsensitive) == 0) {
    // Порядок кадров задан самим файлом -- не сортируем.
    QString error;
    const QString idfPath = paths.front();
    if (!readIdfFramePaths(idfPath, paths, declaredSize, error)) {
      QMessageBox::warning(this, tr("Open Phase-Shifting Series"),
                           tr("Cannot read %1:\n%2").arg(QFileInfo(idfPath).fileName(), error));
      return;
    }
  } else {
    // Порядок кадров -- это порядок сдвигов. Диалог отдаёт файлы в порядке
    // выделения, поэтому сортируем по имени, "frame2" раньше "frame10".
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(paths.begin(), paths.end(), [&collator](const QString &a, const QString &b) {
      return collator.compare(a, b) < 0;
    });
  }
  if (paths.size() < 3) {
    QMessageBox::warning(this, tr("Open Phase-Shifting Series"),
                         tr("Select at least 3 frames -- phase shifting has 3 unknowns per "
                            "point (background, fringe amplitude, phase)."));
    return;
  }

  std::vector<digitqt::core::Bitmap> frames;
  for (const QString &path : paths) {
    const auto result = digitqt::io::loadImage(path);
    if (!result.ok()) {
      QMessageBox::warning(this, tr("Open Phase-Shifting Series"),
                           tr("Failed to load %1:\n%2").arg(path, result.errorMessage));
      return;
    }
    if (!frames.empty() && (result.image.width() != frames.front().width() ||
                            result.image.height() != frames.front().height())) {
      QMessageBox::warning(this, tr("Open Phase-Shifting Series"),
                           tr("All frames must have the same size: %1 differs from the first one.")
                               .arg(QFileInfo(path).fileName()));
      return;
    }
    frames.push_back(result.image);
  }
  if (declaredSize.isValid() && (frames.front().width() != declaredSize.width() ||
                                 frames.front().height() != declaredSize.height())) {
    QMessageBox::warning(this, tr("Open Phase-Shifting Series"),
                         tr("The frames are %1x%2, but the series file declares %3x%4.")
                             .arg(frames.front().width())
                             .arg(frames.front().height())
                             .arg(declaredSize.width())
                             .arg(declaredSize.height()));
    return;
  }
  // Поле вокруг изображения -- как у openImage(), у всех кадров одинаковое
  // (зависит только от размера), так что апертура подходит ко всем.
  const int sourceWidth = frames.front().width(), sourceHeight = frames.front().height();
  for (auto &frame : frames)
    frame = digitqt::core::padImageBackground(frame);

  m_measurement->setPhaseShiftingFrames(std::move(frames), paths.front().toStdString(),
                                        sourceWidth, sourceHeight);
  m_measurement->setCustomPhaseReconstructorPluginPath({});
  m_measurement->setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);
  resetViewsToMeasurement();
  updateStatusBar();
  statusBar()->showMessage(
      tr("Loaded %1 phase-shifting frames; the first one is shown. Draw the aperture, then "
         "run Phase Reconstruction.")
          .arg(paths.size()));
}

void MainWindow::resetViewsToMeasurement() {
  m_undoStack->clear();
  m_controller->setMeasurement(m_measurement.get());
  m_fringeController->setMeasurement(m_measurement.get());
  m_fiducialController->setMeasurement(m_measurement.get());
  m_canvas->setMeasurement(m_measurement.get());
  m_phaseMapView->setMeasurement(m_measurement.get());
  m_surface3DView->setMeasurement(m_measurement.get());
  m_modalPhaseMapView->setMeasurement(m_measurement.get());
}

void MainWindow::importFile() {
  namespace formats = digitqt::formats;

  QStringList filters, allPatterns;
  for (const formats::IFormatAdapter *adapter : formats::importAdapters()) {
    QStringList patterns;
    for (const std::string &extension : adapter->extensions())
      patterns << QStringLiteral("*.") + QString::fromStdString(extension);
    allPatterns << patterns;
    filters << QStringLiteral("%1 (%2)").arg(QString::fromStdString(adapter->displayName()),
                                             patterns.join(QLatin1Char(' ')));
  }
  filters.prepend(tr("All supported files (%1)").arg(allPatterns.join(QLatin1Char(' '))));
  filters << tr("All files (*)");

  const QString path = QFileDialog::getOpenFileName(this, tr("Import"), QString(),
                                                     filters.join(QStringLiteral(";;")));
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("Import"), tr("Could not open file for reading:\n%1").arg(path));
    return;
  }
  const QByteArray bytes = file.readAll();
  const std::string_view data(bytes.constData(), static_cast<size_t>(bytes.size()));

  // The extension only chose which files the dialog listed -- what the
  // file actually is gets decided by its content.
  const formats::IFormatAdapter *adapter =
      formats::detectFormat(data.substr(0, formats::kProbeBytes));
  if (!adapter) {
    QMessageBox::warning(this, tr("Import"), tr("Unrecognized file format:\n%1").arg(path));
    return;
  }

  formats::ImportedData imported;
  std::string error;
  if (!adapter->importData(data, imported, error)) {
    QMessageBox::warning(this, tr("Import"),
                         tr("Import failed:\n%1").arg(QString::fromStdString(error)));
    return;
  }

  // What to do with the result follows from which fields the adapter
  // filled, not from which format the file was.
  if (imported.phaseMap) {
    applyImportedPhaseMap(std::move(*imported.phaseMap));
  } else if (imported.boundaries || !imported.fringeLines.empty()) {
    applyImportedFringes(imported, path);
  } else {
    QMessageBox::warning(this, tr("Import"), tr("The file contains nothing this program can use."));
    return;
  }

  QStringList reportLines;
  for (const formats::FidelityNote &note : imported.report) {
    QString kind;
    switch (note.kind) {
      case formats::Fidelity::Lossless:
        kind = tr("As is");
        break;
      case formats::Fidelity::Converted:
        kind = tr("Converted");
        break;
      case formats::Fidelity::Dropped:
        kind = tr("Dropped");
        break;
      case formats::Fidelity::Missing:
        kind = tr("Missing");
        break;
    }
    reportLines << QStringLiteral("%1: %2").arg(kind, QString::fromStdString(note.what));
  }
  QMessageBox::information(this, tr("Import"),
                           tr("Imported %1 as %2.\n\n%3")
                               .arg(QFileInfo(path).fileName(),
                                    QString::fromStdString(adapter->displayName()),
                                    reportLines.join(QLatin1Char('\n'))));
}

void MainWindow::applyImportedPhaseMap(digitqt::core::PhaseMap phase) {
  // A ready phase map has no Setup/S1 behind it (no image, no boundaries,
  // no fringe tracing) -- it drops straight into S2's slot. S4 and S5 only
  // need phaseMap()/wavefrontMap() (see WavefrontReconstructionStage and
  // ModalAnalysisStage, which derives the aperture from the map's own
  // bounding box rather than Measurement::boundaries()), so running them
  // right away fills the whole rest of the pipeline in one step.
  m_measurement->setImportedPhaseMap(std::move(phase));
  resetViewsToMeasurement();

  m_pipeline->stage(StageId::S2).markComputed();
  // Chained, not fired together: each stage now runs on a background
  // thread (see runComputeInBackground()), so computeModalAnalysis()
  // must not start until computeWavefront()'s worker has actually
  // finished writing wavefrontMap() -- firing both at once would let
  // S5 read it while S4 is still computing it.
  computeWavefront([this] {
    computeModalAnalysis([this] {
      m_pipelineDock->selectStage(StageId::S5);
      updateStatusBar();
    });
  });
}

void MainWindow::applyImportedFringes(digitqt::formats::ImportedData &data,
                                      const QString &sourcePath) {
  // The pipeline needs the image only for its size (the phase grid) and
  // the canvas for a backdrop -- shapes and fringes are complete without
  // it. So the image the file names is used if it happens to lie next to
  // the file, and is not asked for otherwise: usually it isn't there.
  const QString imageName = QString::fromStdString(data.imageFileName);
  const QString imagePath =
      imageName.isEmpty() ? QString() : QFileInfo(sourcePath).dir().filePath(imageName);
  const auto loaded = QFileInfo::exists(imagePath) ? digitqt::io::loadImage(imagePath)
                                                   : digitqt::io::ImageLoadResult{};

  double dx = 0.0, dy = 0.0;
  if (loaded.ok()) {
    digitqt::core::Bitmap padded = digitqt::core::padImageBackground(loaded.image);
    // The model works on the padded image (see openImage()), so coordinates
    // relative to the original image move by the padding margin. A file
    // written by our own FrnExporter already refers to the padded size.
    if (data.imageWidth != padded.width() || data.imageHeight != padded.height()) {
      dx = (padded.width() - loaded.image.width()) / 2.0;
      dy = (padded.height() - loaded.image.height()) / 2.0;
      if (data.imageWidth != loaded.image.width() || data.imageHeight != loaded.image.height())
        data.report.push_back(
            {digitqt::formats::Fidelity::Missing,
             "Image size in the file (" + std::to_string(data.imageWidth) + "x" +
                 std::to_string(data.imageHeight) + ") does not match " + imageName.toStdString() +
                 " (" + std::to_string(loaded.image.width()) + "x" +
                 std::to_string(loaded.image.height()) + "); coordinates may be misplaced"});
    }
    m_measurement->setImage(std::move(padded), imagePath.toStdString());
  } else {
    // No image: a blank canvas of the size the file states, or -- if it
    // states none -- just large enough to hold everything imported.
    int width = data.imageWidth, height = data.imageHeight;
    if (width <= 0 || height <= 0) {
      double maxX = 0.0, maxY = 0.0;
      if (data.boundaries) {
        const auto bounds = data.boundaries->getCombinedBounds();
        maxX = bounds.maxX();
        maxY = bounds.maxY();
      }
      for (const auto &line : data.fringeLines) {
        for (const auto &point : line.points) {
          maxX = std::max(maxX, point.x);
          maxY = std::max(maxY, point.y);
        }
      }
      width = static_cast<int>(std::ceil(maxX)) + 1;
      height = static_cast<int>(std::ceil(maxY)) + 1;
    }
    m_measurement->setPlaceholderImage(width, height);
    data.report.push_back(
        {digitqt::formats::Fidelity::Missing,
         (imageName.isEmpty() ? std::string("No image is named in the file")
                              : "Image " + imageName.toStdString() + " was not found next to the file") +
             "; a blank " + std::to_string(width) + "x" + std::to_string(height) +
             " canvas is used instead"});
  }

  if (data.boundaries) {
    for (const auto *shapes : {&data.boundaries->getExternal(), &data.boundaries->getInternal(),
                               &data.boundaries->getApertures()}) {
      for (const auto &shape : *shapes) {
        shape->shiftX(dx);
        shape->shiftY(dy);
      }
    }
    m_measurement->boundaries() = std::move(*data.boundaries);
    m_measurement->boundaries().notifyShapeModified();
  }
  for (auto &line : data.fringeLines) {
    for (auto &point : line.points) {
      point.x += dx;
      point.y += dy;
    }
  }
  m_measurement->fringeTracing().tracedLines() = std::move(data.fringeLines);

  resetViewsToMeasurement();
  // Numbered fringes are what Setup produces; everything after it has to
  // be computed from them.
  m_pipeline->invalidateFrom(StageId::S2);
  if (!m_measurement->fringeTracing().tracedLines().empty())
    m_pipeline->stage(StageId::Setup).markComputed();
  m_pipelineDock->selectStage(StageId::Setup);
  updateStatusBar();
}

void MainWindow::showPhaseShiftingReport() {
  // Пороги предупреждений подобраны на сериях из notes/IDF и синтетике:
  // у синтетики расхождение половин 0.0 градуса и невязка до 2% амплитуды
  // (с шумом), у серий с одной-двумя полосами или наклоном -- 30-90
  // градусов и 15-50%.
  constexpr double kMaxHalvesDisagreementDeg = 10.0;
  constexpr double kMaxRelativeResidual = 0.1;

  const auto &report = m_measurement->phaseShifting().report();
  QStringList shifts;
  for (const double shift : report.shiftsDeg)
    shifts << QString::number(shift, 'f', 2);
  const QString halves = std::isnan(report.halvesDisagreementDeg)
                             ? tr("n/a")
                             : QString::number(report.halvesDisagreementDeg, 'f', 1);
  QString tilts;
  if (!report.tiltXWaves.empty()) {
    QStringList perFrame;
    for (size_t n = 0; n < report.tiltXWaves.size(); ++n)
      perFrame << QStringLiteral("%1/%2")
                      .arg(report.tiltXWaves[n], 0, 'f', 2)
                      .arg(report.tiltYWaves[n], 0, 'f', 2);
    tilts = tr("; tilt x/y %1 waves across the frame").arg(perFrame.join(QStringLiteral("  ")));
  }
  const QString shiftSource = report.refined
                                  ? tr("estimated shifts %1 deg, %2 iteration(s)")
                                        .arg(shifts.join(QStringLiteral(" / ")))
                                        .arg(report.iterations)
                                  : tr("nominal shifts %1 deg (not refined)")
                                        .arg(shifts.join(QStringLiteral(" / ")));
  statusBar()->showMessage(
      tr("PSI: %1; halves of the field disagree by %2 deg; "
         "fit residual %3 of fringe amplitude %4 grey levels (%5 points)%6")
          .arg(shiftSource)
          .arg(halves)
          .arg(report.residual, 0, 'f', 2)
          .arg(report.amplitude, 0, 'f', 1)
          .arg(report.shiftPoints)
          .arg(tilts));

  QStringList warnings;
  if (!report.converged)
    warnings << tr("The phase shifts did not settle within the iteration limit; the map may "
                   "still carry shift errors (step-like ripple across the fringes).");
  if (std::isnan(report.halvesDisagreementDeg) ||
      report.halvesDisagreementDeg > kMaxHalvesDisagreementDeg)
    warnings << tr("The shifts found from two halves of the field disagree by %1 deg, so they are "
                   "not reliable. Either there are too few fringes to determine the shifts (fewer "
                   "than one or two across the area -- add tilt), or the shift differs across the "
                   "field (tilt between frames).")
                        .arg(halves) +
                    (report.tiltXWaves.empty()
                         ? tr(" If the fringes are plentiful, try \"Compensate tilt between "
                              "frames\" on this stage.")
                         : QString());
  if (report.amplitude > 0.0 && report.residual > kMaxRelativeResidual * report.amplitude)
    warnings << tr("The frames deviate from the ideal fringe model by %1 grey levels, %2% of the "
                   "fringe amplitude. Typical causes: camera noise, overexposure (fringes clipped "
                   "near white are no longer sinusoidal), laser flicker between frames, tilt or "
                   "vibration.")
                        .arg(report.residual, 0, 'f', 1)
                        .arg(100.0 * report.residual / report.amplitude, 0, 'f', 0) +
                    (report.refined ? QString()
                                    : tr(" The shifts were not refined: if the real shifts "
                                         "differ from the nominal step, this alone explains "
                                         "the deviation -- turn on \"Refine phase shifts from "
                                         "the frames\"."));
  if (report.mirrored)
    warnings << tr("The estimated shifts ended up closer to the mirror image of the nominal "
                   "step. The map was flipped to match the nominal direction -- check its sign "
                   "if the nominal step is not reliable.");
  if (report.regionCount > 1)
    warnings << tr("The valid area falls apart into %1 disconnected regions; their relative "
                   "fringe orders are unknown, so steps between them are not real.")
                    .arg(report.regionCount);
  if (!warnings.isEmpty())
    QMessageBox::warning(this, tr("Phase Shifting"), warnings.join(QStringLiteral("\n\n")));
}

void MainWindow::updateStatusBar() {
  const auto &b = m_measurement->boundaries();
  const auto &tracingData = m_measurement->fringeTracing();
  m_statusLabel->setText(tr("External: %1   Internal: %2   Seeds: %3   Traced: %4")
                             .arg(b.getExternal().size())
                             .arg(b.getInternal().size())
                             .arg(tracingData.seeds().size())
                             .arg(tracingData.tracedLines().size()));
  m_pipelineDock->refreshStatuses();
  m_parametersDock->refresh();
}

}  // namespace digitqt::gui
