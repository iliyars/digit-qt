#pragma once

#include <QImage>
#include <QString>

namespace digitqt::io {
/**
 * @brief Result of an image load attempt.
 *
 * On failure, image is null and errorMessage explains why (bad path,
 * unsupported format, corrupt file, ...).
 */
struct ImageLoadResult {
  QImage image;
  QString errorMessage;
  bool ok() const { return !image.isNull(); }
};

/**
 * @brief Reads an image file from disk.
 *
 * Pure I/O: knows nothing about Measurement or any other domain concept.
 * Kept separate from core::Measurement so the document model doesn't have
 * to grow a new "import" method for every file format the app will
 * eventually read (markers, calibration wavefronts, synthesized fringes...).
 * See the master specification's recommended /io layer.
 */
ImageLoadResult loadImage(const QString &path);

}  // namespace digitqt::io
