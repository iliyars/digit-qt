#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/ModalAnalysisResult.h"
#include "core/ModalFitMethod.h"
#include "core/PolynomialBasis.h"
#include "core/ZernikeNormalization.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"
#include "core/pipeline/stages/WavefrontReconstructionStage.h"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

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
  void strehlUsesComputedPassMode_data();
  void strehlUsesComputedPassMode();
  void strehlIsIndependentOfSurfaceOrOpdRepresentation();
  void degenerateFitsFailWithoutReplacingResults_data();
  void degenerateFitsFailWithoutReplacingResults();
  void validRestrictedAperturesStillFit_data();
  void validRestrictedAperturesStillFit();
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

void ModalAnalysisStageTest::strehlUsesComputedPassMode_data() {
  QTest::addColumn<double>("wavelength");
  QTest::addColumn<double>("rms");
  QTest::addColumn<bool>("doublePass");
  QTest::addColumn<double>("expected");
  QTest::newRow("single-pass") << 1000.0 << 50.0 << false << 0.9060180558;
  QTest::newRow("double-pass") << 1000.0 << 50.0 << true << 0.6738254512;
  QTest::newRow("equivalent-height") << 1000.0 << 25.0 << true << 0.9060180558;
  QTest::newRow("different-wavelength") << 2000.0 << 100.0 << false << 0.9060180558;
  QTest::newRow("perfect-single") << 1000.0 << 0.0 << false << 1.0;
  QTest::newRow("perfect-double") << 1000.0 << 0.0 << true << 1.0;
}

void ModalAnalysisStageTest::strehlUsesComputedPassMode() {
  QFETCH(double, wavelength);
  QFETCH(double, rms);
  QFETCH(bool, doublePass);
  QFETCH(double, expected);
  digitqt::core::Measurement measurement;
  auto &result = measurement.modalAnalysis();
  result.residual = digitqt::core::PhaseMap(1, 1);
  result.residual.setValue(0, 0, 0);
  result.rmsAfter = rms;
  result.parameters = digitqt::core::ModalAnalysisResult::ComputationParameters{
      wavelength, doublePass, digitqt::core::ModalFitMethod::JointLeastSquares, 0.0};
  // Editing the controls after S5 must not change the existing report.
  measurement.setWavelengthNm(wavelength * 2);
  measurement.setDoublePass(!doublePass);
  const auto data = digitqt::core::buildModalAnalysisReportData(measurement);
  QVERIFY(std::abs(data.strehl - expected) < 1e-9);
}

void ModalAnalysisStageTest::strehlIsIndependentOfSurfaceOrOpdRepresentation() {
  const auto phase = makeSyntheticWavefront(65, SyntheticCoefficients{}, 0.02);
  double strehlSinglePass = 0.0;
  double rmsSinglePass = 0.0;
  for (bool doublePass : {false, true}) {
    digitqt::core::Measurement measurement;
    measurement.setImportedPhaseMap(phase);
    measurement.setWavelengthNm(1000);
    measurement.setDoublePass(doublePass);
    digitqt::core::pipeline::WavefrontReconstructionStage wavefront;
    digitqt::core::pipeline::ModalAnalysisStage modal;
    QVERIFY(wavefront.compute(measurement));
    QVERIFY(modal.compute(measurement));
    const auto data = digitqt::core::buildModalAnalysisReportData(measurement);
    if (!doublePass) {
      strehlSinglePass = data.strehl;
      rmsSinglePass = data.rmsAfterNm;
      QVERIFY(data.strehl > 0.0 && data.strehl < 1.0);
    } else {
      QVERIFY(std::abs(data.rmsAfterNm * 2 - rmsSinglePass) < 1e-9);
      QVERIFY(std::abs(data.strehl - strehlSinglePass) < 1e-12);
    }
  }
}

void ModalAnalysisStageTest::degenerateFitsFailWithoutReplacingResults_data() {
  QTest::addColumn<int>("method");
  QTest::addColumn<int>("basis");
  QTest::addColumn<QString>("geometry");
  for (int method = 0; method < 3; ++method) {
    for (int basis = 0; basis < 2; ++basis) {
      for (const auto *geometry : {"line", "diagonal", "circle", "three-columns", "empty",
                                   "eroded", "too-few", "infinity"}) {
        const auto name = QStringLiteral("method-%1-basis-%2-%3").arg(method).arg(basis).arg(geometry);
        QTest::newRow(qPrintable(name)) << method << basis << QString::fromLatin1(geometry);
      }
      if (method == static_cast<int>(digitqt::core::ModalFitMethod::SequentialSeregin)) {
        const auto name = QStringLiteral("method-%1-basis-%2-four-radii").arg(method).arg(basis);
        QTest::newRow(qPrintable(name)) << method << basis << QStringLiteral("four-radii");
      }
    }
  }
}

void ModalAnalysisStageTest::degenerateFitsFailWithoutReplacingResults() {
  QFETCH(int, method);
  QFETCH(int, basis);
  QFETCH(QString, geometry);
  using namespace digitqt::core;
  Measurement measurement;
  measurement.modalFitMethod() = static_cast<ModalFitMethod>(method);
  measurement.polynomialBasis() = static_cast<PolynomialBasis>(basis);
  measurement.setEdgeErosionPixels(0);
  PhaseMap map(33, 33);
  if (geometry == "line" || geometry == "eroded") {
    for (int x = 0; x < 33; ++x) map.setValue(x, 16, x*x);
    if (geometry == "eroded") measurement.setEdgeErosionPixels(1);
  } else if (geometry == "diagonal") {
    for (int x = 0; x < 33; ++x) map.setValue(x, x, x*x);
  } else if (geometry == "circle") {
    map = PhaseMap(131, 131);
    int count = 0;
    for (int y = 0; y < 131; ++y) {
      for (int x = 0; x < 131; ++x) {
        if ((x-65)*(x-65) + (y-65)*(y-65) == 65*65) {
          map.setValue(x, y, x + y);
          ++count;
        }
      }
    }
    QVERIFY(count >= 22);  // enough samples, but radius-dependent terms are ambiguous
  } else if (geometry == "three-columns") {
    for (int y = 0; y < 33; ++y)
      for (int x = 15; x <= 17; ++x) map.setValue(x, y, x*x + y*y);
  } else if (geometry == "four-radii") {
    map = PhaseMap(35, 35);
    for (int y = 0; y < 35; ++y) {
      for (int x = 0; x < 35; ++x) {
        const int radiusSquared = (x-17)*(x-17) + (y-17)*(y-17);
        if (radiusSquared == 25 || radiusSquared == 100 ||
            radiusSquared == 169 || radiusSquared == 289)
          map.setValue(x, y, x + y);
      }
    }
  } else if (geometry == "too-few") {
    for (int y = 15; y <= 16; ++y)
      for (int x = 15; x <= 16; ++x) map.setValue(x, y, 1);
  } else if (geometry == "infinity") {
    map = makeSyntheticWavefront(17, SyntheticCoefficients{});
    // Even outside the eroded fit domain an infinite value would corrupt report PV.
    map.setValue(0, 0, std::numeric_limits<double>::infinity());
    measurement.setEdgeErosionPixels(2);
  }
  measurement.wavefrontMap() = std::move(map);
  auto &previous = measurement.modalAnalysis();
  previous.residual = PhaseMap(1, 1);
  previous.residual.setValue(0, 0, 42);
  previous.coefficients.piston = 123;
  previous.rmsAfter = 456;
  pipeline::ModalAnalysisStage stage;
  stage.markComputed();
  QVERIFY(!stage.compute(measurement));
  QCOMPARE(stage.status(), pipeline::StageStatus::Error);
  QVERIFY(!stage.errorMessage().empty());
  QCOMPARE(previous.residual.value(0, 0), 42.0);
  QCOMPARE(previous.coefficients.piston, 123.0);
  QCOMPARE(previous.rmsAfter, 456.0);
  if (geometry == "circle" && method == static_cast<int>(ModalFitMethod::SequentialSeregin))
    QVERIFY(stage.errorMessage().find("defocus") != std::string::npos);
  if (geometry == "four-radii")
    QVERIFY(stage.errorMessage().find("S7") != std::string::npos);
}

void ModalAnalysisStageTest::validRestrictedAperturesStillFit_data() {
  QTest::addColumn<int>("method");
  QTest::addColumn<int>("basis");
  QTest::addColumn<QString>("geometry");
  for (int method = 0; method < 3; ++method) {
    for (int basis = 0; basis < 2; ++basis) {
      for (const auto *geometry : {"narrow", "constant", "piston-line", "piston-point"}) {
        if (method == static_cast<int>(digitqt::core::ModalFitMethod::SequentialSeregin) &&
            QString::fromLatin1(geometry).startsWith("piston"))
          continue;  // this method always fits every term, regardless of checkboxes
        const auto name = QStringLiteral("method-%1-basis-%2-%3").arg(method).arg(basis).arg(geometry);
        QTest::newRow(qPrintable(name)) << method << basis << QString::fromLatin1(geometry);
      }
    }
  }
}

void ModalAnalysisStageTest::validRestrictedAperturesStillFit() {
  QFETCH(int, method);
  QFETCH(int, basis);
  QFETCH(QString, geometry);
  using namespace digitqt::core;
  Measurement measurement;
  measurement.modalFitMethod() = static_cast<ModalFitMethod>(method);
  measurement.polynomialBasis() = static_cast<PolynomialBasis>(basis);
  measurement.setEdgeErosionPixels(0);
  auto map = makeSyntheticWavefront(33, SyntheticCoefficients{}, 0,
                                    static_cast<PolynomialBasis>(basis));
  if (geometry == "narrow") {
    for (int y = 0; y < 33; ++y)
      for (int x = 0; x < 33; ++x)
        if (x < 14 || x > 18) map.setValue(x, y, std::numeric_limits<double>::quiet_NaN());
  } else if (geometry.startsWith("piston")) {
    measurement.modalTermSelection() = {false, false, false, false, false, false};
    map = PhaseMap(33, 33);
    if (geometry == "piston-line") {
      for (int x = 0; x < 33; ++x) map.setValue(x, 16, 2);
    } else {
      map.setValue(16, 16, 2);
    }
  } else {
    for (int y = 0; y < 33; ++y)
      for (int x = 0; x < 33; ++x) map.setValue(x, y, 2);
  }
  measurement.wavefrontMap() = std::move(map);
  pipeline::ModalAnalysisStage stage;
  QVERIFY2(stage.compute(measurement), stage.errorMessage().c_str());
  const auto &result = measurement.modalAnalysis();
  QVERIFY(!result.isEmpty());
  QVERIFY(std::isfinite(result.rmsAfter));
  // The legacy sequential scheme need not remove a mixed surface exactly on
  // a non-circular aperture; this case checks that valid geometry is accepted.
  if (geometry != "narrow" || method != static_cast<int>(ModalFitMethod::SequentialSeregin))
    QVERIFY(result.rmsAfter < 1e-8);
}

QTEST_MAIN(ModalAnalysisStageTest)
#include "ModalAnalysisStageTest.moc"
