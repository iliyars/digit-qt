#include "RemoveShapeCommand.h"

#include "core/ShapeCollectionAccess.h"

#include <QCoreApplication>
#include <algorithm>

namespace digitqt::commands {
RemoveShapeCommand::RemoveShapeCommand(aperture::ShapeCollection &collection,
                                       aperture::TypeLimits type, size_t index,
                                       QUndoCommand *parent)
    : QUndoCommand(parent),
      m_collection(collection),
      m_type(type),
      m_index(index) {
  setText(QCoreApplication::translate("RemoveShapeCommand", "Remove boundary"));
}

void RemoveShapeCommand::redo() {
  auto &container = digitqt::core::mutableContainer(m_collection, m_type);
  if (m_index >= container.size())
    return;
  m_removed = std::move(container[m_index]);
  container.erase(container.begin() + static_cast<long>(m_index));
  m_collection.notifyShapeModified();
}

void RemoveShapeCommand::undo() {
  if (!m_removed)
    return;
  auto &container = digitqt::core::mutableContainer(m_collection, m_type);
  const size_t insertAt = std::min(m_index, container.size());
  container.insert(container.begin() + static_cast<long>(insertAt),
                   std::move(m_removed));
  m_collection.notifyShapeModified();
}

}  // namespace digitqt::commands
