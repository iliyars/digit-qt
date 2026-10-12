#include "FormatRegistry.h"

#include "formats/FrnAdapter.h"
#include "formats/MtrAdapter.h"

namespace digitqt::formats {

const std::vector<const IFormatAdapter *> &importAdapters() {
  static const MtrAdapter mtr;
  static const FrnAdapter frn;
  static const std::vector<const IFormatAdapter *> adapters = {&mtr, &frn};
  return adapters;
}

const IFormatAdapter *detectFormat(std::string_view head) {
  for (const IFormatAdapter *adapter : importAdapters())
    if (adapter->probe(head))
      return adapter;
  return nullptr;
}

}  // namespace digitqt::formats
