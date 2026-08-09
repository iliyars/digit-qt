#include "core/Measurement.h"
#include "core/ModalAnalysisResult.h"
#include "core/ModalFitMethod.h"
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

/// Полностью заполненная квадратная апертура size x size (без вырезов).
/// ModalAnalysisStage берёт центр/радиус из bounding box непустых пикселей
/// самой карты -- для полного квадрата это совпадает с центром/полушириной
/// квадрата, так что нормализованные координаты каждого пикселя можно
/// предсказать здесь же, не заглядывая во внутренности стадии.
digitqt::core::PhaseMap makeSyntheticWavefront(int size, const SyntheticCoefficients &c,
                                                double rippleAmplitude = 0.0) {
  digitqt::core::PhaseMap map(size, size);
  const double center = (size - 1) / 2.0;
  const double radius = (size - 1) / 2.0;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const double nx = (x - center) / radius;
      const double ny = (y - center) / radius;
      double value = sereginSurface(c, nx, ny);
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
  // AnalyticZernike -- совместный МНК без ортогонализации по базису Seregin
  // -- должен точно восстановить коэффициенты синтетической поверхности,
  // построенной из тех же формул: подпространство базиса полноранговое, а
  // сама поверхность лежит в этом подпространстве целиком, так что
  // единственное решение МНК -- это в точности исходные коэффициенты.
  void exactRecoveryMatchesSereginBasis();

  // AnalyticZernike и GramSchmidtOnAperture подгоняют одно и то же
  // подпространство термов, только по-разному параметризованное -- их
  // проекция на это подпространство (а значит и остаток) должна совпадать
  // даже когда в данных есть компонента вне подпространства (рябь), которую
  // ни один из методов не может убрать полностью.
  void bothFitMethodsAgreeOnResidual();
};

void ModalAnalysisStageTest::exactRecoveryMatchesSereginBasis() {
  const SyntheticCoefficients expected;
  digitqt::core::Measurement measurement;
  measurement.wavefrontMap() = makeSyntheticWavefront(65, expected);
  measurement.modalFitMethod() = digitqt::core::ModalFitMethod::AnalyticZernike;

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

void ModalAnalysisStageTest::bothFitMethodsAgreeOnResidual() {
  const SyntheticCoefficients coeffs;
  const auto wavefront = makeSyntheticWavefront(65, coeffs, /*rippleAmplitude=*/0.05);

  digitqt::core::Measurement analytic;
  analytic.wavefrontMap() = wavefront;
  analytic.modalFitMethod() = digitqt::core::ModalFitMethod::AnalyticZernike;
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

QTEST_MAIN(ModalAnalysisStageTest)
#include "ModalAnalysisStageTest.moc"
