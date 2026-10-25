#include "core/ImagePadding.h"
#include "core/Measurement.h"
#include "core/PhaseUnwrapping.h"
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/PipelineStageId.h"
#include "core/psi/PhaseShifting.h"

#include <aperture/include/geometry/Ellipse.h>

#include <QtTest/QtTest>

#include <cmath>

namespace {

using digitqt::core::Bitmap;
using digitqt::core::psi::CosineFit;
using digitqt::core::psi::PsiResult;

const double kPi = std::acos(-1.0);
const int kSize = 128;

double radians(double degrees) { return degrees * kPi / 180.0; }
double degrees(double radians) { return radians * 180.0 / kPi; }

/// Разность углов, свёрнутая в (-pi, pi].
double angleDiff(double a, double b) {
  return std::remainder(a - b, 2.0 * kPi);
}

bool insideAperture(int x, int y) {
  const double dx = x - (kSize - 1) / 2.0, dy = y - (kSize - 1) / 2.0;
  return dx * dx + dy * dy <= 60.0 * 60.0;
}

/// "Настоящая" фаза тестового волнового фронта: наклон (около 6 полос
/// поперёк поля) плюс немного дефокусировки.
double truePhase(int x, int y) {
  const double u = (x - (kSize - 1) / 2.0) / 60.0, v = (y - (kSize - 1) / 2.0) / 60.0;
  return 2.0 * kPi * (3.0 * u + 1.0 * v + 0.7 * (u * u + v * v));
}

/// Серия кадров I = a + b*cos(phi + delta_n), квантованная в 8 бит -- как
/// в статье (256 уровней). a = 120, b = 100, т.е. модуляция 0.833.
std::vector<Bitmap> makeFrames(const std::vector<double> &shiftsDeg) {
  std::vector<Bitmap> frames;
  for (const double shiftDeg : shiftsDeg) {
    Bitmap frame(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
      for (int x = 0; x < kSize; ++x)
        frame.setPixel(x, y, static_cast<uint8_t>(std::lround(
                                 120.0 + 100.0 * std::cos(truePhase(x, y) + radians(shiftDeg)))));
    frames.push_back(std::move(frame));
  }
  return frames;
}

/// Как makeFrames(), но сдвиг кадра n -- плоскость: shiftsDeg[n] +
/// tiltX[n]*u + tiltY[n]*v (рад на пиксель, u, v -- от центра кадра).
std::vector<Bitmap> makeTiltedFrames(const std::vector<double> &shiftsDeg,
                                     const std::vector<double> &tiltX,
                                     const std::vector<double> &tiltY) {
  std::vector<Bitmap> frames;
  for (size_t n = 0; n < shiftsDeg.size(); ++n) {
    Bitmap frame(kSize, kSize);
    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        const double shift = radians(shiftsDeg[n]) + tiltX[n] * (x - (kSize - 1) / 2.0) +
                             tiltY[n] * (y - (kSize - 1) / 2.0);
        frame.setPixel(x, y, static_cast<uint8_t>(std::lround(
                                 120.0 + 100.0 * std::cos(truePhase(x, y) + shift))));
      }
    }
    frames.push_back(std::move(frame));
  }
  return frames;
}

std::vector<double> toRadians(const std::vector<double> &shiftsDeg) {
  std::vector<double> result;
  for (const double shiftDeg : shiftsDeg)
    result.push_back(radians(shiftDeg));
  return result;
}

double phaseErrorRms(const PsiResult &result, double sign);

/// Лучшая достижимая ошибка фазы на этих кадрах: расчёт с НАСТОЯЩИМИ
/// сдвигами. Остаётся только квантование в 8 бит, и оно тем сильнее
/// усиливается, чем хуже сдвиги разнесены по кругу (у 0/350/250/330 --
/// втрое сильнее, чем у пяти сдвигов рис. 4).
double bestPossibleRms(const std::vector<double> &trueShiftsDeg) {
  PsiResult reference;
  std::string error;
  digitqt::core::psi::computePhase(makeFrames(trueShiftsDeg), toRadians(trueShiftsDeg),
                                   insideAperture, reference, error);
  return phaseErrorRms(reference, 1.0);
}

/// СКО ошибки фазы по апертуре, в радианах. Постоянная составляющая
/// ошибки вычитается: общий сдвиг всей карты на константу -- не ошибка
/// формы.
/// sign = -1 -- сравнение с зеркальной фазой -phi.
double phaseErrorRms(const PsiResult &result, double sign) {
  std::vector<double> errors;
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x)
      if (insideAperture(x, y))
        errors.push_back(angleDiff(result.wrappedPhase.value(x, y), sign * truePhase(x, y)));
  double mean = 0.0;
  for (const double e : errors)
    mean += e / errors.size();
  double sumSq = 0.0;
  for (const double e : errors)
    sumSq += (e - mean) * (e - mean);
  return std::sqrt(sumSq / errors.size());
}

// Сдвиги первого примера из статьи (рис. 4).
const std::vector<double> kArticleShiftsDeg = {0.0, 72.0, 198.0, 266.4, 36.0};
// Наклоны сдвига по x для тех же пяти кадров, рад на пиксель: до 4/120 --
// 2 рад (треть полосы) поперёк апертуры.
const std::vector<double> kTiltX = {0.0, 1.0 / 120, 2.0 / 120, 3.0 / 120, 4.0 / 120};

class PhaseShiftingTest : public QObject {
  Q_OBJECT

private slots:
  void cosineFitRecoversKnownCosine();
  void cosineFitRejectsDegenerateInput();
  void computePhaseWithTrueShifts();
  void computePhaseWithWrongShiftsIsVisiblyWorse();
  void computePhaseRejectsBadInput();
  void estimateShiftsFromTruePhase();
  void shiftPointsSkipDarkNoise();
  void diagnosticsOfCleanFrames();
  void diagnosticsFlagTiltBetweenFrames();
  void tiltSolverRecoversTiltedSeries();
  void tiltSolverOnUntiltedFrames();
  void solveRecoversArticleExample_data();
  void solveRecoversArticleExample();
  void solveReportsNonConvergence();
  void knownShiftsSolverSkipsRefinement();
  void mirrorAmbiguityOfOutOfOrderExample();
  void unwrapRecoversSmoothPhase();
  void unwrapRoutesAroundLowQualityNoise();
  void unwrapCountsDisconnectedRegions();
  void pipelineStageComputesPhaseFromSeries();
  void pipelineStageRequiresSeries();
  void pipelineStageWithoutRefinementUsesNominalShifts();
  void pipelineStageReportsTiltOfSourceFrames();
};

void PhaseShiftingTest::cosineFitRecoversKnownCosine() {
  // I = 50 + 20*cos(theta + 1.1): a = 50, c = 20*cos(1.1), s = 20*sin(1.1).
  CosineFit fit;
  for (const double theta : {0.0, 0.9, 2.0, 3.3, 4.1, 5.5})
    fit.add(theta, 50.0 + 20.0 * std::cos(theta + 1.1));

  double a = 0.0, c = 0.0, s = 0.0;
  QVERIFY(fit.solve(a, c, s));
  QVERIFY(std::fabs(a - 50.0) < 1e-9);
  QVERIFY(std::fabs(std::atan2(s, c) - 1.1) < 1e-9);
  QVERIFY(std::fabs(std::sqrt(c * c + s * s) - 20.0) < 1e-9);
}

void PhaseShiftingTest::cosineFitRejectsDegenerateInput() {
  double a = 0.0, c = 0.0, s = 0.0;
  CosineFit tooFew;
  tooFew.add(0.0, 1.0);
  tooFew.add(1.0, 2.0);
  QVERIFY(!tooFew.solve(a, c, s));

  CosineFit sameAngle;  // три уравнения, но все с одним и тем же theta
  for (int i = 0; i < 3; ++i)
    sameAngle.add(0.5, 10.0 + i);
  QVERIFY(!sameAngle.solve(a, c, s));
}

void PhaseShiftingTest::computePhaseWithTrueShifts() {
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::computePhase(makeFrames(kArticleShiftsDeg),
                                            toRadians(kArticleShiftsDeg), insideAperture, result,
                                            error),
           error.c_str());

  QCOMPARE(result.wrappedPhase.width(), kSize);
  QVERIFY(!result.wrappedPhase.hasValue(0, 0));  // вне апертуры
  QVERIFY(result.wrappedPhase.hasValue(kSize / 2, kSize / 2));

  // Единственный источник ошибки -- квантование в 8 бит.
  const double rms = phaseErrorRms(result, 1.0);
  qInfo("phase error RMS with true shifts: %.5f rad = %.5f waves", rms, rms / (2.0 * kPi));
  QVERIFY(rms < 0.005);

  const double m = result.modulation.value(kSize / 2, kSize / 2);
  QVERIFY(std::fabs(m - 100.0 / 120.0) < 0.01);
}

void PhaseShiftingTest::computePhaseWithWrongShiftsIsVisiblyWorse() {
  // Те же кадры, но расчёт с "паспортными" сдвигами по 90 градусов --
  // ситуация рис. 5 статьи. Это и есть проблема, которую решает шаг 2.
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::computePhase(makeFrames(kArticleShiftsDeg),
                                            toRadians({0.0, 90.0, 180.0, 270.0, 360.0}),
                                            insideAperture, result, error),
           error.c_str());
  const double rms = phaseErrorRms(result, 1.0);
  qInfo("phase error RMS with nominal 90-degree shifts: %.5f rad = %.5f waves", rms,
        rms / (2.0 * kPi));
  QVERIFY(rms > 0.05);
}

void PhaseShiftingTest::computePhaseRejectsBadInput() {
  PsiResult result;
  std::string error;
  const auto frames = makeFrames(kArticleShiftsDeg);

  QVERIFY(!digitqt::core::psi::computePhase({frames[0], frames[1]}, toRadians({0.0, 90.0}),
                                            insideAperture, result, error));
  QVERIFY(!digitqt::core::psi::computePhase(frames, toRadians({0.0, 90.0, 180.0}), insideAperture,
                                            result, error));
  // Все сдвиги одинаковы -- три уравнения, но одно и то же.
  QVERIFY(!digitqt::core::psi::computePhase(frames, toRadians({10.0, 10.0, 10.0, 10.0, 10.0}),
                                            insideAperture, result, error));
  std::vector<Bitmap> mixed = {frames[0], frames[1], Bitmap(16, 16)};
  QVERIFY(!digitqt::core::psi::computePhase(mixed, toRadians({0.0, 90.0, 180.0}), insideAperture,
                                            result, error));
}

void PhaseShiftingTest::estimateShiftsFromTruePhase() {
  // Фаза посчитана с настоящими сдвигами -> шаг 2 обязан вернуть эти же
  // сдвиги (с точностью квантования).
  const auto frames = makeFrames(kArticleShiftsDeg);
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::computePhase(frames, toRadians(kArticleShiftsDeg), insideAperture,
                                            result, error),
           error.c_str());

  const auto points =
      digitqt::core::psi::selectShiftEstimationPoints(frames, insideAperture, 0.3);
  std::vector<double> shifts;
  QVERIFY2(digitqt::core::psi::estimateShifts(frames, result, points, shifts, error),
           error.c_str());
  QCOMPARE(shifts.size(), kArticleShiftsDeg.size());
  QCOMPARE(shifts[0], 0.0);
  for (size_t n = 0; n < shifts.size(); ++n) {
    qInfo("frame %d: estimated %.3f deg, true %.1f deg", static_cast<int>(n + 1),
          degrees(shifts[n]), kArticleShiftsDeg[n]);
    QVERIFY(shifts[n] >= 0.0 && shifts[n] < 2.0 * kPi);
    QVERIFY(std::fabs(degrees(angleDiff(shifts[n], radians(kArticleShiftsDeg[n])))) < 0.1);
  }

  // Ни одной точки -- ошибка, а не сдвиги из ничего.
  const digitqt::core::psi::PointMask none(points.size(), 0);
  QVERIFY(!digitqt::core::psi::estimateShifts(frames, result, none, shifts, error));
}

void PhaseShiftingTest::shiftPointsSkipDarkNoise() {
  // Правая треть кадра -- тёмная (фон 3 уровня) и без полос, только шум
  // +-2 уровня: b/a там легко больше 0.3, и прежний отбор по модуляции
  // брал эти точки в оценку сдвигов. Отбор по размаху яркости -- нет.
  auto frames = makeFrames(kArticleShiftsDeg);
  unsigned state = 12345;
  for (auto &frame : frames) {
    for (int y = 0; y < kSize; ++y) {
      for (int x = 2 * kSize / 3; x < kSize; ++x) {
        state = state * 1103515245u + 12345u;
        frame.setPixel(x, y, static_cast<uint8_t>(1 + (state >> 16) % 5));
      }
    }
  }
  const auto points = digitqt::core::psi::selectShiftEstimationPoints(frames, insideAperture, 0.3);
  int inDark = 0, inFringes = 0;
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x)
      if (points[static_cast<size_t>(y) * kSize + x])
        ++(x >= 2 * kSize / 3 ? inDark : inFringes);
  QCOMPARE(inDark, 0);
  QVERIFY(inFringes > 1000);

  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithUnknownShifts(
               frames, toRadians({0.0, 90.0, 180.0, 270.0, 360.0}), insideAperture,
               digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());
  QVERIFY(result.converged);
  for (size_t n = 0; n < kArticleShiftsDeg.size(); ++n)
    QVERIFY(std::fabs(degrees(angleDiff(result.shifts[n], radians(kArticleShiftsDeg[n])))) < 0.2);
}

void PhaseShiftingTest::diagnosticsOfCleanFrames() {
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithUnknownShifts(
               makeFrames(kArticleShiftsDeg), toRadians({0.0, 90.0, 180.0, 270.0, 360.0}),
               insideAperture, digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());
  const auto &d = result.diagnostics;
  qInfo("points %d, halves disagree %.3f deg, residual %.3f of amplitude %.2f", d.shiftPoints,
        d.halvesDisagreementDeg, d.residual, d.amplitude);
  QVERIFY(d.shiftPoints > 1000);
  QVERIFY(d.halvesDisagreementDeg < 0.5);
  QVERIFY(d.residual < 1.0);                   // только квантование
  QVERIFY(std::fabs(d.amplitude - 100.0) < 2.0);  // b из makeFrames()
}

void PhaseShiftingTest::diagnosticsFlagTiltBetweenFrames() {
  // Сдвиг кадра n меняется поперёк поля: наклон n/120 рад на пиксель (до
  // 2 рад на ширину апертуры). Один сдвиг на кадр это не описывает --
  // половины поля должны разойтись, а модель -- заметно не сойтись с кадрами.
  const auto frames =
      makeTiltedFrames(kArticleShiftsDeg, kTiltX, std::vector<double>(kTiltX.size(), 0.0));
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithUnknownShifts(
               frames, toRadians({0.0, 90.0, 180.0, 270.0, 360.0}), insideAperture,
               digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());
  const auto &d = result.diagnostics;
  qInfo("halves disagree %.2f deg, residual %.2f of amplitude %.2f", d.halvesDisagreementDeg,
        d.residual, d.amplitude);
  QVERIFY(d.halvesDisagreementDeg > 10.0);
  QVERIFY(d.residual > 0.1 * d.amplitude);
}

void PhaseShiftingTest::tiltSolverRecoversTiltedSeries() {
  // Наклоны по x и по y сразу, сдвиги рис. 4; ожидаются 90-градусные шаги.
  const std::vector<double> tiltY = {0.0, -1.5 / 120, 1.0 / 120, -0.5 / 120, 2.0 / 120};
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithTiltShifts(
               makeTiltedFrames(kArticleShiftsDeg, kTiltX, tiltY),
               toRadians({0.0, 90.0, 180.0, 270.0, 360.0}), insideAperture,
               digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());
  QVERIFY(result.converged);
  QCOMPARE(result.tiltX.size(), kTiltX.size());
  // Ошибка наклона, переведённая в радианы на краю апертуры (радиус 60).
  for (size_t n = 0; n < kTiltX.size(); ++n) {
    qInfo("frame %d: shift %.3f deg (true %.1f), tilt x %.5f (%.5f), y %.5f (%.5f) rad/px",
          static_cast<int>(n + 1), degrees(result.shifts[n]), kArticleShiftsDeg[n],
          result.tiltX[n], kTiltX[n], result.tiltY[n], tiltY[n]);
    QVERIFY(std::fabs(degrees(angleDiff(result.shifts[n], radians(kArticleShiftsDeg[n])))) < 0.5);
    QVERIFY(std::fabs(result.tiltX[n] - kTiltX[n]) * 60.0 < 0.01);
    QVERIFY(std::fabs(result.tiltY[n] - tiltY[n]) * 60.0 < 0.01);
  }
  const double rms = phaseErrorRms(result, 1.0);
  qInfo("phase error RMS %.5f rad, residual %.3f, halves %.3f deg", rms,
        result.diagnostics.residual, result.diagnostics.halvesDisagreementDeg);
  QVERIFY(rms < 0.01);
  QVERIFY(result.diagnostics.residual < 1.0);
  QVERIFY(result.diagnostics.halvesDisagreementDeg < 0.5);
}

void PhaseShiftingTest::tiltSolverOnUntiltedFrames() {
  // Наклона нет -- решатель с наклоном не должен его выдумать.
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithTiltShifts(
               makeFrames(kArticleShiftsDeg), toRadians({0.0, 90.0, 180.0, 270.0, 360.0}),
               insideAperture, digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());
  for (size_t n = 0; n < kArticleShiftsDeg.size(); ++n) {
    QVERIFY(std::fabs(degrees(angleDiff(result.shifts[n], radians(kArticleShiftsDeg[n])))) < 0.2);
    QVERIFY(std::fabs(result.tiltX[n]) * 60.0 < 0.01);
    QVERIFY(std::fabs(result.tiltY[n]) * 60.0 < 0.01);
  }
  QVERIFY(phaseErrorRms(result, 1.0) < bestPossibleRms(kArticleShiftsDeg) * 1.05 + 1e-4);
}

void PhaseShiftingTest::solveRecoversArticleExample_data() {
  QTest::addColumn<std::vector<double>>("trueShiftsDeg");
  // Оба примера статьи; ожидаемые сдвиги в обоих -- по 90 градусов.
  QTest::newRow("5 frames, uneven steps (fig. 4)") << kArticleShiftsDeg;
  // Пример рис. 8 со знаком сдвигов, при котором настоящее решение ближе
  // к ожидаемым 90-градусным шагам, чем зеркальное (см.
  // mirrorAmbiguityOfOutOfOrderExample() про исходные знаки статьи).
  QTest::newRow("4 frames, out of order (fig. 8, mirrored)")
      << std::vector<double>{0.0, 350.0, 250.0, 330.0};
}

void PhaseShiftingTest::solveRecoversArticleExample() {
  QFETCH(std::vector<double>, trueShiftsDeg);
  std::vector<double> expectedDeg;
  for (size_t n = 0; n < trueShiftsDeg.size(); ++n)
    expectedDeg.push_back(90.0 * n);

  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithUnknownShifts(
               makeFrames(trueShiftsDeg), toRadians(expectedDeg), insideAperture,
               digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());

  QVERIFY(result.converged);
  const double rms = phaseErrorRms(result, 1.0);
  qInfo("%d iterations%s, phase error RMS %.5f waves", result.iterations,
        result.mirrored ? " (mirror solution replaced)" : "", rms / (2.0 * kPi));
  for (size_t n = 0; n < trueShiftsDeg.size(); ++n) {
    qInfo("frame %d: %.3f deg (true %.1f)", static_cast<int>(n + 1), degrees(result.shifts[n]),
          trueShiftsDeg[n]);
    QVERIFY(std::fabs(degrees(angleDiff(result.shifts[n], radians(trueShiftsDeg[n])))) < 0.2);
  }
  // Не хуже, чем с настоящими сдвигами: найденные сдвиги не добавляют
  // ошибки сверх квантования.
  const double best = bestPossibleRms(trueShiftsDeg);
  qInfo("best possible (true shifts): %.5f waves", best / (2.0 * kPi));
  QVERIFY(rms < best * 1.05 + 1e-4);
}

void PhaseShiftingTest::solveReportsNonConvergence() {
  digitqt::core::psi::PsiOptions options;
  options.maxIterations = 1;
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithUnknownShifts(
               makeFrames(kArticleShiftsDeg), toRadians({0.0, 90.0, 180.0, 270.0, 360.0}),
               insideAperture, options, result, error),
           error.c_str());
  QVERIFY(!result.converged);
  QCOMPARE(result.iterations, 1);
  QVERIFY(!result.wrappedPhase.isEmpty());
}

void PhaseShiftingTest::knownShiftsSolverSkipsRefinement() {
  // Те же кадры рис. 4, сдвиги не уточняются. С настоящими сдвигами --
  // точный результат, с номинальными 90 градусов -- рябь (как в
  // computePhaseWithWrongShiftsIsVisiblyWorse()), и она видна по невязке.
  const auto frames = makeFrames(kArticleShiftsDeg);
  const digitqt::core::psi::PsiOptions options;
  std::string error;

  PsiResult exact;
  QVERIFY2(digitqt::core::psi::solveWithKnownShifts(frames, toRadians(kArticleShiftsDeg),
                                                    insideAperture, options, exact, error),
           error.c_str());
  QCOMPARE(exact.iterations, 0);
  QVERIFY(exact.converged);
  QVERIFY(!exact.mirrored);
  QVERIFY(phaseErrorRms(exact, 1.0) < 0.005);

  PsiResult nominal;
  QVERIFY2(digitqt::core::psi::solveWithKnownShifts(
               frames, toRadians({90.0, 180.0, 270.0, 360.0, 450.0}), insideAperture, options,
               nominal, error),
           error.c_str());
  // Сдвиги приведены к shifts[0] == 0: 90..450 -> 0, 90, 180, 270, 0.
  QCOMPARE(nominal.shifts.size(), kArticleShiftsDeg.size());
  QVERIFY(std::fabs(nominal.shifts[0]) < 1e-12);
  QVERIFY(std::fabs(degrees(nominal.shifts[3]) - 270.0) < 1e-9);
  QVERIFY(phaseErrorRms(nominal, 1.0) > 0.05);
  qInfo("residual with true shifts %.3f, with nominal %.3f grey levels",
        exact.diagnostics.residual, nominal.diagnostics.residual);
  QVERIFY(exact.diagnostics.residual < 1.0);
  QVERIFY(nominal.diagnostics.residual > 10.0 * exact.diagnostics.residual);
}

void PhaseShiftingTest::mirrorAmbiguityOfOutOfOrderExample() {
  // Рис. 8 статьи буквально: настоящие 0/10/110/30, ожидаемые 0/90/180/270.
  // Зеркальное решение (0/350/250/330, фаза -phi) описывает кадры ровно так
  // же хорошо и при этом ближе к ожидаемым (суммарно 230 градусов против
  // 270) -- по самим кадрам их не различить, и алгоритм выбирает его.
  // Статья сообщает сходимость к 0/10.1/110/30; скорее всего, у
  // генератора кадров (WinFringe) знак сдвига противоположен нашему
  // I = a + b*cos(phi + delta) -- тогда это ровно наш результат.
  PsiResult result;
  std::string error;
  QVERIFY2(digitqt::core::psi::solveWithUnknownShifts(
               makeFrames({0.0, 10.0, 110.0, 30.0}), toRadians({0.0, 90.0, 180.0, 270.0}),
               insideAperture, digitqt::core::psi::PsiOptions{}, result, error),
           error.c_str());
  QVERIFY(result.converged);
  const std::vector<double> mirroredDeg = {0.0, 350.0, 250.0, 330.0};
  for (size_t n = 0; n < mirroredDeg.size(); ++n) {
    qInfo("frame %d: %.3f deg", static_cast<int>(n + 1), degrees(result.shifts[n]));
    QVERIFY(std::fabs(degrees(angleDiff(result.shifts[n], radians(mirroredDeg[n])))) < 0.2);
  }
  // Форма восстановлена так же точно, как в прямой постановке, -- но с
  // обратным знаком.
  QVERIFY(phaseErrorRms(result, -1.0) < bestPossibleRms({0.0, 10.0, 110.0, 30.0}) * 1.05 + 1e-4);
}


/// Свёрнутая в (-pi, pi] настоящая фаза внутри апертуры, NaN снаружи.
digitqt::core::PhaseMap wrappedTruePhase(double scale) {
  digitqt::core::PhaseMap wrapped(kSize, kSize);
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x)
      if (insideAperture(x, y))
        wrapped.setValue(x, y, std::remainder(scale * truePhase(x, y), 2.0 * kPi));
  return wrapped;
}

/// Наибольшее отклонение развёрнутой карты (в порядках) от настоящей фазы
/// по точкам, где pick(x, y) -- после вычитания общей константы, взятой в
/// точке (kSize/2, kSize/4): внутри апертуры, но вне полосы шума из
/// unwrapRoutesAroundLowQualityNoise() (в самом центре брать нельзя).
double maxUnwrapError(const digitqt::core::PhaseMap &orders, double scale,
                      const std::function<bool(int, int)> &pick) {
  const int rx = kSize / 2, ry = kSize / 4;
  const double offset = orders.value(rx, ry) - scale * truePhase(rx, ry) / (2.0 * kPi);
  double worst = 0.0;
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x)
      if (pick(x, y))
        worst = std::max(worst, std::fabs(orders.value(x, y) -
                                          scale * truePhase(x, y) / (2.0 * kPi) - offset));
  return worst;
}

void PhaseShiftingTest::unwrapRecoversSmoothPhase() {
  // Втрое круче, чем в кадрах: около 20 полос поперёк апертуры.
  digitqt::core::UnwrapResult result;
  std::string error;
  QVERIFY2(digitqt::core::unwrapPhase(wrappedTruePhase(3.0), digitqt::core::PhaseMap(), result,
                                      error),
           error.c_str());
  QCOMPARE(result.regionCount, 1);
  QVERIFY(!result.orders.hasValue(0, 0));
  QVERIFY(maxUnwrapError(result.orders, 3.0, insideAperture) < 1e-9);
}

void PhaseShiftingTest::unwrapRoutesAroundLowQualityNoise() {
  // Полоса шума поперёк середины, но не до краёв апертуры: обход по
  // качеству должен развернуть всё хорошее вокруг неё и только потом зайти
  // в шум -- так, что хорошие точки не испорчены.
  auto wrapped = wrappedTruePhase(3.0);
  digitqt::core::PhaseMap quality(kSize, kSize);
  auto inNoise = [](int x, int y) { return y >= 60 && y < 68 && x >= 20 && x < 100; };
  uint32_t state = 12345;
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      if (!insideAperture(x, y))
        continue;
      quality.setValue(x, y, inNoise(x, y) ? 0.0 : 1.0);
      if (inNoise(x, y)) {
        state = state * 1664525u + 1013904223u;
        wrapped.setValue(x, y, (state / 4294967296.0) * 2.0 * kPi - kPi);
      }
    }
  }
  auto good = [&](int x, int y) { return insideAperture(x, y) && !inNoise(x, y); };

  digitqt::core::UnwrapResult result;
  std::string error;
  QVERIFY2(digitqt::core::unwrapPhase(wrapped, quality, result, error), error.c_str());
  QVERIFY(maxUnwrapError(result.orders, 3.0, good) < 1e-9);

  // Без карты качества тот же шум ломает развёртку и за своими пределами
  // -- в этом и смысл обхода по качеству.
  QVERIFY2(digitqt::core::unwrapPhase(wrapped, digitqt::core::PhaseMap(), result, error),
           error.c_str());
  QVERIFY(maxUnwrapError(result.orders, 3.0, good) > 0.5);
}

void PhaseShiftingTest::unwrapCountsDisconnectedRegions() {
  auto wrapped = wrappedTruePhase(1.0);
  for (int y = 0; y < kSize; ++y)
    wrapped.setValue(kSize / 2, y, std::numeric_limits<double>::quiet_NaN());  // разрез
  digitqt::core::UnwrapResult result;
  std::string error;
  QVERIFY2(digitqt::core::unwrapPhase(wrapped, digitqt::core::PhaseMap(), result, error),
           error.c_str());
  QCOMPARE(result.regionCount, 2);
  QVERIFY(!result.orders.hasValue(kSize / 2, kSize / 2));

  // Одиночная точка вне апертуры -- островок: с порогом размера она
  // выбрасывается и областью не считается.
  wrapped.setValue(1, 1, 0.5);
  QVERIFY2(digitqt::core::unwrapPhase(wrapped, digitqt::core::PhaseMap(), result, error),
           error.c_str());
  QCOMPARE(result.regionCount, 3);
  QVERIFY2(digitqt::core::unwrapPhase(wrapped, digitqt::core::PhaseMap(), result, error, 100),
           error.c_str());
  QCOMPARE(result.regionCount, 2);
  QCOMPARE(result.droppedPoints, 1);
  QVERIFY(!result.orders.hasValue(1, 1));

  QVERIFY(!digitqt::core::unwrapPhase(digitqt::core::PhaseMap(kSize, kSize),
                                      digitqt::core::PhaseMap(), result, error));
}

void PhaseShiftingTest::pipelineStageComputesPhaseFromSeries() {
  // Весь путь, как в программе: серия в Measurement, апертура, S2 с
  // методом PSI и номинальным шагом 90 -- при настоящих сдвигах рис. 4.
  using digitqt::core::pipeline::StageId;
  digitqt::core::Measurement measurement;
  measurement.setPhaseShiftingFrames(makeFrames(kArticleShiftsDeg), "frame1.bmp");
  measurement.boundaries().addExternal(
      std::make_unique<aperture::Ellipse>(60.0, 60.0, (kSize - 1) / 2.0, (kSize - 1) / 2.0));
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);

  digitqt::core::pipeline::Pipeline pipeline;
  // Setup для PSI ничего не трассирует, но и не мешает.
  QVERIFY2(pipeline.stage(StageId::Setup).compute(measurement),
           pipeline.stage(StageId::Setup).errorMessage().c_str());
  QVERIFY2(pipeline.stage(StageId::S2).compute(measurement),
           pipeline.stage(StageId::S2).errorMessage().c_str());

  const auto &report = measurement.phaseShifting().report();
  QVERIFY(report.converged);
  QCOMPARE(report.regionCount, 1);
  QCOMPARE(report.shiftsDeg.size(), kArticleShiftsDeg.size());
  for (size_t n = 0; n < kArticleShiftsDeg.size(); ++n)
    QVERIFY(std::fabs(report.shiftsDeg[n] - kArticleShiftsDeg[n]) < 0.2);

  // Карта -- развёрнутая фаза в порядках полосы: настоящая фаза / 2*pi с
  // точностью до константы.
  const auto &orders = measurement.phaseMap();
  const int c = kSize / 2;
  const double offset = orders.value(c, c) - truePhase(c, c) / (2.0 * kPi);
  double sumSq = 0.0;
  int count = 0;
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      if (!insideAperture(x, y) || !orders.hasValue(x, y))
        continue;
      const double e = orders.value(x, y) - truePhase(x, y) / (2.0 * kPi) - offset;
      sumSq += e * e;
      ++count;
    }
  }
  const double rms = std::sqrt(sumSq / count);
  qInfo("S2 via PSI: %d points, error RMS %.5f waves", count, rms);
  QVERIFY(count > 10000);
  QVERIFY(rms < 0.001);
}

void PhaseShiftingTest::pipelineStageRequiresSeries() {
  using digitqt::core::pipeline::StageId;
  digitqt::core::Measurement measurement;
  measurement.setImage(makeFrames({0.0})[0], "single.bmp");
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);
  digitqt::core::pipeline::Pipeline pipeline;
  QVERIFY(!pipeline.stage(StageId::S2).compute(measurement));
  QVERIFY(pipeline.stage(StageId::S2).errorMessage().find("series") != std::string::npos);

  // Открытие одиночного изображения сбрасывает ранее загруженную серию.
  measurement.setPhaseShiftingFrames(makeFrames(kArticleShiftsDeg), "frame1.bmp");
  QVERIFY(measurement.phaseShifting().hasSeries());
  measurement.setImage(makeFrames({0.0})[0], "single.bmp");
  QVERIFY(!measurement.phaseShifting().hasSeries());
}

void PhaseShiftingTest::pipelineStageWithoutRefinementUsesNominalShifts() {
  using digitqt::core::pipeline::StageId;
  digitqt::core::Measurement measurement;
  measurement.setPhaseShiftingFrames(makeFrames(kArticleShiftsDeg), "frame1.bmp");
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);
  measurement.phaseShifting().setRefineShifts(false);
  // Наклон без уточнения не действует.
  measurement.phaseShifting().setCompensateTilt(true);

  digitqt::core::pipeline::Pipeline pipeline;
  QVERIFY2(pipeline.stage(StageId::Setup).compute(measurement),
           pipeline.stage(StageId::Setup).errorMessage().c_str());
  QVERIFY2(pipeline.stage(StageId::S2).compute(measurement),
           pipeline.stage(StageId::S2).errorMessage().c_str());

  const auto &report = measurement.phaseShifting().report();
  QVERIFY(!report.refined);
  QCOMPARE(report.iterations, 0);
  QVERIFY(report.tiltXWaves.empty());
  const std::vector<double> nominalDeg = {0.0, 90.0, 180.0, 270.0, 0.0};
  QCOMPARE(report.shiftsDeg.size(), nominalDeg.size());
  for (size_t n = 0; n < nominalDeg.size(); ++n)
    QVERIFY(std::fabs(report.shiftsDeg[n] - nominalDeg[n]) < 1e-9);
}

void PhaseShiftingTest::pipelineStageReportsTiltOfSourceFrames() {
  // Как при открытии серии в GUI: кадры дополнены полем. Наклон в отчёте --
  // в волнах на ширину кадра, каким его сняла камера (kSize), а не на
  // ширину кадра с полем -- иначе число зависело бы от поля.
  using digitqt::core::pipeline::StageId;
  auto frames = makeTiltedFrames(kArticleShiftsDeg, kTiltX, std::vector<double>(5, 0.0));
  for (auto &frame : frames)
    frame = digitqt::core::padImageBackground(frame);
  QVERIFY(frames[0].width() > kSize);
  // Полосы в этих кадрах доходят до края, поэтому цвет поля у кадров разный
  // -- апертура обязательна (как нарисовал бы пользователь), иначе поле
  // попадает в оценку сдвигов как "полосы".
  const double center = (frames[0].width() - 1) / 2.0;

  digitqt::core::Measurement measurement;
  measurement.setPhaseShiftingFrames(std::move(frames), "frame1.bmp", kSize, kSize);
  measurement.boundaries().addExternal(
      std::make_unique<aperture::Ellipse>(60.0, 60.0, center, center));
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);
  measurement.phaseShifting().setCompensateTilt(true);
  QCOMPARE(measurement.phaseShifting().sourceWidth(), kSize);

  digitqt::core::pipeline::Pipeline pipeline;
  QVERIFY2(pipeline.stage(StageId::Setup).compute(measurement),
           pipeline.stage(StageId::Setup).errorMessage().c_str());
  QVERIFY2(pipeline.stage(StageId::S2).compute(measurement),
           pipeline.stage(StageId::S2).errorMessage().c_str());

  const auto &report = measurement.phaseShifting().report();
  QVERIFY(report.converged);
  QCOMPARE(report.tiltXWaves.size(), kTiltX.size());
  for (size_t n = 0; n < kTiltX.size(); ++n) {
    const double expected = kTiltX[n] * (kSize - 1) / (2.0 * kPi);
    qInfo("frame %d: tilt x %.4f waves across the source frame (true %.4f)",
          static_cast<int>(n + 1), report.tiltXWaves[n], expected);
    QVERIFY(std::fabs(report.tiltXWaves[n] - expected) < 0.005);
  }
}

}  // namespace

QTEST_APPLESS_MAIN(PhaseShiftingTest)
#include "PhaseShiftingTest.moc"
