#pragma once

namespace digitqt::core {

/**
 * @brief Which family of low-order polynomials S5 fits against -- see
 * ModalAnalysisStage.cpp (buildTermHierarchy) for the exact formulas of
 * both, and notes/uchebnik-interferometriya-i-digitqt.md §7.2 for the
 * derivation connecting them.
 *
 * This is orthogonal to ModalFitMethod (which controls HOW the least
 * squares problem is solved for whichever basis is chosen here): a
 * Seregin fit and a Zernike fit of the SAME wavefront describe the same
 * physical surface, just with different-looking (and differently-scaled)
 * coefficients for the same terms.
 */
enum class PolynomialBasis {
  /// Прямая транскрипция полиномов референсного инструмента (DAPPSIM,
  /// метод Seregin) -- НЕ учебниковые нормированные полиномы Цернике.
  /// Даёт коэффициенты в тех же единицах и с тем же знаком, что и
  /// референсный инструмент (WinFringe/DAPPSIM), включая его оптическую
  /// конвенцию знака Y ("Y растёт вверх" -- см. tiltY = -y в
  /// buildTermHierarchy). Единственная гарантированно совместимая с
  /// существующими .mtr/отчётами WinFringe опция -- используется по
  /// умолчанию.
  Seregin,

  /// Классические (не Gram-Schmidt-ортонормированные, но и не "голые"
  /// мономы, как у Seregin) полиномы Цернике: defocus = 2ρ²-1,
  /// astigX/Y = ρ²cos2θ/ρ²sin2θ = (x²-y², 2xy), coma X/Y =
  /// (3ρ³-2ρ)cosθ/sinθ = (3ρ²-2)x/y, trefoil X/Y = ρ³cos3θ/sin3θ =
  /// (x³-3xy², 3x²y-y³), spherical = 6ρ⁴-6ρ²+1. Знак Y здесь -- сырая
  /// координата сэмпла (растёт вниз по строкам изображения, без
  /// оптического переворота Seregin) -- эта опция существует специально
  /// для сравнения с внешними инструментами/генераторами, которые
  /// используют именно эту, более распространённую в учебниках
  /// конвенцию, а не для совместимости с WinFringe/DAPPSIM.
  Zernike,
};

}  // namespace digitqt::core
