#include "ParametersDock.h"

#include "core/Measurement.h"

#include <QLabel>
#include <QVBoxLayout>

namespace digitqt::gui {

using digitqt::core::pipeline::StageId;

ParametersDock::ParametersDock(QWidget *parent)
    : QDockWidget(tr("Parameters"), parent), m_label(new QLabel(this)) {
  setObjectName("ParametersDock");
  m_label->setWordWrap(true);
  m_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_label->setContentsMargins(8, 8, 8, 8);
  m_label->setTextFormat(Qt::RichText);

  auto *container = new QWidget(this);
  auto *layout = new QVBoxLayout(container);
  layout->addWidget(m_label);
  layout->addStretch();
  setWidget(container);
}

void ParametersDock::setMeasurement(digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  refresh();
}

void ParametersDock::setStage(StageId id) {
  m_currentStage = id;
  refresh();
}

void ParametersDock::refresh() {
  using digitqt::core::pipeline::displayName;
  using digitqt::core::pipeline::shortName;

  QString text =
      tr("<b>%1</b> (%2)<br><br>")
          .arg(displayName(m_currentStage), shortName(m_currentStage));

  if (m_currentStage == StageId::Setup && m_measurement) {
    if (m_measurement->hasImage()) {
      const auto &img = m_measurement->image();
      const auto &b = m_measurement->boundaries();
      const auto &tracingData = m_measurement->fringeTracing();
      text += tr("Image: %1 × %2 px<br>File: %3<br><br>"
                 "External boundaries: %4<br>"
                 "Internal boundaries: %5<br><br>"
                 "Fringe tracing (SCAN-tracer):<br>"
                 "Seed points: %6<br>"
                 "Traced lines: %7<br><br>"
                 "Use the toolbar to add/move/delete boundaries and "
                 "seed points, and to run the tracer.")
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

} // namespace digitqt::gui
