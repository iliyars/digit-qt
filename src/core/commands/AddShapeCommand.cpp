#include "AddShapeCommand.h"
#include "core/ShapeCollectionAccess.h"

#include <QCoreApplication>

namespace digitqt::commands {
AddShapeCommand::AddShapeCommand(aperture::ShapeCollection &collection,
                                 aperture::TypeLimits type,
                                 std::unique_ptr<aperture::Shape> shape,
                                 QUndoCommand *parent)
    : QUndoCommand(parent), m_collection(collection), m_type(type),
      m_shape(std::move(shape)) {
  setText(type == aperture::TypeLimits::EXTERNAL
              ? QCoreApplication::translate("AddShapeCommand",
                                            "Add external boundary")
              : QCoreApplication::translate("AddShapeCommand",
                                            "Add internal boundary"));
}

void AddShapeCommand::redo() {
  auto &container = digitqt::core::mutableContainer(m_collection, m_type);
  m_insertedIndex = container.size();
  container.push_back(std::move(m_shape));
  m_collection.notifyShapeModified();
}

void AddShapeCommand::undo() {
  auto &container = digitqt::core::mutableContainer(m_collection, m_type);
  if (m_insertedIndex >= container.size())
    return;
  m_shape = std::move(container[m_insertedIndex]);
  container.erase(container.begin() + static_cast<long>(m_insertedIndex));
  m_collection.notifyShapeModified();
}
} // namespace digitqt::commands
