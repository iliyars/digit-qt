#pragma once

#include "core/NumberedFringeLine.h"
#include "core/PhaseMap.h"

#include <QString>
#include <functional>
#include <vector>

namespace digitqt::core::pipeline {

/**
 * @brief S2: восстанавливает плотную карту фазы (в единицах порядка
 * полосы) из пронумерованных линий полос построчной кубической
 * сплайн-интерполяцией.
 *
 * Порт WavefrontFromContoursSolver_HorizontalSpline из оригинального
 * проекта Digit -- именно этот метод (а не решение уравнения Лапласа)
 * реально используется там при экспорте в .mtr. Для каждой строки сетки
 * независимо: ищутся точки пересечения пронумерованных линий с этой
 * горизонталью, через них (по X) строится натуральный кубический
 * сплайн, которым заполняется вся строка, включая экстраполяцию за
 * крайние полосы до края апертуры. Строки друг с другом никак не
 * связаны -- в отличие от глобального Лапласиана это не даёт гладкой по
 * вертикали поверхности, зато локальная частота внутри строки в точности
 * равна измеренной при оцифровке, а не выведенной из условия гладкости.
 * Для интерферограмм с доминирующим горизонтальным наклоном (полосы
 * близки к вертикальным) это даёт при обратном переводе в
 * интерферограмму результат, гораздо ближе к исходному, особенно у края
 * апертуры.
 */
class PhaseReconstructor {
public:
  /**
   * @brief Построить карту фазы построчной сплайн-интерполяцией.
   * @param width, height Разрешение сетки решения (может быть меньше
   * исходного изображения ради скорости -- см. PhaseReconstructionStage).
   * @param isVisible Предикат видимости в координатах этой сетки
   * (0..width-1, 0..height-1).
   * @param lines Пронумерованные линии полос, в координатах этой же сетки.
   */
  PhaseMap reconstruct(int width, int height, const std::function<bool(int, int)> &isVisible,
                       const std::vector<NumberedFringeLine> &lines);

  const QString &lastError() const { return m_lastError; }

private:
  QString m_lastError;
};

}  // namespace digitqt::core::pipeline
