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

  if (m_currentStage == StageId::S0a && m_measurement) {
    const auto &b = m_measurement->boundaries();
    text += tr("External boundaries: %1<br>"
               "Internal boundaries: %2<br><br>"
               "Use the toolbar to add, move or delete boundaries directly on "
               "the image.")
                .arg(b.getExternal().size())
                .arg(b.getInternal().size());
  } else {
    text += tr("No parameters yet — this stage is not implemented.");
  }

  m_label->setText(text);
}

} // namespace digitqt::gui
