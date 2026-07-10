#pragma once

#include "core/NumberedFringeLine.h"
#include "core/PhaseMap.h"

#include <QString>
#include <functional>
#include <vector>

namespace digitqt::core::pipeline {

struct PhaseReconstructionParams {
  double cgTolerance = 1e-6;
  int maxIterations = 2000;
};

/**
 * @brief S2: восстанавливает плотную карту фазы (в единицах порядка
 * полосы) из пронумерованных линий полос решением уравнения Лапласа.
 *
 * Идея портирована из оригинального проекта Digit
 * (WavefrontSolver/IsoLinesToTopogram): каждая линия растеризуется на
 * сетку как известное значение (номер полосы — граничное условие
 * Дирихле); для всех остальных пикселей внутри апертуры решается
 * ∇²z = 0 методом сопряжённых градиентов (Eigen), с естественным
 * условием Неймана на краю апертуры (соседи вне апертуры просто не
 * учитываются в разностном шаблоне). Результат — самая гладкая
 * поверхность, точно проходящая через заданные линии; никакого явного
 * "разворачивания фазы" не требуется, так как номер полосы уже и есть
 * развёрнутая величина по построению.
 */
class PhaseReconstructor {
public:
  /**
   * @brief Решить уравнение Лапласа и построить карту фазы.
   * @param width, height Разрешение сетки решения (может быть меньше
   * исходного изображения ради скорости — см. PhaseReconstructionStage,
   * который решает, какое разрешение выбрать).
   * @param isVisible Предикат видимости в координатах этой сетки
   * (0..width-1, 0..height-1).
   * @param lines Пронумерованные линии полос, в координатах этой же сетки.
   */
  PhaseMap reconstruct(int width, int height, const std::function<bool(int, int)> &isVisible,
                       const std::vector<NumberedFringeLine> &lines,
                       const PhaseReconstructionParams &params = PhaseReconstructionParams());

  const QString &lastError() const { return m_lastError; }

private:
  QString m_lastError;
};

}  // namespace digitqt::core::pipeline
