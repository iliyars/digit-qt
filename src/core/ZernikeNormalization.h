#pragma once

#include "core/ModalAnalysisResult.h"
#include "core/PolynomialBasis.h"

#include <cmath>

namespace digitqt::core {

/**
 * @brief Перевод коэффициентов Цернике из единичной нормировки (в ней
 * идёт подгонка S5) в `normalization`.
 *
 * Если член нормирован как P = N·U (U -- полином с единицей на краю), то
 * волна W = a_U·U = a_N·P, откуда a_N = a_U / N.
 * Для Iso14999 N = 1/C (C -- коэффициент при старшей степени ρ в U),
 * значит a_N = a_U·C; для Rms N = √(n+1) или √(2(n+1)), a_N = a_U / N.
 */
inline ModalCoefficients toZernikeNormalization(const ModalCoefficients &unit,
                                                ZernikeNormalization normalization) {
  ModalCoefficients c = unit;
  switch (normalization) {
    case ZernikeNormalization::Unit:
      break;
    case ZernikeNormalization::Iso14999:
      // Старшие коэффициенты: 2ρ²-1 -> 2, (3ρ³-2ρ) -> 3, 6ρ⁴-6ρ²+1 -> 6;
      // у пистона, наклона, астигматизма и трилистника они равны 1.
      c.defocus *= 2.0;
      c.comaX *= 3.0;
      c.comaY *= 3.0;
      c.spherical *= 6.0;
      break;
    case ZernikeNormalization::Rms: {
      // n = 1 (m = 1), 2 (m = 0), 2 (m = 2), 3 (m = 1), 3 (m = 3), 4 (m = 0).
      const double tilt = 2.0, defocus = std::sqrt(3.0), astig = std::sqrt(6.0),
                   coma = std::sqrt(8.0), trefoil = std::sqrt(8.0), spherical = std::sqrt(5.0);
      c.tiltX /= tilt;
      c.tiltY /= tilt;
      c.defocus /= defocus;
      c.astigX /= astig;
      c.astigY /= astig;
      c.comaX /= coma;
      c.comaY /= coma;
      c.trefoilX /= trefoil;
      c.trefoilY /= trefoil;
      c.spherical /= spherical;
      break;
    }
  }
  return c;
}

}  // namespace digitqt::core
