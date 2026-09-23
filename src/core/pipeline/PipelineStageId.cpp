#include "PipelineStageId.h"

namespace digitqt::core::pipeline {

std::string shortName(StageId id) {
  switch (id) {
    case StageId::Setup:
      return "Setup";
    case StageId::S2:
      return "S2";
    case StageId::S4:
      return "S4";
    case StageId::S4b:
      return "S4b";
    case StageId::S5:
      return "S5";
    case StageId::S6:
      return "S6";
    case StageId::S7:
      return "S7";
  }
  return "?";
}

std::string displayName(StageId id) {
  switch (id) {
    case StageId::Setup:
      return "Setup (Image, Aperture, Markers, Fringe Tracing)";
    case StageId::S2:
      return "Phase Reconstruction";
    case StageId::S4:
      return "Wavefront Reconstruction";
    case StageId::S4b:
      return "Wavefront Calibration";
    case StageId::S5:
      return "Polynomial / Modal Analysis";
    case StageId::S6:
      return "Diffraction Analysis";
    case StageId::S7:
      return "Interferogram Synthesis";
  }
  return "Unknown Stage";
}

}  // namespace digitqt::core::pipeline
