#pragma once

#include <QUndoCommand>
#include <memory>

#include <aperture/include/visibility/ShapeCollection.h>

namespace digitqt::commands {
/**
 * @brief Removes one boundary shape by index from a ShapeCollection.
 *
 * redo() erases the shape at m_index (keeping a clone for undo).
 * undo() re-inserts it back at the same index.
 */
class RemoveShapeCommand : public QUndoCommand {
public:
  RemoveShapeCommand(aperture::ShapeCollection &collection,
                     aperture::TypeLimits type, size_t index,
                     QUndoCommand *parent = nullptr);
  void redo() override;
  void undo() override;

private:
  aperture::ShapeCollection &m_collection;
  aperture::TypeLimits m_type;
  size_t m_index;
  std::unique_ptr<aperture::Shape> m_removed;
};
} // namespace digitqt::commands
