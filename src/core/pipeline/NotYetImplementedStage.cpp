#include "NotYetImplementedStage.h"

namespace digitqt::core::pipeline {

bool NotYetImplementedStage::doCompute(digitqt::core::Measurement & /*measurement*/,
                                       std::string &errorMessage) {
  errorMessage = displayName(id()) + " (" + shortName(id()) + ") is not implemented yet";
  return false;
}

}  // namespace digitqt::core::pipeline
