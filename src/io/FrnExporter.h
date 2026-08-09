#pragma once

#include "core/NumberedFringeLine.h"

#include <QString>
#include <vector>

namespace aperture {
class ShapeCollection;
}

namespace digitqt::core::io {

/**
 * @brief Экспорт в WinFringe-совместимый .frn.
 *
 * Порт WinFringe-варианта WriteFRNData() из оригинального Digit
 * (InterfSolver/Tools/ReadWriteData.cpp) -- второй "родной" вариант
 * формата (без переворота/нормализации Y) не реализован.
 *
 * Структура файла:
 *   [GENERAL]              Title=, Date=, Time=, ScaleFactor=, FiScan=
 *   [ELLIPSES] / [RECTANGLES] (если есть)
 *                           фигуры апертуры (EXTERNAL+INTERNAL, APERTURE не
 *                           экспортируется -- в оригинальном формате нет
 *                           такого типа), Y перевёрнут и нормализован на
 *                           минимальную охватывающую окружность видимой
 *                           области (как делает WinFringe)
 *   [POLYGONS] (если есть) те же фигуры-полигоны, только Y перевёрнут
 *   [BOUNDS]                по одной строке на неповёрнутый ellipse/rect,
 *                           Y перевёрнут, БЕЗ нормализации
 *   [FRINGES]               по блоку NFringe=<номер> на каждую
 *                           пронумерованную линию, Y перевёрнут, без
 *                           нормализации
 *   [IMAGE_FILE]             Size=, Name=
 *
 * @param path Путь для сохранения (обычно с расширением .frn).
 * @param boundaries Фигуры апертуры, в пиксельных координатах изображения.
 * @param lines Пронумерованные линии полос, в тех же координатах.
 * @param imageWidth, imageHeight Размер исходного изображения в пикселях.
 * @param imageFileName Имя файла изображения (без пути) для секции
 * [IMAGE_FILE].
 * @param errorMessage Заполняется при неудаче.
 */
bool writeFrnFile(const QString &path, const aperture::ShapeCollection &boundaries,
                  const std::vector<digitqt::core::NumberedFringeLine> &lines,
                  int imageWidth, int imageHeight, const QString &imageFileName,
                  QString &errorMessage);

}  // namespace digitqt::core::io
