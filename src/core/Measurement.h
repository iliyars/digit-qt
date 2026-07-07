#pragma once

#include <QImage>
#include <QString>

#include "core/FringeTracingData.h"

#include <aperture/include/visibility/ShapeCollection.h>

namespace digitqt::core {

/**
 * @brief The application's single "document".
 *
 * Follows the rule from the MFringe master specification:
 *   "One Measurement = One Physical Measurement"
 *   "Measurement owns all data. Views never store persistent results."
 *
 * Today Measurement only holds the raw image and the boundary shapes
 * (S0 / S0a in the canonical pipeline). Later pipeline stages (reference
 * markers, phase, wavefront, ...) will be added here as additional members
 * -- never inside a widget/view.
 */
class Measurement {
public:
  Measurement() = default;

  // --- S0: raw image -------------------------------------------------
  // Loading images from disk is not this class's job (see io::loadImage).
  // Measurement only stores the result.
  void setImage(QImage image, QString path);
  bool hasImage() const { return !m_image.isNull(); }
  const QImage &image() const { return m_image; }
  const QString &imagePath() const { return m_imagePath; }

  // --- S0a: boundaries (external aperture / internal obstructions) ---
  aperture::ShapeCollection &boundaries() { return m_boundaries; }
  const aperture::ShapeCollection &boundaries() const { return m_boundaries; }

  // --- S1: fringe tracing (seed points + traced centerlines) ---------
  FringeTracingData &fringeTracing() { return m_fringeTracing; }
  const FringeTracingData &fringeTracing() const { return m_fringeTracing; }

  bool isModified() const { return m_modified; }
  void setModified(bool modified) { m_modified = modified; }

private:
  QImage m_image;
  QString m_imagePath;
  aperture::ShapeCollection m_boundaries;
  FringeTracingData m_fringeTracing;
  bool m_modified = false;
};

} // namespace digitqt::core
