#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace digitqt::formats {

/**
 * Файл-описание серии кадров для фазового сдвига (.idf, "Interferograms
 * descriptive file") -- из программ Д. Серегина. Текст, строки CRLF:
 *
 *   Interferograms descriptive file
 *   <пустая строка>
 *   5            -- число кадров
 *   400          -- ширина кадра
 *   400          -- высота кадра
 *   c33_0.bmp    -- имена кадров в порядке сдвигов, по одному в строке,
 *   ...             относительно папки самого .idf
 *
 * Сдвигов в файле нет: их предполагается находить по самим кадрам.
 *
 * Это не IFormatAdapter: адаптер выдаёт срез одного измерения
 * (ImportedData), а .idf -- только список файлов; читать картинки -- дело
 * вызывающего.
 */
struct IdfDescription {
  int width = 0;
  int height = 0;
  /// Имена как записаны в файле (байты, обычно ASCII; кодировку для
  /// не-ASCII имён -- Windows-1251 у программ Серегина -- выбирает
  /// вызывающий).
  std::vector<std::string> frameFiles;
};

/// Узнаёт .idf по первой строке.
bool probeIdf(std::string_view head);

/// @return false и error, если заголовок не тот, числа не читаются или
///         имён кадров меньше, чем объявлено. Строки после последнего
///         объявленного кадра игнорируются.
bool parseIdf(std::string_view text, IdfDescription &out, std::string &error);

}  // namespace digitqt::formats
