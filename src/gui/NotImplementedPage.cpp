#include "NotImplementedPage.h"

#include <QLabel>
#include <QVBoxLayout>

namespace digitqt::gui {

NotImplementedPage::NotImplementedPage(QWidget *parent)
    : QWidget(parent), m_label(new QLabel(this)) {
  m_label->setAlignment(Qt::AlignCenter);
  QFont f = m_label->font();
  f.setPointSize(f.pointSize() + 2);
  m_label->setFont(f);
  m_label->setStyleSheet(QStringLiteral("color: palette(mid);"));

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_label);
}

void NotImplementedPage::setStage(digitqt::core::pipeline::StageId id) {
  m_label->setText(tr("%1 (%2)\nnot implemented yet")
                       .arg(digitqt::core::pipeline::displayName(id),
                            digitqt::core::pipeline::shortName(id)));
}

}  // namespace digitqt::gui
