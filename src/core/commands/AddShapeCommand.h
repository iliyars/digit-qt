#pragma once

#include <QUndoCommand>
#include <aperture/include/visibility/ShapeCollection.h>
#include <memory>

namespace digitqt::commands {
/**
 * @brief Adds one boundary shape (external or internal) to a ShapeCollection.
 *
 * redo() inserts the shape at the end of its type's container.
 * undo() removes exactly the shape that was inserted.
 */

class AddShapeCommand : public QUndoCommand {
public:
  AddShapeCommand(aperture::ShapeCollection &collection,
                  aperture::TypeLimits type,
                  std::unique_ptr<aperture::Shape> shape,
                  QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  aperture::ShapeCollection &m_collection;
  aperture::TypeLimits m_type;
  std::unique_ptr<aperture::Shape> m_shape;
  size_t m_insertedIndex = 0;
};
}  // namespace digitqt::commands
