#pragma once

#include "core/Bitmap.h"
#include "core/PhaseMap.h"
#include "core/pipeline/stages/phase_reconstruction/IPhaseReconstructor.h"

#include <functional>
#include <string>

namespace digitqt::core::pipeline {

/**
 * @brief Реконструкция фазы методом непрерывного вейвлет-анализа
 * (Wavelet Transform Profilometry, Zhong & Weng, 2004) -- альтернатива
 * и трассировке линий, и глобальному Фурье-методу.
 *
 * В отличие от FourierPhaseExtractor (одно глобальное БПФ и одна
 * несущая частота на всё изображение), здесь для каждой строки и
 * каждой точки x строится непрерывное вейвлет-преобразование сигнала
 * яркости комплексным вейвлетом Морле на наборе масштабов вокруг
 * ожидаемого периода полос; локальная мгновенная частота находится как
 * "гребень" -- масштаб, на котором модуль коэффициента максимален, а
 * фаза берётся из аргумента коэффициента на этом масштабе. Это даёт
 * локально-адаптивную оценку частоты: там, где полосы из-за аберраций
 * заметно меняют период вдоль строки, гребень следует за этим
 * изменением точка за точкой, а не усредняет его по всей строке/
 * изображению, как это делает один глобальный пик БПФ.
 *
 * Трассировка полос (S1) не нужна -- метод работает прямо по
 * изображению и маске апертуры, как и Фурье-метод.
 */
class WaveletPhaseExtractor : public digitqt::core::IPhaseReconstructor {
public:
  PhaseMap reconstruct(int gridWidth, int gridHeight, const digitqt::core::Bitmap &image,
                       std::function<bool(int, int)> isVisible,
                       const std::vector<NumberedFringeLine> &lines) override;

  std::string name() const override { return "Wavelet Phase Extractor"; }
  const std::string &lastError() const override { return m_lastError; }

private:
  std::string m_lastError;
};

}  // namespace digitqt::core::pipeline
