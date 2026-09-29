#pragma once

#include "core/Bitmap.h"
#include "core/PhaseMap.h"
#include "core/pipeline/stages/phase_reconstruction/IPhaseReconstructor.h"

#include <functional>
#include <string>

namespace digitqt::core::pipeline {

/**
 * @brief Метод Фурье-анализа полос (Fourier Transform Method, метод
 * Такеды, 1982) -- альтернатива трассировке отдельных полос: извлекает
 * фазу сразу во всех точках апертуры одним БПФ, без поиска и нумерации
 * отдельных линий.
 *
 * Принцип: 2D БПФ картинки полос даёт центральный (постоянная
 * составляющая) пик плюс два симметричных боковых пика на частоте
 * несущей (наклона). Вырезаем окрестность одного бокового пика, сдвигаем
 * его в центр (демодуляция) и берём обратное БПФ -- результат
 * комплексный, его фаза = arctan(мнимая/реальная) даёт свёрнутую фазу
 * сразу во всех точках. Дальше -- разворачивание (unwrap).
 *
 * Валидировано на синтетических данных с заранее известными
 * коэффициентами: дефокус/астигматизм/кома/трефойл/сферическая
 * восстанавливаются точно. Наклон -- отдельный случай: демодуляция
 * (сдвиг пика в центр) вычитает несущую частоту как часть переноса,
 * поэтому после разворачивания фазы наклон почти полностью пропадает.
 * Восстанавливаем его в явном виде из самого положения найденного пика в
 * спектре и складываем с остатком -- без этого шага результат будет
 * систематически занижен по наклону в десятки раз.
 */
class FourierPhaseExtractor : public digitqt::core::IPhaseReconstructor {
public:
  PhaseMap reconstruct(int gridWidth, int gridHeight, const digitqt::core::Bitmap &image,
                       std::function<bool(int, int)> isVisible,
                       const std::vector<NumberedFringeLine> &lines) override;

  std::string name() const override { return "Fourier Phase Extractor"; }
  const std::string &lastError() const override { return m_lastError; }

private:
  std::string m_lastError;
};

}  // namespace digitqt::core::pipeline
