#pragma once

#include "core/NumberedFringeLine.h"
#include "core/PhaseMap.h"

#include <aperture/include/visibility/ShapeCollection.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace digitqt::formats {

/// Что произошло с одним куском данных при импорте (см. notes/architecture.md,
/// раздел 11.4 "Import/Export fidelity").
enum class Fidelity {
  Lossless,   ///< перенесено как есть
  Converted,  ///< перенесено, но в другом представлении (сетка, единицы, ...)
  Dropped,    ///< в файле было, в модель не попало
  Missing,    ///< в файле нет, принято значение по умолчанию
};

struct FidelityNote {
  Fidelity kind;
  std::string what;
};

/**
 * @brief Срез канонической модели, который адаптер достал из внешнего файла.
 *
 * Пустое поле значит "в этом файле такого нет", а не ошибку: .mtr заполняет
 * только phaseMap, .frn -- boundaries/fringeLines/imageFileName. Вызывающий
 * код решает, что делать с результатом, по заполненным полям, а не по
 * расширению файла.
 *
 * Только перемещается, не копируется -- из-за aperture::ShapeCollection
 * (фигуры в ней лежат в unique_ptr).
 */
struct ImportedData {
  std::optional<core::PhaseMap> phaseMap;
  std::optional<aperture::ShapeCollection> boundaries;
  std::vector<core::NumberedFringeLine> fringeLines;
  std::string imageFileName;
  /// Размер изображения, к пикселям которого привязаны boundaries и
  /// fringeLines; 0 -- в файле не указан.
  int imageWidth = 0;
  int imageHeight = 0;
  std::vector<FidelityNote> report;
};

/// Сколько первых байт файла вызывающий код передаёт в probe().
inline constexpr std::size_t kProbeBytes = 4096;

/**
 * @brief Один внешний формат файла: как его узнать и как перевести в
 * каноническую модель.
 *
 * Адаптер не трогает файловую систему -- получает содержимое файла уже в
 * памяти. Чтение с диска (и юникодные пути Windows) решается один раз у
 * вызывающего кода, а probe() и importData() работают с одними и теми же
 * байтами.
 */
class IFormatAdapter {
public:
  virtual ~IFormatAdapter() = default;

  /// Имя для пользователя, например "WinFringe Matrix".
  virtual std::string displayName() const = 0;

  /// Расширения без точки, в нижнем регистре. Только подсказка для диалога
  /// выбора файла -- что это за файл на самом деле, решает probe().
  virtual std::vector<std::string> extensions() const = 0;

  /// @param head Начало файла (не больше kProbeBytes байт).
  /// @return true, если содержимое похоже на этот формат.
  virtual bool probe(std::string_view head) const = 0;

  /// @param data Файл целиком.
  /// @param out Заполняется при успехе (включая out.report).
  /// @param error Заполняется при неудаче.
  virtual bool importData(std::string_view data, ImportedData &out, std::string &error) const = 0;
};

}  // namespace digitqt::formats
