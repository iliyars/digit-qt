#pragma once

#include "formats/FormatAdapter.h"

namespace digitqt::formats {

/**
 * @brief Адаптер формата .mtr (WinFringe): текстовая матрица волнового
 * фронта в нормированных координатах [-1, 1], Units=WAV.
 *
 * Qt-свободный наследник io::readMtrFile() (см. io/MtrImporter.h -- там же
 * описано, как нормированные X/Y раскладываются по новой сетке Size×Size).
 * Заполняет только ImportedData::phaseMap.
 */
class MtrAdapter : public IFormatAdapter {
public:
  std::string displayName() const override;
  std::vector<std::string> extensions() const override;
  bool probe(std::string_view head) const override;
  bool importData(std::string_view data, ImportedData &out, std::string &error) const override;
};

}  // namespace digitqt::formats
