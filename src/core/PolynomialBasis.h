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

  /// Классические полиномы Цернике (Борн–Вольф): defocus = 2ρ²-1,
  /// astigX/Y = ρ²cos2θ/ρ²sin2θ = (X²-Y², 2XY), coma X/Y =
  /// (3ρ³-2ρ)cosθ/sinθ = (3ρ²-2)X/Y, trefoil X/Y = ρ³cos3θ/sin3θ =
  /// (X³-3XY², 3X²Y-Y³), spherical = 6ρ⁴-6ρ²+1. Ось Y -- оптическая
  /// (вверх, Y = -y), как у Seregin, θ -- от +X против часовой стрелки.
  /// Подгонка всегда идёт в единичной нормировке; в какой нормировке
  /// показывать коэффициенты -- отдельный выбор, см. ZernikeNormalization.
  Zernike,
};

/**
 * @brief В какой нормировке показывать коэффициенты Цернике (только при
 * PolynomialBasis::Zernike). Функции одни и те же, отличается только
 * множитель при каждом члене, поэтому смена нормировки не меняет ни
 * подгонку, ни остаток (RMS/PV) -- только числа коэффициентов; S5 не
 * пересчитывается. Перевод -- toZernikeNormalization()
 * (ZernikeNormalization.h). Обзор нормировок -- руководство FastInterf
 * (test_data/zernkike), гл. «Нормировка полиномов Цернике».
 */
enum class ZernikeNormalization {
  /// Значение 1 на краю зрачка (Борн–Вольф, Малакара, CodeV).
  Unit,
  /// Коэффициент при старшей степени ρ равен 1: defocus = ρ²-1/2, кома =
  /// (ρ³-2ρ/3)cosθ, сферическая = ρ⁴-ρ²+1/6 (ISO 14999-2, ОСТ 3-5476-83;
  /// так же нормированы B2/B4/C в отчётах DAPPSIM).
  Iso14999,
  /// Каждый член с RMS = 1 по кругу: множитель √(n+1) при m = 0 и
  /// √(2(n+1)) при m ≠ 0 (ANSI Z80.28, Zemax Standard Zernike).
  Rms,
};

}  // namespace digitqt::core
