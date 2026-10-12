#pragma once

#include "formats/FormatAdapter.h"

namespace digitqt::formats {

/// Все адаптеры, умеющие импорт. Живут всё время работы программы.
const std::vector<const IFormatAdapter *> &importAdapters();

/// Адаптер, узнавший файл по содержимому (см. IFormatAdapter::probe()),
/// или nullptr.
/// @param head Начало файла (не больше kProbeBytes байт).
const IFormatAdapter *detectFormat(std::string_view head);

}  // namespace digitqt::formats
