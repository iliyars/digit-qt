#pragma once

#include "formats/FormatAdapter.h"

namespace digitqt::formats {

/**
 * @brief Адаптер формата .frn
 */

class FrnAdapter : public IFormatAdapter {
public:
  std::string displayName() const override;
  std::vector<std::string> extensions() const override;
  bool probe(std::string_view head) const override;
  bool importData(std::string_view data, ImportedData &out, std::string &error) const override;
};
}  // namespace digitqt::formats
