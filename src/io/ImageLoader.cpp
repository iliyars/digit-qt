#include "ImageLoader.h"

#include <QImageReader>

namespace digitqt::io {
ImageLoadResult loadImage(const QString &path) {
  QImageReader reader(path);
  reader.setAutoTransform(true);

  ImageLoadResult result;
  result.image = reader.read();
  if (result.image.isNull())
    result.errorMessage = reader.errorString();
  return result;
}

} // namespace digitqt::io
