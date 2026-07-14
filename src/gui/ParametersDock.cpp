#include "ParametersDock.h"

#include "canvas/FringeTracingController.h"
#include "canvas/PhaseMapView.h"
#include "core/Measurement.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <cmath>


namespace digitqt::gui {

using digitqt::core::FringeCenterMode;
using digitqt::core::TracerAlgorithm;
using digitqt::core::pipeline::StageId;

ParametersDock::ParametersDock(QWidget *parent)
    : QDockWidget(tr("Parameters"), parent),
      m_algorithmCombo(new QComboBox(this)),
      m_fringeCenterCombo(new QComboBox(this)),
      m_orderSpin(new QDoubleSpinBox(this)),
      m_wavelengthSpin(new QDoubleSpinBox(this)),
      m_isolineStepSpin(new QDoubleSpinBox(this)),
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

  m_wavelengthSpin->setRange(1.0, 20000.0);
  m_wavelengthSpin->setDecimals(1);
  m_wavelengthSpin->setSuffix(tr(" nm"));
  connect(m_wavelengthSpin, &QDoubleSpinBox::valueChanged, this,
          &ParametersDock::onWavelengthChanged);

  m_isolineStepSpin->setRange(0.001, 100000.0);
  m_isolineStepSpin->setDecimals(3);
  connect(m_isolineStepSpin, &QDoubleSpinBox::valueChanged, this,
          &ParametersDock::onIsolineStepChanged);

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

  m_wavelengthRow = new QWidget(container);
  auto *wavelengthLayout = new QVBoxLayout(m_wavelengthRow);
  wavelengthLayout->setContentsMargins(0, 0, 0, 0);
  auto *wavelengthLabel = new QLabel(tr("<b>Wavelength</b>"), m_wavelengthRow);
  wavelengthLabel->setContentsMargins(8, 8, 8, 0);
  wavelengthLayout->addWidget(wavelengthLabel);
  wavelengthLayout->addWidget(m_wavelengthSpin);
  layout->addWidget(m_wavelengthRow);
  m_wavelengthRow->setVisible(false);

  m_isolineStepRow = new QWidget(container);
  auto *isolineLayout = new QVBoxLayout(m_isolineStepRow);
  isolineLayout->setContentsMargins(0, 0, 0, 0);
  auto *isolineLabel = new QLabel(tr("<b>Isoline step</b>"), m_isolineStepRow);
  isolineLabel->setContentsMargins(8, 8, 8, 0);
  isolineLayout->addWidget(isolineLabel);
  isolineLayout->addWidget(m_isolineStepSpin);
  layout->addWidget(m_isolineStepRow);
  m_isolineStepRow->setVisible(false);

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

    const QSignalBlocker blocker(m_wavelengthSpin);
    m_wavelengthSpin->setValue(m_measurement->wavelengthNm());
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

void ParametersDock::setPhaseMapView(digitqt::gui::canvas::PhaseMapView *view) {
  m_phaseMapView = view;
}

void ParametersDock::setStage(StageId id) {
  m_currentStage = id;
  m_wavelengthRow->setVisible(id == StageId::S4);

  const bool showIsolineStep = (id == StageId::S2 || id == StageId::S4);
  m_isolineStepRow->setVisible(showIsolineStep);
  if (showIsolineStep && m_phaseMapView) {
    // setSource() (called by MainWindow just before this) already
    // auto-adjusted the step for the newly-selected map's units --
    // just reflect that here, don't override it.
    const QSignalBlocker blocker(m_isolineStepSpin);
    m_isolineStepSpin->setValue(m_phaseMapView->isolineStep());
    m_isolineStepSpin->setSuffix(id == StageId::S4 ? tr(" nm") : QString());
  }

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

void ParametersDock::onWavelengthChanged(double value) {
  if (!m_measurement)
    return;
  m_measurement->setWavelengthNm(value);
}

void ParametersDock::onIsolineStepChanged(double value) {
  if (m_phaseMapView)
    m_phaseMapView->setIsolineStep(value);
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
  } else if (m_currentStage == StageId::S2 && m_measurement) {
    const auto &phase = m_measurement->phaseMap();
    if (phase.isEmpty()) {
      text +=
          tr("Not computed yet. Press ▶ Compute phase in the toolbar "
             "(needs numbered fringe lines from Setup first).");
    } else {
      text += tr("Grid: %1 × %2<br><br>"
                 "Values are in fringe-order units (not yet physical "
                 "length) -- see S4 (Wavefront Reconstruction) for that.")
                  .arg(phase.width())
                  .arg(phase.height());
    }
  } else if (m_currentStage == StageId::S4 && m_measurement) {
    const auto &wavefront = m_measurement->wavefrontMap();
    if (wavefront.isEmpty()) {
      text +=
          tr("Not computed yet. Set the wavelength above, then press "
             "▶ Compute wavefront in the toolbar (needs S2 first).");
    } else {
      text += tr("Grid: %1 × %2<br>"
                 "Wavelength: %3 nm<br><br>"
                 "height = fringe order × wavelength / 2")
                  .arg(wavefront.width())
                  .arg(wavefront.height())
                  .arg(m_measurement->wavelengthNm());
    }
  } else if (m_currentStage == StageId::S5 && m_measurement) {
    const auto &modal = m_measurement->modalAnalysis();
    if (modal.isEmpty()) {
      text +=
          tr("Not computed yet. Press ▶ Fit aberrations in the toolbar "
             "(needs a wavefront map from S4 first).");
    } else {
      const auto &c = modal.coefficients;
      const double astigMag = std::sqrt(c.astigX * c.astigX + c.astigY * c.astigY);
      const double astigAngleDeg =
          0.5 * std::atan2(c.astigY, c.astigX) * 180.0 / 3.14159265358979323846;
      const double comaMag = std::sqrt(c.comaX * c.comaX + c.comaY * c.comaY);
      const double comaAngleDeg = std::atan2(c.comaY, c.comaX) * 180.0 / 3.14159265358979323846;
      const double trefoilMag = std::sqrt(c.trefoilX * c.trefoilX + c.trefoilY * c.trefoilY);
      const double trefoilAngleDeg =
          std::atan2(c.trefoilY, c.trefoilX) * 180.0 / 3.14159265358979323846 / 3.0;

      text += tr("Setup geometry (not a surface property):<br>"
                 "Piston: %1<br>"
                 "Tilt X: %2<br>"
                 "Tilt Y: %3<br>"
                 "Defocus: %4<br><br>"
                 "Surface aberrations:<br>"
                 "Astigmatism: %5 at %6°<br>"
                 "Coma: %7 at %8°<br>"
                 "Trefoil: %9 at %10°<br>"
                 "Spherical (3rd order): %11<br><br>"
                 "RMS before: %12 nm<br>"
                 "RMS after: %13 nm<br><br>"
                 "The 3D view shows what's left after subtracting all of "
                 "the above -- drag to rotate, scroll to zoom.")
                  .arg(c.piston, 0, 'f', 1)
                  .arg(c.tiltX, 0, 'f', 1)
                  .arg(c.tiltY, 0, 'f', 1)
                  .arg(c.defocus, 0, 'f', 1)
                  .arg(astigMag, 0, 'f', 1)
                  .arg(astigAngleDeg, 0, 'f', 0)
                  .arg(comaMag, 0, 'f', 1)
                  .arg(comaAngleDeg, 0, 'f', 0)
                  .arg(trefoilMag, 0, 'f', 1)
                  .arg(trefoilAngleDeg, 0, 'f', 0)
                  .arg(c.spherical, 0, 'f', 1)
                  .arg(modal.rmsBefore, 0, 'f', 1)
                  .arg(modal.rmsAfter, 0, 'f', 1);
    }
  } else {
    text += tr("No parameters yet — this stage is not implemented.");
  }

  m_label->setText(text);
}

}  // namespace digitqt::gui
