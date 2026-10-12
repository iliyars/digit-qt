#include "Measurement.h"

#include <utility>

namespace digitqt::core {

void Measurement::setImage(digitqt::core::Bitmap image, std::string path) {
  m_image = std::move(image);
  m_imagePath = std::move(path);
  m_imageIsPlaceholder = false;
  m_boundaries.clear();
  m_fringeTracing.clear();
  m_fiducials.clear();
  m_fiducialTransformFit = FiducialTransformFit{};
  m_phaseMap.clear();
  m_wavefrontMap.clear();
  m_modalAnalysis = ModalAnalysisResult{};
  m_modified = true;
}

void Measurement::setPlaceholderImage(int width, int height) {
  digitqt::core::Bitmap blank(width, height);
  blank.fill(0);
  setImage(std::move(blank), std::string());
  m_imageIsPlaceholder = true;
}

void Measurement::setImportedPhaseMap(PhaseMap phase) {
  m_image = digitqt::core::Bitmap();
  m_imagePath.clear();
  m_imageIsPlaceholder = false;
  m_boundaries.clear();
  m_fringeTracing.clear();
  m_fiducials.clear();
  m_fiducialTransformFit = FiducialTransformFit{};
  m_phaseMap = std::move(phase);
  m_wavefrontMap.clear();
  m_modalAnalysis = ModalAnalysisResult{};
  m_modified = true;
}

}  // namespace digitqt::core
