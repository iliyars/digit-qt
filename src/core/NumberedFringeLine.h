#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

namespace digitqt::core {

/**
 * @brief Трассированная центральная линия полосы с номером порядка.
 *
 * Номер порядка используется  для вычисления фазы
 * (phase = 2*pi * order с интерполяцией между соседними полосами).
 * Должен существовать и иметь одинаковый смысл для любых трассировщиков.
 * Только ScanlineExtremumTracker вычисляет порядок внутри себя (через FringeConstructor);
 * трекеры на основе seed'ов генерируют независимые неупорядоченные линии,
 * поэтому порядок назначается позже (autoAssignFringeOrder()) и может быть
 * скорректирован вручную.
 */
struct NumberedFringeLine {
  tracing::TracedLine points;
  double order = 0.0;
  bool orderIsManual = false;
};
}  // namespace digitqt::core
