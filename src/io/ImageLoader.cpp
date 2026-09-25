#include "ImageLoader.h"

#include <QImage>
#include <QImageReader>

#include <cstring>

namespace digitqt::io {

digitqt::core::Bitmap fromQImage(const QImage &image) {
  const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
  digitqt::core::Bitmap bitmap(gray.width(), gray.height());
  for (int y = 0; y < gray.height(); ++y) {
    const uchar *srcRow = gray.constScanLine(y);
    uint8_t *dstRow = bitmap.scanLine(y);
    std::memcpy(dstRow, srcRow, static_cast<size_t>(gray.width()));
  }
  return bitmap;
}

ImageLoadResult loadImage(const QString &path) {
  QImageReader reader(path);
  reader.setAutoTransform(true);

  ImageLoadResult result;
  const QImage loaded = reader.read();
  if (loaded.isNull()) {
    result.errorMessage = reader.errorString();
    return result;
  }

  result.image = fromQImage(loaded);
  return result;
}

QImage toQImage(const digitqt::core::Bitmap &bitmap) {
  if (bitmap.isNull())
    return {};
  QImage img(bitmap.width(), bitmap.height(), QImage::Format_Grayscale8);
  for (int y = 0; y < bitmap.height(); ++y)
    std::memcpy(img.scanLine(y), bitmap.scanLine(y), static_cast<size_t>(bitmap.width()));
  return img;
}

}  // namespace digitqt::io
