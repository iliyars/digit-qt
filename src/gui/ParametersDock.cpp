#include "ParametersDock.h"

#include "canvas/FringeTracingController.h"
#include "core/Measurement.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>

namespace digitqt::gui {

using digitqt::core::FringeCenterMode;
using digitqt::core::TracerAlgorithm;
using digitqt::core::pipeline::StageId;

ParametersDock::ParametersDock(QWidget *parent)
    : QDockWidget(tr("Parameters"), parent),
      m_algorithmCombo(new QComboBox(this)),
      m_fringeCenterCombo(new QComboBox(this)),
      m_orderSpin(new QDoubleSpinBox(this)),
      m_label(new QLabel(this)) {
  setObjectName("ParametersDock");

  m_algorithmCombo->addItem(tr("Sequential Fringe Tracking (FTM)"),
                            static_cast<int>(TracerAlgorithm::SequentialTracking));
  m_algorithmCombo->addItem(tr("Ridge Tracking (Structure Tensor)"),
                            static_cast<int>(TracerAlgorithm::StructureTensor));
  m_algorithmCombo->addItem(tr("Scanline Extremum Method (FTM)"),
                            static_cast<int>(TracerAlgorithm::ScanlineExtremum));
  m_algorithmCombo->addItem(tr("Binary Thinning Method (FBM)"),
                            static_cast<int>(TracerAlgorithm::BinaryThinning));
  connect(m_algorithmCombo, &QComboBox::currentIndexChanged, this,
          &ParametersDock::onAlgorithmChanged);

  m_fringeCenterCombo->addItem(tr("Bright fringes only (Max)"),
                               static_cast<int>(FringeCenterMode::Max));
  m_fringeCenterCombo->addItem(tr("Dark fringes only (Min)"),
                               static_cast<int>(FringeCenterMode::Min));
  m_fringeCenterCombo->addItem(tr("Both, alternating (MinMax)"),
                               static_cast<int>(FringeCenterMode::MinMax));
  m_fringeCenterCombo->setToolTip(tr("Only used by Scanline Extremum Method"));
  connect(m_fringeCenterCombo, &QComboBox::currentIndexChanged, this,
          &ParametersDock::onFringeCenterModeChanged);

  m_orderSpin->setRange(-999.0, 999.0);
  m_orderSpin->setDecimals(2);
  connect(m_orderSpin, &QDoubleSpinBox::valueChanged, this,
          &ParametersDock::onFringeOrderSpinChanged);

  m_label->setWordWrap(true);
  m_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_label->setContentsMargins(8, 8, 8, 8);
  m_label->setTextFormat(Qt::RichText);

  auto *container = new QWidget(this);
  auto *layout = new QVBoxLayout(container);

  auto *algorithmLabel = new QLabel(tr("<b>Fringe tracing algorithm</b>"), container);
  algorithmLabel->setContentsMargins(8, 8, 8, 0);
  layout->addWidget(algorithmLabel);
  layout->addWidget(m_algorithmCombo);

  auto *fringeCenterLabel = new QLabel(tr("<b>Fringe center</b>"), container);
  fringeCenterLabel->setContentsMargins(8, 8, 8, 0);
  layout->addWidget(fringeCenterLabel);
  layout->addWidget(m_fringeCenterCombo);

  m_orderEditorRow = new QWidget(container);
  auto *orderLayout = new QVBoxLayout(m_orderEditorRow);
  orderLayout->setContentsMargins(0, 0, 0, 0);
  auto *orderLabel =
      new QLabel(tr("<b>Fringe order (double-click a line to edit)</b>"), m_orderEditorRow);
  orderLabel->setContentsMargins(8, 8, 8, 0);
  orderLayout->addWidget(orderLabel);
  orderLayout->addWidget(m_orderSpin);
  layout->addWidget(m_orderEditorRow);
  m_orderEditorRow->setVisible(false);

  layout->addWidget(m_label);
  layout->addStretch();
  setWidget(container);
}

void ParametersDock::setMeasurement(digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  if (m_measurement) {
    const auto &tracingData = m_measurement->fringeTracing();

    const int algoIndex = m_algorithmCombo->findData(static_cast<int>(tracingData.algorithm()));
    if (algoIndex >= 0) {
      const QSignalBlocker blocker(m_algorithmCombo);
      m_algorithmCombo->setCurrentIndex(algoIndex);
    }

    const int centerIndex =
        m_fringeCenterCombo->findData(static_cast<int>(tracingData.fringeCenterMode()));
    if (centerIndex >= 0) {
      const QSignalBlocker blocker(m_fringeCenterCombo);
      m_fringeCenterCombo->setCurrentIndex(centerIndex);
    }

    m_fringeCenterCombo->setEnabled(tracingData.algorithm() == TracerAlgorithm::ScanlineExtremum);
  }
  refreshOrderEditor();
  refresh();
}

void ParametersDock::setFringeController(
    digitqt::gui::canvas::FringeTracingController *controller) {
  m_fringeController = controller;
  if (m_fringeController) {
    connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::lineEditModeChanged,
            this, &ParametersDock::refreshOrderEditor);
    connect(m_fringeController, &digitqt::gui::canvas::FringeTracingController::tracedLinesChanged,
            this, &ParametersDock::refreshOrderEditor);
  }
  refreshOrderEditor();
}

void ParametersDock::setStage(StageId id) {
  m_currentStage = id;
  refresh();
}

void ParametersDock::onAlgorithmChanged(int /*index*/) {
  if (!m_measurement)
    return;
  const auto algorithm = static_cast<TracerAlgorithm>(m_algorithmCombo->currentData().toInt());
  m_measurement->fringeTracing().setAlgorithm(algorithm);
  m_fringeCenterCombo->setEnabled(algorithm == TracerAlgorithm::ScanlineExtremum);
}

void ParametersDock::onFringeCenterModeChanged(int /*index*/) {
  if (!m_measurement)
    return;
  const auto mode = static_cast<FringeCenterMode>(m_fringeCenterCombo->currentData().toInt());
  m_measurement->fringeTracing().setFringeCenterMode(mode);
}

void ParametersDock::onFringeOrderSpinChanged(double value) {
  if (!m_fringeController)
    return;
  const auto editingIndex = m_fringeController->editingLineIndex();
  if (!editingIndex)
    return;
  m_fringeController->setLineOrder(*editingIndex, value);
}

void ParametersDock::refreshOrderEditor() {
  if (!m_fringeController || !m_measurement) {
    m_orderEditorRow->setVisible(false);
    return;
  }

  const auto editingIndex = m_fringeController->editingLineIndex();
  const auto &lines = m_measurement->fringeTracing().tracedLines();

  if (!editingIndex || *editingIndex >= lines.size()) {
    m_orderEditorRow->setVisible(false);
    return;
  }

  m_orderEditorRow->setVisible(true);
  const QSignalBlocker blocker(m_orderSpin);
  m_orderSpin->setValue(lines[*editingIndex].order);
}

void ParametersDock::refresh() {
  using digitqt::core::pipeline::displayName;
  using digitqt::core::pipeline::shortName;

  QString text =
      tr("<b>%1</b> (%2)<br><br>").arg(displayName(m_currentStage), shortName(m_currentStage));

  if (m_currentStage == StageId::Setup && m_measurement) {
    if (m_measurement->hasImage()) {
      const auto &img = m_measurement->image();
      const auto &b = m_measurement->boundaries();
      const auto &tracingData = m_measurement->fringeTracing();
      text += tr("Image: %1 × %2 px<br>File: %3<br><br>"
                 "External boundaries: %4<br>"
                 "Internal boundaries: %5<br><br>"
                 "Seed points: %6<br>"
                 "Traced lines: %7<br><br>"
                 "Use the toolbar to add/move/delete boundaries and "
                 "seed points, and to run the tracer. Double-click a "
                 "traced line to edit its points or fringe order.")
                  .arg(img.width())
                  .arg(img.height())
                  .arg(m_measurement->imagePath())
                  .arg(b.getExternal().size())
                  .arg(b.getInternal().size())
                  .arg(tracingData.seeds().size())
                  .arg(tracingData.tracedLines().size());
    } else {
      text += tr("No image loaded yet. Use File → Open Image.");
    }
  } else {
    text += tr("No parameters yet — this stage is not implemented.");
  }

  m_label->setText(text);
}

}  // namespace digitqt::gui
