#pragma once

#include "core/pipeline/PipelineStageId.h"

#include <QWidget>

class QLabel;

namespace digitqt::gui {

/// Shown in the central QStackedWidget for any stage that isn't wired to a
/// real view yet.
class NotImplementedPage : public QWidget {
  Q_OBJECT
public:
  explicit NotImplementedPage(QWidget *parent = nullptr);

  void setStage(digitqt::core::pipeline::StageId id);

private:
  QLabel *m_label;
};

}  // namespace digitqt::gui
