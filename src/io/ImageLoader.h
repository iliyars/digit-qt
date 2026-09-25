#pragma once

#include "core/Bitmap.h"
#include <QString>

class QImage;

namespace digitqt::io {
/**
 * @brief Result of an image load attempt.
 *
 * On failure, image is null and errorMessage explains why (bad path,
 * unsupported format, corrupt file, ...).
 */
struct ImageLoadResult {
  digitqt::core::Bitmap image;
  QString errorMessage;
  bool ok() const { return !image.isNull(); }
};

/**
 * @brief Reads an image file from disk and converts it to grayscale
 * (core::Bitmap) -- the only place this conversion happens; everything
 * downstream (tracers, phase reconstruction) gets an already-grayscale
 * buffer instead of repeating QImage::convertToFormat() itself.
 *
 * Pure I/O: knows nothing about Measurement or any other domain concept.
 * Kept separate from core::Measurement so the document model doesn't have
 * to grow a new "import" method for every file format the app will
 * eventually read (markers, calibration wavefronts, synthesized fringes...).
 * See the master specification's recommended /io layer.
 */
ImageLoadResult loadImage(const QString &path);

/// Конвертирует произвольный QImage в grayscale core::Bitmap -- то же
/// самое, что loadImage() делает после чтения файла. Публичный, отдельно
/// от loadImage(), чтобы тесты и другие Qt-based источники (не только
/// файлы с диска) могли получить Bitmap тем же путём.
digitqt::core::Bitmap fromQImage(const QImage &image);

/// Обратная конвертация -- нужна только для отображения (ImageCanvas).
/// Живёт здесь, а не в core/, потому что возвращает Qt-тип.
QImage toQImage(const digitqt::core::Bitmap &bitmap);

}  // namespace digitqt::io
