#include "PipelineStageId.h"

namespace digitqt::core::pipeline {

QString shortName(StageId id) {
  switch (id) {
  case StageId::Setup:
    return QStringLiteral("Setup");
  case StageId::S2:
    return QStringLiteral("S2");
  case StageId::S3:
    return QStringLiteral("S3");
  case StageId::S4:
    return QStringLiteral("S4");
  case StageId::S4b:
    return QStringLiteral("S4b");
  case StageId::S5:
    return QStringLiteral("S5");
  case StageId::S6:
    return QStringLiteral("S6");
  case StageId::S7:
    return QStringLiteral("S7");
  }
  return QStringLiteral("?");
}

QString displayName(StageId id) {
  switch (id) {
  case StageId::Setup:
    return QStringLiteral("Setup (Image, Aperture, Markers, Fringe Tracing)");
  case StageId::S2:
    return QStringLiteral("Phase Reconstruction");
  case StageId::S3:
    return QStringLiteral("Phase Unwrapping");
  case StageId::S4:
    return QStringLiteral("Wavefront Reconstruction");
  case StageId::S4b:
    return QStringLiteral("Wavefront Calibration");
  case StageId::S5:
    return QStringLiteral("Polynomial / Modal Analysis");
  case StageId::S6:
    return QStringLiteral("Diffraction Analysis");
  case StageId::S7:
    return QStringLiteral("Interferogram Synthesis");
  }
  return QStringLiteral("Unknown Stage");
}

} // namespace digitqt::core::pipeline
