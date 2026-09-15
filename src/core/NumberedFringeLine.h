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

  /// How many points at the front/back of `points` were produced by the
  /// edge-extrapolation tools (see FringeEdgeExtrapolation.h) rather than
  /// by the tracer or by hand -- i.e. are not a direct measurement, only
  /// a same-step continuation of the last real data. removeFringeExtensions()
  /// uses these to strip exactly the auto-generated points back off
  /// (dropping the whole line if that empties it -- see there for why a
  /// fully-synthetic line can't instead be a second NumberedFringeLine
  /// for the same physical fringe).
  int syntheticFrontCount = 0;
  int syntheticBackCount = 0;

  bool isSynthetic() const { return syntheticFrontCount > 0 || syntheticBackCount > 0; }
};
}  // namespace digitqt::core
