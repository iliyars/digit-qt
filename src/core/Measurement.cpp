#include "Measurement.h"

namespace digitqt::core {

void Measurement::setImage(QImage image, QString path) {
  m_image = std::move(image);
  m_imagePath = std::move(path);
  m_boundaries.clear();
  m_modified = true;
}
} // namespace digitqt::core
