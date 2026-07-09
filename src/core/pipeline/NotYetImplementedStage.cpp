#include "NotYetImplementedStage.h"

namespace digitqt::core::pipeline {

bool NotYetImplementedStage::doCompute(
    digitqt::core::Measurement & /*measurement*/, QString &errorMessage) {
  errorMessage = QStringLiteral("%1 (%2) is not implemented yet")
                     .arg(displayName(id()), shortName(id()));
  return false;
}

}  // namespace digitqt::core::pipeline
