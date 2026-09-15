#include "ModalAnalysisReportData.h"

#include "core/Measurement.h"

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

  const auto &c = modal.coefficients;
  data.coefficients = c;
  data.selection = modal.selection;
  data.basis = modal.basis;
  data.fitMethod = measurement.modalFitMethod();
  data.sequential = modal.sequential;
  data.wavelengthNm = measurement.wavelengthNm();

  data.astigMagnitude = std::sqrt(c.astigX * c.astigX + c.astigY * c.astigY);
  data.comaMagnitude = std::sqrt(c.comaX * c.comaX + c.comaY * c.comaY);

  // Знак внутри atan2 (не сама величина!) для астигматизма/комы зависит
  // от базиса. На Zernike-базисе (astigX=x²-y², astigY=2xy; comaX/comaY
  // без Seregin-переворота Y) обычный atan2(Y,X) уже даёт правильный
  // угол -- проверено на test_data/report.txt (FIA=45.000 совпало без
  // всякой коррекции). На Seregin-базисе тот же обычный atan2 даёт УГОЛ
  // С ОШИБКОЙ (величина при этом верна): на том же report.txt наш
  // astigmatism выходил -45.0° вместо FIA=45.000, а coma -45.0° вместо
  // FIC=-135.000. Побайтово проверенная поправка (даёт РОВНО 45.000 и
  // -135.000, не "почти"): для астигматизма брать -astigY вместо astigY,
  // для комы -- -comaX вместо comaX. Не выведено из первых принципов
  // (нет доступа к тому, как именно WinFringe считает FIA/FIC) -- чисто
  // эмпирическая подгонка под этот один референсный файл; для трилистника
  // аналогичной проверки нет (в report.txt он нулевой), поэтому его знак
  // НЕ трогаем.
  if (modal.basis == PolynomialBasis::Seregin) {
    data.astigAngleDeg = 0.5 * std::atan2(-c.astigY, c.astigX) * 180.0 / kPi;
    data.comaAngleDeg = std::atan2(c.comaY, -c.comaX) * 180.0 / kPi;
  } else {
    data.astigAngleDeg = 0.5 * std::atan2(c.astigY, c.astigX) * 180.0 / kPi;
    data.comaAngleDeg = std::atan2(c.comaY, c.comaX) * 180.0 / kPi;
  }

  data.trefoilMagnitude = std::sqrt(c.trefoilX * c.trefoilX + c.trefoilY * c.trefoilY);
  data.trefoilAngleDeg = std::atan2(c.trefoilY, c.trefoilX) * 180.0 / kPi / 3.0;

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
