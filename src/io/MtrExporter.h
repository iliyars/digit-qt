#pragma once

#include "core/PhaseMap.h"

#include <QString>

namespace digitqt::core::io {

/**
 * @brief Экспорт карты волнового фронта в формат .mtr (WinFringe).
 *
 * Структура файла:
 *   Title=
 *   Date=YYYY-MM-DD
 *   Time=HH:MM:SS
 *   Units=WAV
 *   (пустая строка)
 *   Size=<нечётный диаметр минимальной охватывающей окружности апертуры>
 *   [MATRIX]
 *   <для каждой строки, попадающей в диапазон Y от -1 до 1:>
 *     Y  Z1 X1  Z2 X2  Z3 X3  Z4 X4  Z5 X5  Z6 X6
 *        Z7 X7  ... (продолжение, отступ 7 пробелов, по 6 пар в строке)
 *     ... E
 *   END
 *
 * X, Y нормализованы в [-1, 1] относительно минимальной охватывающей
 * окружности видимых данных карты (не самого изображения!). Z — сама
 * величина карты как есть (никакого домножения на длину волны внутри
 * этой функции — вызывающий код должен передать карту уже в нужных
 * единицах; WinFringe ожидает волны, см. Units=WAV).
 *
 * @param path Путь для сохранения (обычно с расширением .mtr).
 * @param waves Карта для экспорта, уже в единицах волн (нанометры / λ).
 * @param errorMessage Заполняется при неудаче.
 */
bool writeMtrFile(const QString &path, const digitqt::core::PhaseMap &waves, QString &errorMessage);

}  // namespace digitqt::core::io
