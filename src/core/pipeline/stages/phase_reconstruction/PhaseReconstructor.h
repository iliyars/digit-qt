#pragma once

#include "core/Bitmap.h"
#include "core/NumberedFringeLine.h"
#include "core/PhaseMap.h"
#include "core/pipeline/stages/phase_reconstruction/IPhaseReconstructor.h"

#include <functional>
#include <string>
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
class PhaseReconstructor : public digitqt::core::IPhaseReconstructor {
public:
  /**
   * @brief Построить карту фазы построчной сплайн-интерполяцией.
   * @param gridWidth, gridHeight Разрешение сетки решения (может быть
   * меньше исходного изображения ради скорости -- см.
   * PhaseReconstructionStage). image не используется вообще -- этому
   * методу нужна только геометрия линий, не пиксели (см.
   * IPhaseReconstructor::reconstruct()).
   * @param isVisible Предикат видимости в координатах этой сетки
   * (0..gridWidth-1, 0..gridHeight-1).
   * @param lines Пронумерованные линии полос, в координатах этой же сетки.
   */
  PhaseMap reconstruct(int gridWidth, int gridHeight, const digitqt::core::Bitmap &image,
                       std::function<bool(int, int)> isVisible,
                       const std::vector<NumberedFringeLine> &lines) override;

  std::string name() const override { return "Horizontal Spline Interpolation"; }

  const std::string &lastError() const override { return m_lastError; }

private:
  std::string m_lastError;
};

}  // namespace digitqt::core::pipeline
