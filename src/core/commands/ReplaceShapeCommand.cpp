#include "ReplaceShapeCommand.h"

#include "core/ShapeCollectionAccess.h"

#include <QCoreApplication>

namespace digitqt::commands {

ReplaceShapeCommand::ReplaceShapeCommand(
    aperture::ShapeCollection &collection, aperture::TypeLimits type,
    size_t index, std::unique_ptr<aperture::Shape> before,
    std::unique_ptr<aperture::Shape> after, QUndoCommand *parent)
    : QUndoCommand(parent),
      m_collection(collection),
      m_type(type),
      m_index(index),
      m_before(std::move(before)),
      m_after(std::move(after)) {
  setText(QCoreApplication::translate("ReplaceShapeCommand",
                                      "Move/resize boundary"));
}

void ReplaceShapeCommand::redo() {
  auto &container = digitqt::core::mutableContainer(m_collection, m_type);
  if (m_index >= container.size())
    return;
  container[m_index] = m_after->clone();
  m_collection.notifyShapeModified();
}

void ReplaceShapeCommand::undo() {
  auto &container = digitqt::core::mutableContainer(m_collection, m_type);
  if (m_index >= container.size())
    return;
  container[m_index] = m_before->clone();
  m_collection.notifyShapeModified();
}

}  // namespace digitqt::commands
