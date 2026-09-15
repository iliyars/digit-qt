#include "core/Measurement.h"
#include "core/ModalAnalysisResult.h"
#include "core/ModalFitMethod.h"
#include "core/PolynomialBasis.h"
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
/// (учебниковые, не Gram-Schmidt) полиномы Цернике, без Seregin-flip оси
/// Y (см. комментарий там же). Продублировано намеренно, по той же
/// причине, что и sereginSurface() выше.
double zernikeSurface(const SyntheticCoefficients &c, double x, double y) {
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

QTEST_MAIN(ModalAnalysisStageTest)
#include "ModalAnalysisStageTest.moc"
