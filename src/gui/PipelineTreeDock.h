#pragma once

#include "core/pipeline/PipelineStageId.h"

#include <QDockWidget>

class QTreeWidget;

namespace digitqt::core::pipeline {
class Pipeline;
}

namespace digitqt::gui {

/**
 * @brief Left-hand dock: the canonical pipeline (S0..S7) as a tree.
 *
 * Pure presentation over core::pipeline::Pipeline -- per "views never
 * compute", selecting a node only emits stageSelected(); it never runs
 * anything itself.
 */
class PipelineTreeDock : public QDockWidget {
  Q_OBJECT
public:
  explicit PipelineTreeDock(QWidget *parent = nullptr);

  void setPipeline(digitqt::core::pipeline::Pipeline *pipeline);

  /// Re-reads status() from every stage and refreshes the tree's icons.
  /// Call after anything that might change a stage's status.
  void refreshStatuses();

signals:
  void stageSelected(digitqt::core::pipeline::StageId id);

private:
  QTreeWidget *m_tree;
  digitqt::core::pipeline::Pipeline *m_pipeline = nullptr;
};

}  // namespace digitqt::gui
