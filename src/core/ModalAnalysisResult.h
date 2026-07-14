#pragma once

#include "core/PhaseMap.h"

namespace digitqt::core {

/**
 * @brief Коэффициенты низкочастотной раскладки волнового фронта.
 *
 * Базис (все члены -- простые полиномы от нормализованных пупильных
 * координат x, y в [-1, 1], без ортогонализации в духе полиномов
 * Цернике -- каждый добавленный термин просто ещё одна колонка в том же
 * МНК; корреляция между термами МНК разрешает сам):
 *   1                       -- пистон
 *   x, y                    -- наклон X/Y
 *   x²+y²                   -- дефокус
 *   x²-y², xy               -- астигматизм (0°/45° составляющие)
 *   x³+xy², y³+x²y          -- кома (X/Y составляющие)
 *   x³-3xy², 3x²y-y³        -- трилистник (X/Y составляющие)
 *   (x²+y²)²                -- сферическая аберрация 3-го порядка
 *
 * Значения — в тех же единицах, что и исходная карта (нанометры для
 * Measurement::wavefrontMap()).
 *
 * По мотивам классической раскладки аберраций в оригинальном проекте
 * Digit (InterfSolver/Tools/ClassicAberr.h: SPH_COEFF = пистон+наклон+
 * дефокус, AST_COEFF = астигматизм, COM_COEFF = кома), через обычный
 * МНК на Eigen, без переноса их собственных Matrix/Vector классов.
 */
struct ModalCoefficients {
  double piston = 0.0;
  double tiltX = 0.0;
  double tiltY = 0.0;
  double defocus = 0.0;
  double astigX = 0.0;     // x²-y² составляющая
  double astigY = 0.0;     // xy составляющая
  double comaX = 0.0;      // x³+xy² составляющая
  double comaY = 0.0;      // y³+x²y составляющая
  double trefoilX = 0.0;   // x³-3xy² составляющая
  double trefoilY = 0.0;   // 3x²y-y³ составляющая
  double spherical = 0.0;  // (x²+y²)² составляющая
};

/// Результат S5: коэффициенты + остаток (карта минус подогнанный полином)
/// -- это и есть "настоящие" особенности формы поверхности, без вклада
/// геометрии измерения (наклон/дефокус/астигматизм).
struct ModalAnalysisResult {
  ModalCoefficients coefficients;
  PhaseMap residual;
  double rmsBefore = 0.0;
  double rmsAfter = 0.0;

  bool isEmpty() const { return residual.isEmpty(); }

  void clear() {
    coefficients = ModalCoefficients();
    residual.clear();
    rmsBefore = 0.0;
    rmsAfter = 0.0;
  }
};

}  // namespace digitqt::core
