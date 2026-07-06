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
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QUndoStack>

namespace digitqt::gui {

using digitqt::core::pipeline::StageId;
using digitqt::gui::canvas::EditMode;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_measurement(std::make_unique<digitqt::core::Measurement>()),
      m_pipeline(std::make_unique<digitqt::core::pipeline::Pipeline>()),
      m_undoStack(new QUndoStack(this)),
      m_controller(
          new digitqt::gui::canvas::BoundaryEditController(m_undoStack, this)) {
  setWindowTitle(tr("DigitQt — Interferogram Processing"));
  resize(1200, 800);

  // --- Central view: one page per implemented stage, a shared
  // placeholder page for everything else. ---
  m_centralStack = new QStackedWidget(this);
  m_canvas = new digitqt::gui::canvas::ImageCanvas(m_controller, this);
  m_notImplementedPage = new NotImplementedPage(this);
  m_centralStack->addWidget(m_canvas);             // index 0: S0 / S0a
  m_centralStack->addWidget(m_notImplementedPage); // index 1: everything else
  setCentralWidget(m_centralStack);

  m_statusLabel = new QLabel(this);
  statusBar()->addPermanentWidget(m_statusLabel);

  connect(m_controller,
          &digitqt::gui::canvas::BoundaryEditController::boundariesChanged,
          this, &MainWindow::updateStatusBar);

  buildMenusAndToolbars();
  buildDocks();

  m_controller->setMeasurement(m_measurement.get());
  m_canvas->setMeasurement(m_measurement.get());
  m_pipelineDock->setPipeline(m_pipeline.get());
  m_parametersDock->setMeasurement(m_measurement.get());
  onStageSelected(StageId::S0a);
  updateStatusBar();
}

void MainWindow::buildMenusAndToolbars() {
  // --- File menu ---
  auto *fileMenu = menuBar()->addMenu(tr("&File"));
  auto *openAction =
      fileMenu->addAction(tr("&Open Image..."), this, &MainWindow::openImage);
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

  // --- Boundary toolbar (S0a: external/internal boundary editing) ---
  auto *toolBar = addToolBar(tr("Boundaries"));
  toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  toolBar->setIconSize(QSize(22, 22));
  toolBar->setMovable(false);

  auto *modeGroup = new QActionGroup(this);
  modeGroup->setExclusive(true);

  auto addModeAction = [&](const QIcon &icon, const QString &tooltip,
                           EditMode mode, bool checked = false) {
    auto *action = toolBar->addAction(icon, tooltip);
    action->setToolTip(tooltip);
    action->setCheckable(true);
    action->setChecked(checked);
    modeGroup->addAction(action);
    connect(action, &QAction::triggered, this,
            [this, mode] { m_controller->setMode(mode); });
    return action;
  };
  using digitqt::gui::icons::cursorIcon;
  using digitqt::gui::icons::shapeIcon;

  const QColor externalColor(0, 190, 60);
  const QColor internalColor(220, 40, 40);

  addModeAction(cursorIcon(), tr("Select / move / delete"), EditMode::Select,
                true);
  toolBar->addSeparator();
  addModeAction(shapeIcon(/*ellipse=*/true, externalColor, Qt::SolidLine),
                tr("Add external boundary (ellipse)"),
                EditMode::AddExternalEllipse);
  addModeAction(shapeIcon(/*ellipse=*/false, externalColor, Qt::SolidLine),
                tr("Add external boundary (rectangle)"),
                EditMode::AddExternalRectangle);
  toolBar->addSeparator();
  addModeAction(shapeIcon(/*ellipse=*/true, internalColor, Qt::DashLine),
                tr("Add internal boundary (ellipse)"),
                EditMode::AddInternalEllipse);
  addModeAction(shapeIcon(/*ellipse=*/false, internalColor, Qt::DashLine),
                tr("Add internal boundary (rectangle)"),
                EditMode::AddInternalRectangle);
  toolBar->addSeparator();

  auto *deleteAction = toolBar->addAction(
      style()->standardIcon(QStyle::SP_TrashIcon), tr("Delete selected"));
  deleteAction->setToolTip(tr("Delete selected boundary"));
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, m_controller,
          &digitqt::gui::canvas::BoundaryEditController::deleteSelection);
}

void MainWindow::buildDocks() {
  m_pipelineDock = new PipelineTreeDock(this);
  addDockWidget(Qt::LeftDockWidgetArea, m_pipelineDock);
  connect(m_pipelineDock, &PipelineTreeDock::stageSelected, this,
          &MainWindow::onStageSelected);

  m_parametersDock = new ParametersDock(this);
  addDockWidget(Qt::RightDockWidgetArea, m_parametersDock);

  auto *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(m_pipelineDock->toggleViewAction());
  viewMenu->addAction(m_parametersDock->toggleViewAction());
}

void MainWindow::onStageSelected(StageId id) {
  m_centralStack->setCurrentIndex(id == StageId::S0a ? 0 : 1);
  if (id != StageId::S0a)
    m_notImplementedPage->setStage(id);
  m_parametersDock->setStage(id);
}

void MainWindow::openImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open Interferogram Image"), QString(),
      tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*)"));
  if (path.isEmpty())
    return;

  QString error;
  const auto result = digitqt::io::loadImage(path);
  if (!result.ok()) {
    QMessageBox::warning(
        this, tr("Open Image"),
        tr("Failed to load image:\n%1").arg(result.errorMessage));
    return;
  }
  m_measurement->setImage(result.image, path);
  m_undoStack->clear();
  m_controller->setMeasurement(m_measurement.get());
  m_canvas->setMeasurement(m_measurement.get());
  updateStatusBar();
}

void MainWindow::updateStatusBar() {
  const auto &b = m_measurement->boundaries();
  m_statusLabel->setText(tr("External: %1   Internal: %2")
                             .arg(b.getExternal().size())
                             .arg(b.getInternal().size()));
}

} // namespace digitqt::gui
