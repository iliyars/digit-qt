#pragma once

#include "core/PhaseMap.h"

#include <QString>

namespace digitqt::core::io {

/**
 * @brief Импорт карты фазы из формата .mtr (WinFringe), обратный к
 * writeMtrFile() из MtrExporter.h.
 *
 * Файл не хранит исходные пиксельные координаты (только X/Y, нормированные
 * в [-1, 1] относительно охватывающей окружности апертуры) -- поэтому
 * восстановленная карта не привязана ни к какому реальному изображению.
 * Она размещается в новой квадратной сетке размера Size×Size (как записано
 * в заголовке файла), с апертурой, отцентрованной по этой сетке: pixel
 * col = round(xNorm / ratio + (Size-1)/2), аналогично для строки (с тем же
 * инвертированием Y, что и при экспорте).
 *
 * Значения Z читаются как есть (единицы волн, Units=WAV) -- ровно то, что
 * writeMtrFile() кладёт в файл из Measurement::phaseMap() без домножения на
 * длину волны, так что импортированную карту можно напрямую положить в
 * Measurement::phaseMap() (см. Measurement::setImportedPhaseMap()).
 *
 * @param path Путь к .mtr файлу.
 * @param outPhase Заполняется восстановленной картой при успехе.
 * @param errorMessage Заполняется при неудаче.
 */
bool readMtrFile(const QString &path, digitqt::core::PhaseMap &outPhase, QString &errorMessage);

}  // namespace digitqt::core::io
