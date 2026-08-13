#pragma once

namespace digitqt::core {

enum class PhaseReconstructionAlgorithm {
  /// Построчная кубическая сплайн-интерполяция по пронумерованным линиям
  /// полос (см. PhaseReconstructor) -- порт
  /// WavefrontFromContoursSolver_HorizontalSpline из референсного Digit.
  /// Требует трассированные линии из Setup (S1).
  HorizontalSpline,

  /// Метод Фурье-анализа полос (Fourier Transform Method, Такеда, 1982,
  /// см. FourierPhaseExtractor) -- извлекает фазу сразу по всей апертуре
  /// одним 2D БПФ, без трассировки отдельных линий. Трассировка в Setup
  /// (S1) для этого метода не нужна и не выполняется.
  FourierTransform,

  /// Метод непрерывного вейвлет-анализа полос (Wavelet Transform
  /// Profilometry, Zhong & Weng, 2004, см. WaveletPhaseExtractor) --
  /// локально-адаптивная альтернатива Фурье-методу: частота ищется по
  /// гребню вейвлет-преобразования отдельно в каждой точке, а не одним
  /// глобальным пиком на всё изображение. Трассировка в Setup (S1) тоже
  /// не нужна.
  WaveletTransform,
};

}  // namespace digitqt::core
