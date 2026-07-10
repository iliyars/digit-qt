#include "MainWindow.h"

#include "gui/IconFactory.h"
#include "gui/NotImplementedPage.h"
#include "gui/ParametersDock.h"
#include "gui/PipelineTreeDock.h"
#include "io/ImageLoader.h"

#include <QActionGroup>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QUndoStack>

namespace digitqt::gui {

using digitqt::core::pipeline::StageId;
using digitqt::gui::canvas::ActiveController;
using digitqt::gui::canvas::EditMode;
using digitqt::gui::canvas::FringeEditMode;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_measurement(std::make_unique<digitqt::core::Measurement>()),
      m_pipeline(std::make_unique<digitqt::core::pipeline::Pipeline>()),
      m_undoStack(new QUndoStack(this)),
      m_controller(new digitqt::gui::canvas::BoundaryEditController(m_undoStack, this)),
      m_fringeController(new digitqt::gui::canvas::FringeTracingController(m_undoStack, this)) {
  setWindowTitle(tr("DigitQt — Interferogram Processing"));
  resize(1400, 900);

  // --- Central view: one page for image-based stages (Setup, S1),
  // a shared placeholder page for everything else. ---
  m_centralStack = new QStackedWidget(this);
  m_canvas = new digitqt::gui::canvas::ImageCanvas(m_controller, m_fringeController, this);
  m_notImplementedPage = new NotImplementedPage(this);
  m_centralStack->addWidget(m_canvas);              // index 0: Setup / S1
  m_centralStack->addWidget(m_notImplementedPage);  // index 1: everything else
  setCentralWidget(m_centralStack);

  m_statusLabel = new QLabel(this);
  statusBar()->addPermanentWidget(m_statusLabel);

  connect(m_controller, &digitqt::gui::canvas::BoundaryEditController::boundariesChanged, this,
          &MainWindow::updateStatusBar);
  connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::seedsChanged, this,
          &MainWindow::updateStatusBar);
  connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::tracedLinesChanged,
          this, &MainWindow::updateStatusBar);

  buildMenusAndToolbars();
  buildLanguageMenu();
  buildDocks();

  m_controller->setMeasurement(m_measurement.get());
  m_fringeController->setMeasurement(m_measurement.get());
  m_fringeController->setPipeline(m_pipeline.get());
  m_canvas->setMeasurement(m_measurement.get());
  m_pipelineDock->setPipeline(m_pipeline.get());
  m_parametersDock->setMeasurement(m_measurement.get());
  m_parametersDock->setFringeController(m_fringeController);
  onStageSelected(StageId::Setup);
  updateStatusBar();
}

void MainWindow::buildMenusAndToolbars() {
  // --- File menu ---
  auto *fileMenu = menuBar()->addMenu(tr("&File"));
  auto *openAction = fileMenu->addAction(tr("&Open Image..."), this, &MainWindow::openImage);
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

  // --- Boundaries (aperture) ---
  addBoundaryModeAction(cursorIcon(), tr("Select / move / delete a boundary"), EditMode::Select,
                        true);
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
  toolBar->addSeparator();

  // --- Fringe tracing (seed points + run tracer) ---
  addFringeModeAction(seedIcon(), tr("Add seed point (click on a fringe)"),
                      FringeEditMode::AddSeed);
  addFringeModeAction(cursorIcon(), tr("Select / delete a seed point"), FringeEditMode::Select);

  auto *autoSeedAction =
      toolBar->addAction(digitqt::gui::icons::autoSeedIcon(), tr("Auto-place seeds"));
  autoSeedAction->setToolTip(
      tr("Automatically place seeds along one row at the fringes' intensity peaks"));
  connect(autoSeedAction, &QAction::triggered, this,
          [this] { m_fringeController->autoPlaceSeeds(); });

  toolBar->addSeparator();

  // --- Shared actions: Delete (whichever tool is active) and Trace ---
  auto *deleteAction =
      toolBar->addAction(style()->standardIcon(QStyle::SP_TrashIcon), tr("Delete selected"));
  deleteAction->setToolTip(tr("Delete the currently selected boundary or seed point"));
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this, [this] {
    if (m_canvas->activeController() == ActiveController::Boundary)
      m_controller->deleteSelection();
    else
      m_fringeController->deleteSelection();
  });

  auto *traceAction =
      toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Trace fringes"));
  traceAction->setToolTip(tr("Run the fringe tracer on all seed points"));
  connect(traceAction, &QAction::triggered, this, &MainWindow::runTracing);
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

  m_centralStack->setCurrentIndex(isSetup ? 0 : 1);
  if (!isSetup)
    m_notImplementedPage->setStage(id);

  m_setupToolBar->setEnabled(isSetup);

  m_parametersDock->setStage(id);
}

void MainWindow::runTracing() {
  if (!m_fringeController->runTracing()) {
    QMessageBox::warning(this, tr("Fringe Tracing"),
                         tr("Tracing failed:\n%1").arg(m_fringeController->lastError()));
  }
  updateStatusBar();
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
  m_measurement->setImage(result.image, path);
  m_undoStack->clear();
  m_controller->setMeasurement(m_measurement.get());
  m_fringeController->setMeasurement(m_measurement.get());
  m_canvas->setMeasurement(m_measurement.get());
  updateStatusBar();
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
