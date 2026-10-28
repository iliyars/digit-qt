#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/ModalAnalysisResult.h"
#include "core/ModalFitMethod.h"
#include "core/PolynomialBasis.h"
#include "core/ZernikeNormalization.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"

#include <QtTest/QtTest>

#include <cmath>

namespace {

/// Синтетический набор коэффициентов (нм), используемый как известный
/// "правильный ответ" для проверки подгонки.
struct SyntheticCoefficients {
  double piston = 5.0;
  double tiltX = 2.0;
  double tiltY = -1.5;
  double defocus = 3.0;
  double astigX = 0.7;
  double astigY = -0.4;
  double comaX = 1.1;
  double comaY = -0.9;
  double trefoilX = 0.6;
  double trefoilY = -0.3;
  double spherical = 0.25;
};

/// Точная транскрипция базиса термов из
/// core/pipeline/stages/ModalAnalysisStage.cpp (buildTermHierarchy), в свою
/// очередь -- транскрипция полиномов Seregin из DAPPSIM (Includes/Fitting.cpp:
/// GetTiltSeregin/GetPowerSeregin/GetAstigSeregin/GetComaSeregin/
/// GetS3Seregin). Продублировано здесь намеренно: тест ловит именно
/// расхождение между этими формулами и тем, что реально стоит в
/// buildTermHierarchy() -- если один файл поправят, а второй забудут, тест
/// покраснеет.
double sereginSurface(const SyntheticCoefficients &c, double x, double y) {
  const double r2 = x * x + y * y;
  return c.piston                                                //
         + c.tiltX * x + c.tiltY * (-y)                           //
         + c.defocus * r2                                         //
         + c.astigX * (1.5 * (x * x - y * y))                     //
         + c.astigY * (-x * y)                                    //
         + c.comaX * (x * x * x + x * y * y - (2.0 / 3.0) * x)    //
         + c.comaY * ((2.0 / 3.0) * y - y * r2)                   //
         + c.trefoilX * (3.0 * x * y * y - x * x * x)             //
         + c.trefoilY * (3.0 * x * x * y - y * y * y)             //
         + c.spherical * (r2 * r2);
}

/// Точная транскрипция buildZernikeHierarchy() -- классические
/// (учебниковые, не Gram-Schmidt) полиномы Цернике в единичной нормировке,
/// с оптической осью Y вверх (Y = -y, как у Seregin). Продублировано
/// намеренно, по той же причине, что и sereginSurface() выше.
double zernikeSurface(const SyntheticCoefficients &c, double x, double rowY) {
  const double y = -rowY;
  const double r2 = x * x + y * y;
  return c.piston                                   //
         + c.tiltX * x + c.tiltY * y                 //
         + c.defocus * (2.0 * r2 - 1.0)               //
         + c.astigX * (x * x - y * y)                //
         + c.astigY * (2.0 * x * y)                  //
         + c.comaX * ((3.0 * r2 - 2.0) * x)          //
         + c.comaY * ((3.0 * r2 - 2.0) * y)          //
         + c.trefoilX * (x * x * x - 3.0 * x * y * y) //
         + c.trefoilY * (3.0 * x * x * y - y * y * y) //
         + c.spherical * (6.0 * r2 * r2 - 6.0 * r2 + 1.0);
}

/// Полностью заполненная квадратная апертура size x size (без вырезов).
/// ModalAnalysisStage берёт центр/радиус из bounding box непустых пикселей
/// самой карты -- для полного квадрата это совпадает с центром/полушириной
/// квадрата, так что нормализованные координаты каждого пикселя можно
/// предсказать здесь же, не заглядывая во внутренности стадии.
digitqt::core::PhaseMap makeSyntheticWavefront(
    int size, const SyntheticCoefficients &c, double rippleAmplitude = 0.0,
    digitqt::core::PolynomialBasis basis = digitqt::core::PolynomialBasis::Seregin) {
  digitqt::core::PhaseMap map(size, size);
  const double center = (size - 1) / 2.0;
  const double radius = (size - 1) / 2.0;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const double nx = (x - center) / radius;
      const double ny = (y - center) / radius;
      double value = basis == digitqt::core::PolynomialBasis::Zernike
                         ? zernikeSurface(c, nx, ny)
                         : sereginSurface(c, nx, ny);
      if (rippleAmplitude != 0.0)
        value += rippleAmplitude * std::sin(5.0 * nx) * std::cos(3.0 * ny);
      map.setValue(x, y, value);
    }
  }
  return map;
}

}  // namespace

class ModalAnalysisStageTest : public QObject {
  Q_OBJECT

private slots:
  // JointLeastSquares -- совместный МНК без ортогонализации по базису Seregin
  // -- должен точно восстановить коэффициенты синтетической поверхности,
  // построенной из тех же формул: подпространство базиса полноранговое, а
  // сама поверхность лежит в этом подпространстве целиком, так что
  // единственное решение МНК -- это в точности исходные коэффициенты.
  void exactRecoveryMatchesSereginBasis();

  // То же самое, но для PolynomialBasis::Zernike -- та же логика точного
  // восстановления, другой (учебниковый) базис.
  void exactRecoveryMatchesZernikeBasis();

  // JointLeastSquares и GramSchmidtOnAperture подгоняют одно и то же
  // подпространство термов, только по-разному параметризованное -- их
  // проекция на это подпространство (а значит и остаток) должна совпадать
  // даже когда в данных есть компонента вне подпространства (рябь), которую
  // ни один из методов не может убрать полностью.
  void bothFitMethodsAgreeOnResidual();

  void sequentialSereginDoesNotCrash();

  // RMS -- СКО относительно среднего: прибавка константы к карте (она
  // произвольна после развёртки) не меняет ни "до", ни "после".
  void rmsIgnoresMapConstant();

  // Нормировки Цернике -- только масштаб коэффициентов: коэффициент,
  // переведённый в нормировку, умноженный на полином этой нормировки,
  // должен давать ту же волну, что и в единичной; плюс известные
  // множители FastInterf/ISO (B2 = 2·Z20, C = 3·Z31, B4 = 6·Z40).
  void zernikeNormalizationsDescribeSameWavefront();

  // Нормировка влияет только на отображение (ModalAnalysisReportData), не
  // на результат подгонки и не на углы.
  void zernikeNormalizationIsDisplayOnly();

  // Углы астигматизма/комы/трилистника -- свойство самой волны: подгонка
  // в базисе Seregin и в базисе Цернике должна давать одни и те же углы.
  void anglesAgreeBetweenBases();

  // Соглашение об углах -- как в DAPPSIM: от +Y против часовой, Y вверх.
  // Кома вдоль +Y -- 0°, вдоль +X -- -90°; астигматизм cos 2θ (ось X) --
  // 90°, sin 2θ (ось 45°) -- -45°.
  void anglesFollowDappsimConvention();
};

void ModalAnalysisStageTest::exactRecoveryMatchesSereginBasis() {
  const SyntheticCoefficients expected;
  digitqt::core::Measurement measurement;
  measurement.wavefrontMap() = makeSyntheticWavefront(65, expected);
  measurement.modalFitMethod() = digitqt::core::ModalFitMethod::JointLeastSquares;

  digitqt::core::pipeline::ModalAnalysisStage stage;
  QVERIFY(stage.compute(measurement));

  const auto &fitted = measurement.modalAnalysis().coefficients;
  const double tol = 1e-8;
  QVERIFY(std::abs(fitted.piston - expected.piston) < tol);
  QVERIFY(std::abs(fitted.tiltX - expected.tiltX) < tol);
  QVERIFY(std::abs(fitted.tiltY - expected.tiltY) < tol);
  QVERIFY(std::abs(fitted.defocus - expected.defocus) < tol);
  QVERIFY(std::abs(fitted.astigX - expected.astigX) < tol);
  QVERIFY(std::abs(fitted.astigY - expected.astigY) < tol);
  QVERIFY(std::abs(fitted.comaX - expected.comaX) < tol);
  QVERIFY(std::abs(fitted.comaY - expected.comaY) < tol);
  QVERIFY(std::abs(fitted.trefoilX - expected.trefoilX) < tol);
  QVERIFY(std::abs(fitted.trefoilY - expected.trefoilY) < tol);
  QVERIFY(std::abs(fitted.spherical - expected.spherical) < tol);

  QVERIFY(measurement.modalAnalysis().rmsAfter < tol);
}

void ModalAnalysisStageTest::exactRecoveryMatchesZernikeBasis() {
  const SyntheticCoefficients expected;
  digitqt::core::Measurement measurement;
  measurement.wavefrontMap() =
      makeSyntheticWavefront(65, expected, /*rippleAmplitude=*/0.0,
                             digitqt::core::PolynomialBasis::Zernike);
  measurement.modalFitMethod() = digitqt::core::ModalFitMethod::JointLeastSquares;
  measurement.polynomialBasis() = digitqt::core::PolynomialBasis::Zernike;

  digitqt::core::pipeline::ModalAnalysisStage stage;
  QVERIFY(stage.compute(measurement));

  const auto &fitted = measurement.modalAnalysis().coefficients;
  const double tol = 1e-8;
  QVERIFY(std::abs(fitted.piston - expected.piston) < tol);
  QVERIFY(std::abs(fitted.tiltX - expected.tiltX) < tol);
  QVERIFY(std::abs(fitted.tiltY - expected.tiltY) < tol);
  QVERIFY(std::abs(fitted.defocus - expected.defocus) < tol);
  QVERIFY(std::abs(fitted.astigX - expected.astigX) < tol);
  QVERIFY(std::abs(fitted.astigY - expected.astigY) < tol);
  QVERIFY(std::abs(fitted.comaX - expected.comaX) < tol);
  QVERIFY(std::abs(fitted.comaY - expected.comaY) < tol);
  QVERIFY(std::abs(fitted.trefoilX - expected.trefoilX) < tol);
  QVERIFY(std::abs(fitted.trefoilY - expected.trefoilY) < tol);
  QVERIFY(std::abs(fitted.spherical - expected.spherical) < tol);

  QVERIFY(measurement.modalAnalysis().rmsAfter < tol);
  QVERIFY(measurement.modalAnalysis().basis == digitqt::core::PolynomialBasis::Zernike);
}

void ModalAnalysisStageTest::bothFitMethodsAgreeOnResidual() {
  const SyntheticCoefficients coeffs;
  const auto wavefront = makeSyntheticWavefront(65, coeffs, /*rippleAmplitude=*/0.05);

  digitqt::core::Measurement analytic;
  analytic.wavefrontMap() = wavefront;
  analytic.modalFitMethod() = digitqt::core::ModalFitMethod::JointLeastSquares;
  digitqt::core::pipeline::ModalAnalysisStage analyticStage;
  QVERIFY(analyticStage.compute(analytic));

  digitqt::core::Measurement gramSchmidt;
  gramSchmidt.wavefrontMap() = wavefront;
  gramSchmidt.modalFitMethod() = digitqt::core::ModalFitMethod::GramSchmidtOnAperture;
  digitqt::core::pipeline::ModalAnalysisStage gramSchmidtStage;
  QVERIFY(gramSchmidtStage.compute(gramSchmidt));

  QVERIFY(std::abs(analytic.modalAnalysis().rmsBefore - gramSchmidt.modalAnalysis().rmsBefore) < 1e-9);
  QVERIFY(std::abs(analytic.modalAnalysis().rmsAfter - gramSchmidt.modalAnalysis().rmsAfter) < 1e-8);
}

void ModalAnalysisStageTest::rmsIgnoresMapConstant() {
  const SyntheticCoefficients coeffs;
  const auto wavefront = makeSyntheticWavefront(65, coeffs, /*rippleAmplitude=*/0.05);
  auto shifted = wavefront;
  for (int y = 0; y < shifted.height(); ++y)
    for (int x = 0; x < shifted.width(); ++x)
      if (shifted.hasValue(x, y))
        shifted.setValue(x, y, shifted.value(x, y) + 1000.0);

  digitqt::core::Measurement a;
  a.wavefrontMap() = wavefront;
  digitqt::core::pipeline::ModalAnalysisStage stageA;
  QVERIFY(stageA.compute(a));

  digitqt::core::Measurement b;
  b.wavefrontMap() = shifted;
  digitqt::core::pipeline::ModalAnalysisStage stageB;
  QVERIFY(stageB.compute(b));

  QVERIFY(std::abs(a.modalAnalysis().rmsBefore - b.modalAnalysis().rmsBefore) < 1e-8);
  QVERIFY(std::abs(a.modalAnalysis().rmsAfter - b.modalAnalysis().rmsAfter) < 1e-8);
  QVERIFY(a.modalAnalysis().rmsBefore > a.modalAnalysis().rmsAfter);
}

void ModalAnalysisStageTest::sequentialSereginDoesNotCrash() {
  const SyntheticCoefficients coeffs;
  digitqt::core::Measurement measurement;
  measurement.wavefrontMap() = makeSyntheticWavefront(65, coeffs);
  measurement.modalFitMethod() = digitqt::core::ModalFitMethod::SequentialSeregin;

  digitqt::core::pipeline::ModalAnalysisStage stage;
  QVERIFY(stage.compute(measurement));

  // ВАЖНО: в отличие от JointLeastSquares (exactRecoveryMatchesSereginBasis
  // выше), последовательная схема НЕ обязана точно восстановить piston/
  // tiltX/tiltY -- они подгоняются на самой первой стадии, ДО того как
  // убраны дефокус/астигматизм/кома/сферическая, поэтому неизбежно
  // "цепляют" часть их вклада (та же неортогональность термов, из-за
  // которой сам DAPPSIM даёт другие числа в Sequential vs Joint режиме --
  // см. ModalFitMethod::SequentialSeregin). Проверяем то, что
  // действительно гарантировано: конечный остаток должен сойтись к ~0 на
  // данных, которые целиком лежат в 22-мерном пространстве термов схемы
  // (наша синтетика построена ровно из этих формул) -- если он застревает
  // на заметной величине, это значит, что где-то теряется вклад
  // отброшенного интерсепта одной из стадий 2-7 (см. комментарий в
  // fitSequentialSeregin и SequentialSereginResult::discardedIntercepts).
  QVERIFY(measurement.modalAnalysis().rmsAfter < 1e-6);
}

void ModalAnalysisStageTest::zernikeNormalizationsDescribeSameWavefront() {
  using digitqt::core::ZernikeNormalization;
  digitqt::core::ModalCoefficients unit;
  unit.piston = 0.5;
  unit.tiltX = 2.0;
  unit.tiltY = -1.5;
  unit.defocus = 3.0;
  unit.astigX = 0.7;
  unit.astigY = -0.4;
  unit.comaX = 1.1;
  unit.comaY = -0.9;
  unit.trefoilX = 0.6;
  unit.trefoilY = -0.3;
  unit.spherical = 0.25;

  const auto iso = digitqt::core::toZernikeNormalization(unit, ZernikeNormalization::Iso14999);
  QVERIFY(std::abs(iso.defocus - 2.0 * unit.defocus) < 1e-12);
  QVERIFY(std::abs(iso.comaX - 3.0 * unit.comaX) < 1e-12);
  QVERIFY(std::abs(iso.spherical - 6.0 * unit.spherical) < 1e-12);
  QVERIFY(std::abs(iso.astigX - unit.astigX) < 1e-12);
  QVERIFY(std::abs(iso.tiltY - unit.tiltY) < 1e-12);

  const auto rms = digitqt::core::toZernikeNormalization(unit, ZernikeNormalization::Rms);

  // Полиномы нормировок как функции от (ρ, θ), выписанные независимо от
  // toZernikeNormalization() (формулы -- руководство FastInterf); пистон
  // во всех нормировках один и тот же, поэтому сравниваем без него.
  auto wave = [](const digitqt::core::ModalCoefficients &c, ZernikeNormalization n, double r,
                 double t) {
    const double r2 = r * r;
    double tilt = 1.0, def = 1.0, ast = 1.0, com = 1.0, tre = 1.0, sph = 1.0;
    double defR = 2.0 * r2 - 1.0, comR = 3.0 * r2 * r - 2.0 * r, sphR = 6.0 * r2 * r2 - 6.0 * r2 + 1.0;
    if (n == ZernikeNormalization::Iso14999) {
      defR = r2 - 0.5;
      comR = r2 * r - 2.0 * r / 3.0;
      sphR = r2 * r2 - r2 + 1.0 / 6.0;
    } else if (n == ZernikeNormalization::Rms) {
      tilt = 2.0;
      def = std::sqrt(3.0);
      ast = std::sqrt(6.0);
      com = std::sqrt(8.0);
      tre = std::sqrt(8.0);
      sph = std::sqrt(5.0);
    }
    return tilt * r * (c.tiltX * std::cos(t) + c.tiltY * std::sin(t)) + def * c.defocus * defR +
           ast * r2 * (c.astigX * std::cos(2 * t) + c.astigY * std::sin(2 * t)) +
           com * comR * (c.comaX * std::cos(t) + c.comaY * std::sin(t)) +
           tre * r2 * r * (c.trefoilX * std::cos(3 * t) + c.trefoilY * std::sin(3 * t)) +
           sph * c.spherical * sphR;
  };
  for (const auto &[normalization, coeffs] :
       {std::pair{ZernikeNormalization::Iso14999, iso}, std::pair{ZernikeNormalization::Rms, rms}}) {
    for (double r : {0.3, 0.7, 1.0}) {
      for (double t : {0.0, 1.0, 2.5, 4.0}) {
        const double expected = wave(unit, ZernikeNormalization::Unit, r, t) -
                                wave(unit, ZernikeNormalization::Unit, 0.0, 0.0);
        const double actual =
            wave(coeffs, normalization, r, t) - wave(coeffs, normalization, 0.0, 0.0);
        QVERIFY2(std::abs(expected - actual) < 1e-12, qPrintable(QString::number(r)));
      }
    }
  }
}

void ModalAnalysisStageTest::zernikeNormalizationIsDisplayOnly() {
  const SyntheticCoefficients coeffs;
  digitqt::core::Measurement measurement;
  measurement.wavefrontMap() = makeSyntheticWavefront(65, coeffs, /*rippleAmplitude=*/0.0,
                                                      digitqt::core::PolynomialBasis::Zernike);
  measurement.polynomialBasis() = digitqt::core::PolynomialBasis::Zernike;
  digitqt::core::pipeline::ModalAnalysisStage stage;
  QVERIFY(stage.compute(measurement));

  const auto unitData = digitqt::core::buildModalAnalysisReportData(measurement);
  measurement.zernikeNormalization() = digitqt::core::ZernikeNormalization::Rms;
  const auto rmsData = digitqt::core::buildModalAnalysisReportData(measurement);

  QVERIFY(std::abs(measurement.modalAnalysis().coefficients.defocus - coeffs.defocus) < 1e-8);
  QVERIFY(rmsData.zernikeNormalization == digitqt::core::ZernikeNormalization::Rms);
  QVERIFY(std::abs(rmsData.coefficients.defocus - coeffs.defocus / std::sqrt(3.0)) < 1e-8);
  QVERIFY(std::abs(rmsData.astigAngleDeg - unitData.astigAngleDeg) < 1e-9);
  QVERIFY(std::abs(rmsData.comaAngleDeg - unitData.comaAngleDeg) < 1e-9);
  QVERIFY(std::abs(rmsData.trefoilAngleDeg - unitData.trefoilAngleDeg) < 1e-9);
  QVERIFY(std::abs(rmsData.rmsAfterNm - unitData.rmsAfterNm) < 1e-12);
}

void ModalAnalysisStageTest::anglesAgreeBetweenBases() {
  const SyntheticCoefficients coeffs;
  const auto wavefront = makeSyntheticWavefront(65, coeffs, /*rippleAmplitude=*/0.05);

  digitqt::core::Measurement seregin;
  seregin.wavefrontMap() = wavefront;
  digitqt::core::pipeline::ModalAnalysisStage sereginStage;
  QVERIFY(sereginStage.compute(seregin));

  digitqt::core::Measurement zernike;
  zernike.wavefrontMap() = wavefront;
  zernike.polynomialBasis() = digitqt::core::PolynomialBasis::Zernike;
  digitqt::core::pipeline::ModalAnalysisStage zernikeStage;
  QVERIFY(zernikeStage.compute(zernike));

  const auto s = digitqt::core::buildModalAnalysisReportData(seregin);
  const auto z = digitqt::core::buildModalAnalysisReportData(zernike);
  QVERIFY(std::abs(s.astigAngleDeg - z.astigAngleDeg) < 1e-6);
  QVERIFY(std::abs(s.comaAngleDeg - z.comaAngleDeg) < 1e-6);
  QVERIFY(std::abs(s.trefoilAngleDeg - z.trefoilAngleDeg) < 1e-6);
}

void ModalAnalysisStageTest::anglesFollowDappsimConvention() {
  struct Case {
    SyntheticCoefficients coeffs;
    double astigDeg;
    double comaDeg;
  };
  auto pure = [](double astigX, double astigY, double comaX, double comaY) {
    SyntheticCoefficients c{};
    c.piston = c.tiltX = c.tiltY = c.defocus = c.trefoilX = c.trefoilY = c.spherical = 0.0;
    c.astigX = astigX;
    c.astigY = astigY;
    c.comaX = comaX;
    c.comaY = comaY;
    return c;
  };
  const Case cases[] = {
      {pure(1.0, 0.0, 0.0, 1.0), 90.0, 0.0},
      {pure(0.0, 1.0, 1.0, 0.0), -45.0, -90.0},
      {pure(0.0, -1.0, 1.0, -1.0), 45.0, -135.0},  // как в test_data/report.txt
  };
  for (const auto basis : {digitqt::core::PolynomialBasis::Seregin, digitqt::core::PolynomialBasis::Zernike}) {
    for (const auto &c : cases) {
      digitqt::core::Measurement measurement;
      measurement.wavefrontMap() = makeSyntheticWavefront(65, c.coeffs, /*rippleAmplitude=*/0.0,
                                                          digitqt::core::PolynomialBasis::Zernike);
      measurement.polynomialBasis() = basis;
      digitqt::core::pipeline::ModalAnalysisStage stage;
      QVERIFY(stage.compute(measurement));
      const auto data = digitqt::core::buildModalAnalysisReportData(measurement);
      QVERIFY2(std::abs(data.astigAngleDeg - c.astigDeg) < 1e-6, qPrintable(QString::number(data.astigAngleDeg)));
      QVERIFY2(std::abs(data.comaAngleDeg - c.comaDeg) < 1e-6, qPrintable(QString::number(data.comaAngleDeg)));
    }
  }
}

QTEST_MAIN(ModalAnalysisStageTest)
#include "ModalAnalysisStageTest.moc"
