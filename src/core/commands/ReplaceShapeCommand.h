#pragma once

#include <QUndoCommand>
#include <memory>

#include <aperture/include/visibility/ShapeCollection.h>

namespace digitqt::commands {
/**
 * @brief Replaces one shape in-place with another (same type, same index).
 *
 * Used to make a completed move/resize drag a single undoable step: the
 * caller supplies the shape as it was before the drag and as it is after
 * the drag; redo()/undo() just swap between the two.
 */
class ReplaceShapeCommand : public QUndoCommand {
public:
  ReplaceShapeCommand(aperture::ShapeCollection &collection,
                      aperture::TypeLimits type, size_t index,
                      std::unique_ptr<aperture::Shape> before,
                      std::unique_ptr<aperture::Shape> after,
                      QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  aperture::ShapeCollection &m_collection;
  aperture::TypeLimits m_type;
  size_t m_index;
  std::unique_ptr<aperture::Shape> m_before;
  std::unique_ptr<aperture::Shape> m_after;
};
} // namespace digitqt::commands
