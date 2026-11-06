#include "PsiWorkflowPanel.h"

#include "core/Measurement.h"
#include "core/pipeline/Pipeline.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QDir>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>

namespace digitqt::gui {
using digitqt::core::pipeline::StageId;
using digitqt::core::pipeline::StageStatus;

PsiWorkflowPanel::PsiWorkflowPanel(QWidget *parent) : QWidget(parent) {
  setObjectName("PsiWorkflowPanel");
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 4);
  auto *header = new QHBoxLayout;
  m_summary = new QLabel(this);
  m_summary->setTextFormat(Qt::PlainText);
  m_summary->setWordWrap(true);
  header->addWidget(m_summary, 1);
  auto *open = new QPushButton(tr("Open PSI series..."), this);
  open->setObjectName("psiOpen");
  header->addWidget(open);
  connect(open, &QPushButton::clicked, this, &PsiWorkflowPanel::openRequested);
  layout->addLayout(header);
  m_frames = new QListWidget(this);
  m_frames->setObjectName("psiFrames");
  m_frames->setViewMode(QListView::IconMode);
  m_frames->setFlow(QListView::LeftToRight);
  m_frames->setWrapping(false);
  m_frames->setMovement(QListView::Static);
  m_frames->setIconSize(QSize(72, 54));
  m_frames->setMaximumHeight(88);
  m_frames->setMinimumHeight(88);
  m_frames->setToolTip(
      tr("Select a frame to inspect it. The original order and calculation data stay unchanged."));
  layout->addWidget(m_frames);
  connect(m_frames, &QListWidget::currentRowChanged, this, [this](int index) {
    if (index >= 0)
      emit frameSelected(index);
  });
  m_actions = new QWidget(this);
  auto *actions = new QHBoxLayout(m_actions);
  actions->setContentsMargins(0, 0, 0, 0);
  auto addButton = [this](QHBoxLayout *row, const QString &text, const char *name, auto signal) {
    auto *button = new QPushButton(text, this);
    button->setObjectName(name);
    row->addWidget(button);
    connect(button, &QPushButton::clicked, this, signal);
    return button;
  };
  m_fullFrame = new QCheckBox(tr("Use the entire frame"), this);
  m_fullFrame->setObjectName("psiFullFrame");
  actions->addWidget(m_fullFrame);
  connect(m_fullFrame, &QCheckBox::toggled, this, [this] { refresh(); });
  actions->addStretch();
  m_calculate = addButton(actions, tr("Calculate PSI"), "psiCalculate",
                          &PsiWorkflowPanel::calculateRequested);
  m_calculate->setStyleSheet(QStringLiteral("QPushButton { font-weight: 600; }"));
  m_calculate->setToolTip(
      tr("Calculate phase, convert to physical units and fit aberrations using the current "
         "settings."));
  layout->addWidget(m_actions);
  m_next = new QLabel(this);
  m_next->setObjectName("psiNextStep");
  m_next->setWordWrap(true);
  m_next->setTextFormat(Qt::PlainText);
  layout->addWidget(m_next);
  m_results = new QWidget(this);
  m_results->setStyleSheet(QStringLiteral(
      "QPushButton:disabled {"
      " color: #969696;"
      " background-color: #e8e8e8;"
      " border: 1px solid #d2d2d2;"
      " border-radius: 2px;"
      "}"));
  auto *results = new QHBoxLayout(m_results);
  results->setContentsMargins(0, 0, 0, 0);
  addButton(results, tr("Frames / aperture"), "psiSource", &PsiWorkflowPanel::sourceRequested);
  m_phase = addButton(results, tr("Phase"), "psiPhase", &PsiWorkflowPanel::phaseRequested);
  m_wavefront = addButton(results, tr("Surface / wavefront"), "psiWavefront",
                          &PsiWorkflowPanel::wavefrontRequested);
  m_analysis =
      addButton(results, tr("Aberrations"), "psiAnalysis", &PsiWorkflowPanel::analysisRequested);
  results->addStretch();
  m_exportMap = addButton(results, tr("Export map..."), "psiExportMap",
                          &PsiWorkflowPanel::exportMapRequested);
  m_exportMap->setToolTip(tr("WinFringe matrix in phase cycles. Wavelength and reflection conversion are not applied to this file."));
  m_exportReport = addButton(results, tr("Report..."), "psiExportReport",
                             &PsiWorkflowPanel::exportReportRequested);
  layout->addWidget(m_results);
  m_quality = new QLabel(this);
  m_quality->setObjectName("psiQuality");
  m_quality->setTextFormat(Qt::PlainText);
  m_quality->setWordWrap(true);
  layout->addWidget(m_quality);
  m_warnings = new QPlainTextEdit(this);
  m_warnings->setObjectName("psiWarnings");
  m_warnings->setReadOnly(true);
  m_warnings->setMaximumHeight(82);
  m_warnings->setAccessibleName(tr("Measurement remarks"));
  layout->addWidget(m_warnings);
  refresh();
}

void PsiWorkflowPanel::setMeasurement(digitqt::core::Measurement *measurement,
                                      digitqt::core::pipeline::Pipeline *pipeline,
                                      const QStringList &paths) {
  m_measurement = measurement;
  m_pipeline = pipeline;
  const QSignalBlocker blocker(m_frames);
  m_frames->clear();
  m_fullFrame->setChecked(false);
  setWarnings({});
  if (measurement && measurement->phaseShifting().hasSeries()) {
    const auto &frames = measurement->phaseShifting().frames();
    for (size_t i = 0; i < frames.size(); ++i) {
      const auto &frame = frames[i];
      const QImage image(frame.data(), frame.width(), frame.height(), frame.width(),
                         QImage::Format_Grayscale8);
      auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(image.scaled(
                                           72, 54, Qt::KeepAspectRatio, Qt::SmoothTransformation))),
                                       tr("Frame %1").arg(i + 1), m_frames);
      if (static_cast<int>(i) < paths.size())
        item->setToolTip(paths[static_cast<int>(i)]);
    }
    m_frames->setCurrentRow(0);
  }
  refresh();
}

void PsiWorkflowPanel::setWarnings(const QStringList &warnings) {
  m_warnings->setPlainText(warnings.join(QStringLiteral("\n\n")));
  m_warnings->setVisible(!warnings.isEmpty());
}

void PsiWorkflowPanel::refresh() {
  const bool series = m_measurement && m_measurement->phaseShifting().hasSeries();
  const bool psi = series && m_measurement->customPhaseReconstructorPluginPath().empty() &&
                   m_measurement->phaseReconstructionAlgorithm() ==
                       digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting;
  const bool empty =
      !m_measurement || (!m_measurement->hasImage() && m_measurement->phaseMap().isEmpty());
  setVisible(psi || empty);
  m_frames->setVisible(psi);
  m_actions->setVisible(psi);
  m_results->setVisible(psi);
  m_results->setEnabled(false);
  m_quality->setVisible(psi);
  if (!psi) {
    m_summary->setText(tr("PSI: open a series of at least three phase-shifted frames."));
    m_next->setText(tr("Frames → aperture → calculation → result → export"));
    m_warnings->hide();
    return;
  }
  const auto &data = m_measurement->phaseShifting();
  m_wavefront->setText(m_measurement->isDoublePass() ? tr("Surface height") : tr("Wavefront"));
  m_warnings->setVisible(!m_warnings->toPlainText().isEmpty());
  const bool aperture = m_measurement->boundaries().hasAnyExternal();
  m_summary->setText(
      tr("PSI · %1 frames · %2 × %3 px · %4")
          .arg(data.frames().size())
          .arg(data.sourceWidth())
          .arg(data.sourceHeight())
          .arg(QFileInfo(QString::fromStdString(m_measurement->imagePath())).dir().dirName()+
               QStringLiteral(" / ")+QFileInfo(QString::fromStdString(m_measurement->imagePath())).fileName()));
  if (aperture) {
    const QSignalBlocker blocker(m_fullFrame);
    m_fullFrame->setChecked(false);
  }
  m_fullFrame->setVisible(!aperture);
  auto current = [&](StageId id) {
    return m_pipeline && m_pipeline->stage(id).status() == StageStatus::Computed;
  };
  const bool phase = current(StageId::S2), wavefront = current(StageId::S4),
             analysis = current(StageId::S5);
  m_results->setEnabled(phase);
  m_phase->setEnabled(phase);
  m_wavefront->setEnabled(phase && wavefront);
  m_analysis->setEnabled(phase && wavefront && analysis);
  m_exportMap->setEnabled(phase && wavefront);
  m_exportReport->setEnabled(phase && wavefront && analysis);
  m_calculate->setEnabled(aperture || m_fullFrame->isChecked());
  const bool previous = !m_measurement->phaseMap().isEmpty();
  m_calculate->setText(previous ? tr("Recalculate PSI") : tr("Calculate PSI"));
  if (!aperture && !m_fullFrame->isChecked())
    m_next->setText(tr(
        "Next: detect or draw the aperture. You can explicitly choose the entire frame instead."));
  else if (!phase)
    m_next->setText(previous
                        ? tr("Inputs changed. Recalculate before viewing or exporting the result.")
                        : tr("Next: check PSI, wavelength and reflection/transmission settings on "
                             "the right, then calculate."));
  else if (!wavefront || !analysis)
    m_next->setText(tr(
        "Phase is ready. Physical maps or aberration results are missing or out of date. Recalculate PSI."));
  else
    m_next->setText(tr("Result is ready. Inspect the maps and remarks before exporting."));
  if (m_pipeline)
    for (auto id : {StageId::S2, StageId::S4, StageId::S5})
      if (m_pipeline->stage(id).status() == StageStatus::Error) {
        m_next->setText(tr("Calculation stopped: %1")
                            .arg(QString::fromStdString(m_pipeline->stage(id).errorMessage())));
        break;
      }
  if (!phase) {
    m_quality->setText(previous
                           ? tr("Previous result is out of date. Its remarks are retained below.")
                           : tr("No calculated map yet."));
  } else {
    const auto &report = data.report();
    m_quality->setText(
        tr("Coverage: %1% · Unwrap regions: %2 · Phase: cycles · %3: nm · λ = %4 nm (%5)")
            .arg(100 * report.mapCoverage, 0, 'f', 1)
            .arg(report.regionCount)
            .arg(m_measurement->isDoublePass() ? tr("Surface height")
                                               : tr("Optical path difference"))
            .arg(m_measurement->wavelengthNm(), 0, 'f', 1)
            .arg(m_measurement->isDoublePass() ? tr("reflection, λ/2") : tr("transmission, λ")));
  }
}
}  // namespace digitqt::gui
