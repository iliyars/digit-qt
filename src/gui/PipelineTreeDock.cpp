#include "PipelineTreeDock.h"

#include "core/pipeline/Pipeline.h"

#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace digitqt::gui {

using digitqt::core::pipeline::kCanonicalOrder;
using digitqt::core::pipeline::StageId;
using digitqt::core::pipeline::StageStatus;

namespace {

constexpr int kStageIdRole = Qt::UserRole + 1;

QIcon statusIcon(const QWidget *w, StageStatus status) {
  switch (status) {
    case StageStatus::Computed:
      return w->style()->standardIcon(QStyle::SP_DialogApplyButton);
    case StageStatus::Stale:
      return w->style()->standardIcon(QStyle::SP_BrowserReload);
    case StageStatus::Error:
      return w->style()->standardIcon(QStyle::SP_MessageBoxCritical);
    case StageStatus::NotComputed:
      return w->style()->standardIcon(QStyle::SP_DialogCancelButton);
  }
  return {};
}

}  // namespace

PipelineTreeDock::PipelineTreeDock(QWidget *parent)
    : QDockWidget(tr("Pipeline"), parent), m_tree(new QTreeWidget(this)) {
  setObjectName("PipelineTreeDock");
  m_tree->setHeaderHidden(true);
  m_tree->setRootIsDecorated(false);
  m_tree->setIndentation(0);
  setWidget(m_tree);

  for (const StageId id : kCanonicalOrder) {
    auto *item =
        new QTreeWidgetItem(m_tree, {digitqt::core::pipeline::displayName(id)});
    item->setData(0, kStageIdRole, static_cast<int>(id));
    item->setToolTip(0, digitqt::core::pipeline::shortName(id));
  }

  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
            if (!current)
              return;
            emit stageSelected(
                static_cast<StageId>(current->data(0, kStageIdRole).toInt()));
          });

  if (auto *first = m_tree->topLevelItem(0))
    m_tree->setCurrentItem(first);
}

void PipelineTreeDock::setPipeline(
    digitqt::core::pipeline::Pipeline *pipeline) {
  m_pipeline = pipeline;
  refreshStatuses();
}

void PipelineTreeDock::refreshStatuses() {
  if (!m_pipeline)
    return;
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
    auto *item = m_tree->topLevelItem(i);
    const auto id = static_cast<StageId>(item->data(0, kStageIdRole).toInt());
    item->setIcon(0, statusIcon(m_tree, m_pipeline->stage(id).status()));
  }
}

}  // namespace digitqt::gui
