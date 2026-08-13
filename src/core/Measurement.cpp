#include "Measurement.h"

#include <utility>

namespace digitqt::core {

void Measurement::setImage(QImage image, QString path) {
  m_image = std::move(image);
  m_imagePath = std::move(path);
  m_boundaries.clear();
  m_fringeTracing.clear();
  m_phaseMap.clear();
  m_wavefrontMap.clear();
  m_modalAnalysis = ModalAnalysisResult{};
  m_modified = true;
}

void Measurement::setImportedPhaseMap(PhaseMap phase) {
  m_image = QImage();
  m_imagePath.clear();
  m_boundaries.clear();
  m_fringeTracing.clear();
  m_phaseMap = std::move(phase);
  m_wavefrontMap.clear();
  m_modalAnalysis = ModalAnalysisResult{};
  m_modified = true;
}

}  // namespace digitqt::core
