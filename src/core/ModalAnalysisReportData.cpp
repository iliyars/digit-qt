#include "ModalAnalysisReportData.h"

#include "core/Measurement.h"
#include "core/ZernikeNormalization.h"

#include <cmath>
#include <limits>

namespace digitqt::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

/// Range of known (non-NaN) values in `map`; false if the map has none.
bool computeRange(const PhaseMap &map, double &outMin, double &outMax) {
  outMin = std::numeric_limits<double>::max();
  outMax = std::numeric_limits<double>::lowest();
  bool any = false;
  for (int y = 0; y < map.height(); ++y) {
    for (int x = 0; x < map.width(); ++x) {
      if (!map.hasValue(x, y))
        continue;
      const double v = map.value(x, y);
      outMin = std::min(outMin, v);
      outMax = std::max(outMax, v);
      any = true;
    }
  }
  return any;
}

}  // namespace

ModalAnalysisReportData buildModalAnalysisReportData(const Measurement &measurement) {
  ModalAnalysisReportData data;
  const auto &modal = measurement.modalAnalysis();
  data.isEmpty = modal.isEmpty();
  if (data.isEmpty)
    return data;

  data.coefficients = modal.coefficients;
  data.selection = modal.selection;
  data.basis = modal.basis;
  data.fitMethod = measurement.modalFitMethod();
  data.sequential = modal.sequential;
  data.wavelengthNm = measurement.wavelengthNm();

  // WinFringe-совместимое отображение (не сам алгоритм подгонки!): на
  // базисе Seregin кома, трилистник и сферическая выходят из МНК РОВНО
  // вдвое МЕНЬШЕ, чем те же термы в test_data/report.txt (проверено на
  // test_data/Terminal.mtr -- Coma 0.250λ у нас vs C=0.500 в отчёте,
  // Spherical 0.250λ у нас vs B4=0.500, оба РОВНО ×2, не "примерно").
  // Астигматизм при этом совпадает без всякого множителя (0.500=0.500) --
  // так что множитель специфичен для термов 3-го/4-го порядка и выше, не
  // общий. Природа множителя не выведена из первых принципов (нет
  // исходников WinFringe, который формирует report.txt, а не сам DAPPSIM)
  // -- чисто эмпирическая подгонка под один референсный файл. Для
  // трилистника отдельной проверки нет (в report.txt он нулевой) --
  // множитель на него перенесён по аналогии с комой (тот же МНК-этап
  // GetComaSeregin), это ПРЕДПОЛОЖЕНИЕ, не подтверждённый факт.
  if (modal.basis == PolynomialBasis::Seregin) {
    data.coefficients.comaX *= 2.0;
    data.coefficients.comaY *= 2.0;
    data.coefficients.trefoilX *= 2.0;
    data.coefficients.trefoilY *= 2.0;
    data.coefficients.spherical *= 2.0;
  } else {
    // Нормировка Цернике -- только отображение (подгонка шла в единичной).
    data.zernikeNormalization = measurement.zernikeNormalization();
    data.coefficients = toZernikeNormalization(data.coefficients, data.zernikeNormalization);
  }
  const auto &c = data.coefficients;

  data.astigMagnitude = std::sqrt(c.astigX * c.astigX + c.astigY * c.astigY);
  data.comaMagnitude = std::sqrt(c.comaX * c.comaX + c.comaY * c.comaY);
  data.trefoilMagnitude = std::sqrt(c.trefoilX * c.trefoilX + c.trefoilY * c.trefoilY);

  // Углы -- одинаково для обоих базисов, по соглашению DAPPSIM: от оси +Y,
  // против часовой стрелки, Y вверх. Берутся из сырых коэффициентов
  // подгонки, приведённых к общему виду (A при X^2-Y^2 и при 2XY,
  // C при X(...) и Y(...), T при X^3-3XY^2 и 3X^2Y-Y^3) по формулам
  // buildSereginHierarchy()/buildZernikeHierarchy(); положительные
  // множители нормировок на угол не влияют. Проверено на двух эталонах:
  // test_data/report.txt (FIA = 45, FIC = -135 -- заодно фиксирует
  // направление вращения: по часовой вышло бы 135) и синтетика
  // test_data/psi_synthetic/test_shift-PHASE (FIA = FIC = 0). Прежние
  // формулы, подогнанные только под первый эталон, давали на втором 90°
  // и различались для комы Seregin/Цернике на 180°. Трилистник эталонами
  // не проверен (в обоих нулевой).
  const auto &raw = modal.coefficients;
  const bool zernike = modal.basis == PolynomialBasis::Zernike;
  const double astigCos = zernike ? raw.astigX : 1.5 * raw.astigX;
  const double astigSin = zernike ? raw.astigY : 0.5 * raw.astigY;
  const double trefoilCos = zernike ? raw.trefoilX : -raw.trefoilX;
  const double trefoilSin = zernike ? raw.trefoilY : -raw.trefoilY;
  // Угол в (-period/2, period/2] с отсчётом от +Y.
  auto fromY = [](double degFromX, double period) {
    double deg = std::fmod(degFromX - 90.0, period);
    if (deg > period / 2.0)
      deg -= period;
    else if (deg <= -period / 2.0)
      deg += period;
    return deg;
  };
  constexpr double kDegPerRad = 180.0 / kPi;
  data.astigAngleDeg = fromY(0.5 * std::atan2(astigSin, astigCos) * kDegPerRad, 180.0);
  data.comaAngleDeg = fromY(std::atan2(raw.comaY, raw.comaX) * kDegPerRad, 360.0);
  data.trefoilAngleDeg = fromY(std::atan2(trefoilSin, trefoilCos) / 3.0 * kDegPerRad, 120.0);

  double minB = 0.0, maxB = 0.0, minA = 0.0, maxA = 0.0;
  if (computeRange(measurement.wavefrontMap(), minB, maxB))
    data.pvBeforeNm = maxB - minB;
  if (computeRange(modal.residual, minA, maxA))
    data.pvAfterNm = maxA - minA;

  data.rmsBeforeNm = modal.rmsBefore;
  data.rmsAfterNm = modal.rmsAfter;

  // rmsAfterNm -- RMS остатка карты ВЫСОТЫ поверхности (см. S4: высота =
  // порядок × λ/2, коэффициент 2 -- двойной проход при отражении).
  // Волновой фронт (OPD) вдвое больше высоты, поэтому для Струма нужен
  // именно он, а не высота напрямую.
  const double rmsWaves = data.wavelengthNm > 0.0 ? data.rmsAfterNm / data.wavelengthNm : 0.0;
  const double rmsWavefrontWaves = 2.0 * rmsWaves;
  data.strehl = std::exp(-std::pow(2.0 * kPi * rmsWavefrontWaves, 2.0));

  return data;
}

}  // namespace digitqt::core
