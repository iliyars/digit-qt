#include "ApertureMaskingStage.h"

namespace digitqt::core::pipeline {

bool ApertureMaskingStage::doCompute(
    digitqt::core::Measurement & /*measurement*/, QString & /*errorMessage*/) {
  // Nothing to compute yet -- see class comment. Boundaries are already
  // live data on Measurement by the time this runs.
  return true;
}

} // namespace digitqt::core::pipeline
