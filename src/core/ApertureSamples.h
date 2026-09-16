#pragma once

#include "core/PhaseMap.h"

#include <vector>

namespace digitqt::core {

/// Центр и радиус апертуры в координатах самой карты (пиксели), не
/// изображения -- "зрачковые координаты", где край апертуры всегда ровно
/// at radius=1, как принято в оптике. См. computeApertureGeometry().
struct ApertureGeometry {
  double centerX = 0.0;
  double centerY = 0.0;
  double radius = 0.0;
};

/// Определяет центр/радиус апертуры из bounding box НЕПУСТЫХ пикселей
/// самой карты (не из Measurement::boundaries() -- те заданы в
/// координатах полного изображения, а карта может храниться на
/// уменьшенной сетке, см. PhaseReconstructionStage). Используется и
/// ModalAnalysisStage (см. buildTermHierarchy/doCompute), и любым
/// внешним инструментом, которому нужна ТА ЖЕ нормализация координат,
/// что видит сама программа (например, сравнение двух .mtr-карт).
///
/// Если карта пуста, возвращает геометрию по размеру самой карты
/// (radius = max(w,h)/2, центр -- геометрический центр) -- тот же
/// консервативный fallback, что был в ModalAnalysisStage раньше.
ApertureGeometry computeApertureGeometry(const PhaseMap &map);

/// Один сэмпл карты внутри апертуры: нормализованные пупильные
/// координаты (x, y в [-1, 1] относительно geometry), значение карты z,
/// и исходные пиксельные координаты (px, py) -- нужны, чтобы разложить
/// подгонку/остаток обратно в PhaseMap той же формы, что и исходная карта.
struct ApertureSample {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  int px = 0;
  int py = 0;
};

/// Собирает сэмплы карты `map`, нормализованные относительно `geometry`,
/// с эрозией края апертуры на `edgeErosionPixels` (см. комментарий в
/// ModalAnalysisStage.cpp про то, почему крайний ободок карты -- самые
/// ненадёжные данные во всей карте). edgeErosionPixels <= 0 отключает
/// эрозию (сэмплируются все непустые пиксели).
std::vector<ApertureSample> collectApertureSamples(const PhaseMap &map,
                                                    const ApertureGeometry &geometry,
                                                    int edgeErosionPixels);

}  // namespace digitqt::core
