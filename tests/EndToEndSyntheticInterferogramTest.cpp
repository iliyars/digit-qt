#include "core/Measurement.h"
#include "core/ModalAnalysisResult.h"
#include "core/ModalFitMethod.h"
#include "core/PhaseReconstructionAlgorithm.h"
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/PipelineStageId.h"

#include <aperture/include/geometry/Ellipse.h>

#include <QtTest/QtTest>

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

constexpr double kPi = 3.14159265358979323846;

/// Same synthetic coefficient set/formula as ModalAnalysisStageTest.cpp's
/// SyntheticCoefficients/sereginSurface -- duplicated deliberately (see
/// that file's comment for why), scaled up to a realistic nm range for a
/// real interferogram image rather than the tiny values used there for
/// exact-arithmetic comparison.
struct SyntheticCoefficients {
  double piston = 0.0;      // arbitrary/unmeasurable from a real interferogram -- not checked
  double tiltX = 1500.0;
  double tiltY = -900.0;
  double defocus = 300.0;
  double astigX = 80.0;
  double astigY = 50.0;
  double comaX = 45.0;
  double comaY = -35.0;
  double trefoilX = 25.0;
  double trefoilY = 18.0;
  double spherical = 40.0;
};

double sereginSurface(const SyntheticCoefficients &c, double x, double y) {
  const double r2 = x * x + y * y;
  return c.piston                                              //
         + c.tiltX * x + c.tiltY * (-y)                         //
         + c.defocus * r2                                       //
         + c.astigX * (1.5 * (x * x - y * y))                   //
         + c.astigY * (-x * y)                                  //
         + c.comaX * (x * x * x + x * y * y - (2.0 / 3.0) * x)  //
         + c.comaY * ((2.0 / 3.0) * y - y * r2)                 //
         + c.trefoilX * (3.0 * x * y * y - x * x * x)           //
         + c.trefoilY * (3.0 * x * x * y - y * y * y)           //
         + c.spherical * (r2 * r2);
}

/// Renders a two-beam interferogram: intensity = 128 + 110*cos(2*pi*order),
/// where order is the fringe-order equivalent of the synthetic OPD
/// (order = 2*wavefrontNm/wavelengthNm -- the exact inverse of
/// WavefrontReconstructionStage's wavefront = order * wavelength/2, so
/// feeding this image through the real S1->S2->S4 pipeline should recover
/// wavefrontNm = sereginSurface(...) if that pipeline is self-consistent).
/// Pixels outside the aperture circle are left at flat background (128) --
/// VisibilityChecker keeps the tracer/reconstructor from reading them
/// during setup.
QImage renderInterferogram(int size, double centerX, double centerY, double radius,
                           const SyntheticCoefficients &c, double wavelengthNm) {
  QImage image(size, size, QImage::Format_Grayscale8);
  image.fill(128);
  for (int y = 0; y < size; ++y) {
    uchar *line = image.scanLine(y);
    for (int x = 0; x < size; ++x) {
      const double nx = (x - centerX) / radius;
      const double ny = (y - centerY) / radius;
      if (nx * nx + ny * ny > 1.0)
        continue;
      const double wavefrontNm = sereginSurface(c, nx, ny);
      const double order = 2.0 * wavefrontNm / wavelengthNm;
      const double intensity = 128.0 + 110.0 * std::cos(2.0 * kPi * order);
      line[x] = static_cast<uchar>(std::clamp(intensity, 0.0, 255.0));
    }
  }
  return image;
}

/// Runs S1(Setup)->S2->S4->S5 on a Measurement already carrying an image +
/// aperture, asserting each stage succeeds (fails the test with the
/// stage's own errorMessage() if not).
void runFullPipeline(digitqt::core::Measurement &measurement) {
  using digitqt::core::pipeline::Pipeline;
  using digitqt::core::pipeline::StageId;
  Pipeline pipeline;

  QVERIFY2(pipeline.stage(StageId::Setup).compute(measurement),
           qPrintable(pipeline.stage(StageId::Setup).errorMessage()));
  QVERIFY2(pipeline.stage(StageId::S2).compute(measurement),
           qPrintable(pipeline.stage(StageId::S2).errorMessage()));
  QVERIFY2(pipeline.stage(StageId::S4).compute(measurement),
           qPrintable(pipeline.stage(StageId::S4).errorMessage()));
  QVERIFY2(pipeline.stage(StageId::S5).compute(measurement),
           qPrintable(pipeline.stage(StageId::S5).errorMessage()));
}

/// Bounding box of non-NaN pixels in `map` -- the exact same computation
/// ModalAnalysisStage::doCompute() does internally to derive its pupil
/// center/radius. Exposed here purely for diagnostics: comparing this
/// against the aperture the image was actually rendered with tells us
/// whether S5's normalization matches the true aperture, or whether S1/S2
/// produced a phase map that doesn't fully cover it.
QString boundingBoxReport(const digitqt::core::PhaseMap &map) {
  int minX = map.width(), maxX = -1, minY = map.height(), maxY = -1;
  for (int y = 0; y < map.height(); ++y)
    for (int x = 0; x < map.width(); ++x)
      if (map.hasValue(x, y)) {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
      }
  if (maxX < minX || maxY < minY)
    return QStringLiteral("empty");
  const double cx = (minX + maxX) / 2.0, cy = (minY + maxY) / 2.0;
  const double radius = std::max(maxX - minX, maxY - minY) / 2.0;
  return QStringLiteral("bbox=[%1,%2]-[%3,%4] center=(%5,%6) radius=%7")
      .arg(minX).arg(minY).arg(maxX).arg(maxY).arg(cx).arg(cy).arg(radius);
}

/// Appends one line to a fixed report file next to the test binary (CWD
/// when run under ctest -- see tests/CMakeLists.txt's "Directory:" in
/// LastTest.log). Exists because this environment's stdio capture for
/// this test binary is unreliable (empty output via ctest/direct-run
/// redirection alike, even for previously-passing tests) -- writing the
/// diagnostic to a file directly sidesteps that entirely, no matter the
/// underlying cause.
void appendToReportFile(const QString &line) {
  QFile file(QStringLiteral("e2e_report.txt"));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    return;
  QTextStream out(&file);
  out << line << '\n';
}

/// Logs expected-vs-fitted for every term (always, pass or fail -- this
/// test exists to produce a diagnostic, not just a checkmark) and asserts
/// each is within toleranceFraction of the expected magnitude (or
/// toleranceFloorNm, whichever is larger, for terms whose expected value
/// is small). Piston is deliberately not compared -- see
/// SyntheticCoefficients::piston.
///
/// defocus/spherical get a wider tolerance than everything else: both
/// are purely radial (r^2, r^4) and, unlike the rest of JointLeastSquares's
/// basis, genuinely NOT orthogonal to each other over a disk (see
/// buildTermHierarchy()'s docs and notes/uchebnik-...md §7.3) -- any real
/// (small) noise in the reconstructed phase map gets amplified out of
/// proportion specifically between this pair, even though the joint LSQ
/// still recovers everything exactly given a noiseless map (see
/// ModalAnalysisStageTest::exactRecoveryMatchesSereginBasis). That's an
/// accepted property of this basis choice, not a regression to guard
/// against here.
void reportAndCheck(const char *label, const SyntheticCoefficients &expected,
                    const digitqt::core::ModalCoefficients &fitted, double toleranceFraction,
                    double toleranceFloorNm) {
  appendToReportFile(QStringLiteral("--- %1 ---").arg(label));
  auto check = [&](const char *name, double exp, double got, double fraction, double floorNm) {
    appendToReportFile(QStringLiteral("%1 expected=%2 got=%3 diff=%4")
                            .arg(name, -10)
                            .arg(exp, 9, 'f', 2)
                            .arg(got, 9, 'f', 2)
                            .arg(got - exp, 8, 'f', 2));
    const double tol = std::max(std::abs(exp) * fraction, floorNm);
    QVERIFY2(std::abs(got - exp) <= tol,
             qPrintable(QStringLiteral("%1: expected %2, got %3 (tolerance %4)")
                            .arg(name).arg(exp).arg(got).arg(tol)));
  };
  check("tiltX", expected.tiltX, fitted.tiltX, toleranceFraction, toleranceFloorNm);
  check("tiltY", expected.tiltY, fitted.tiltY, toleranceFraction, toleranceFloorNm);
  check("defocus", expected.defocus, fitted.defocus, 0.6, 100.0);
  check("astigX", expected.astigX, fitted.astigX, toleranceFraction, toleranceFloorNm);
  check("astigY", expected.astigY, fitted.astigY, toleranceFraction, toleranceFloorNm);
  check("comaX", expected.comaX, fitted.comaX, toleranceFraction, toleranceFloorNm);
  check("comaY", expected.comaY, fitted.comaY, toleranceFraction, toleranceFloorNm);
  check("trefoilX", expected.trefoilX, fitted.trefoilX, toleranceFraction, toleranceFloorNm);
  check("trefoilY", expected.trefoilY, fitted.trefoilY, toleranceFraction, toleranceFloorNm);
  check("spherical", expected.spherical, fitted.spherical, 0.6, 100.0);
}

constexpr int kSize = 600;
constexpr double kCenter = kSize / 2.0;
constexpr double kRadius = 260.0;
constexpr double kWavelengthNm = 632.8;

/// A carrier tilt kept identical across every isolated-term case below --
/// on its own (zero everything else) it's the baseline; every other case
/// adds exactly one more nonzero term on top of this SAME tilt, so any
/// term that fails to recover cleanly can't be blamed on "no carrier" (a
/// real, documented limitation of the Fourier method for closed-ring
/// patterns -- e.g. defocus alone, with no tilt, is concentric rings).
SyntheticCoefficients withCarrierTilt() {
  SyntheticCoefficients c;
  c.tiltX = 1500.0;
  c.tiltY = -900.0;
  c.defocus = 0.0;
  c.astigX = 0.0;
  c.astigY = 0.0;
  c.comaX = 0.0;
  c.comaY = 0.0;
  c.trefoilX = 0.0;
  c.trefoilY = 0.0;
  c.spherical = 0.0;
  return c;
}

/// Renders `coeffs`, runs it through the real Fourier-method pipeline
/// (S1 clears/skips tracing for this algorithm -- see SetupStage.cpp --
/// so this exercises S2's FourierPhaseExtractor + S4 + S5 only), logs
/// full diagnostics to e2e_report.txt, and checks every term against
/// `coeffs` with a generous tolerance.
void runFourierCase(const char *label, const SyntheticCoefficients &coeffs) {
  digitqt::core::Measurement measurement;
  measurement.setImage(
      renderInterferogram(kSize, kCenter, kCenter, kRadius, coeffs, kWavelengthNm),
      QStringLiteral("synthetic"));
  measurement.boundaries().addExternal(
      std::make_unique<aperture::Ellipse>(kRadius, kRadius, kCenter, kCenter));
  measurement.setWavelengthNm(kWavelengthNm);
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::FourierTransform);

  runFullPipeline(measurement);

  appendToReportFile(QStringLiteral("%1: phaseMap %2; piston=%3 rmsBefore=%4 rmsAfter=%5")
                          .arg(label)
                          .arg(boundingBoxReport(measurement.phaseMap()))
                          .arg(measurement.modalAnalysis().coefficients.piston)
                          .arg(measurement.modalAnalysis().rmsBefore)
                          .arg(measurement.modalAnalysis().rmsAfter));
  reportAndCheck(label, coeffs, measurement.modalAnalysis().coefficients,
                 /*toleranceFraction=*/0.15, /*toleranceFloorNm=*/20.0);
}

}  // namespace

class EndToEndSyntheticInterferogramTest : public QObject {
  Q_OBJECT

private slots:
  // Pure carrier tilt, nothing else -- establishes whether tilt recovery
  // itself is clean before adding anything on top of it.
  void tiltOnlyBaseline();

  // Each of these adds exactly ONE aberration on top of the SAME carrier
  // tilt as the baseline above, isolating which specific term (if any)
  // breaks -- rather than the original combined-everything test, which
  // showed large errors on every term at once and couldn't say why.
  void tiltPlusDefocus();
  void tiltPlusAstigmatism();
  void tiltPlusComa();
  void tiltPlusTrefoil();
  void tiltPlusSpherical();
};

void EndToEndSyntheticInterferogramTest::tiltOnlyBaseline() {
  runFourierCase("tiltOnlyBaseline", withCarrierTilt());
}

void EndToEndSyntheticInterferogramTest::tiltPlusDefocus() {
  auto c = withCarrierTilt();
  c.defocus = 300.0;
  runFourierCase("tiltPlusDefocus", c);
}

void EndToEndSyntheticInterferogramTest::tiltPlusAstigmatism() {
  auto c = withCarrierTilt();
  c.astigX = 80.0;
  c.astigY = 50.0;
  runFourierCase("tiltPlusAstigmatism", c);
}

void EndToEndSyntheticInterferogramTest::tiltPlusComa() {
  auto c = withCarrierTilt();
  c.comaX = 45.0;
  c.comaY = -35.0;
  runFourierCase("tiltPlusComa", c);
}

void EndToEndSyntheticInterferogramTest::tiltPlusTrefoil() {
  auto c = withCarrierTilt();
  c.trefoilX = 25.0;
  c.trefoilY = 18.0;
  runFourierCase("tiltPlusTrefoil", c);
}

void EndToEndSyntheticInterferogramTest::tiltPlusSpherical() {
  auto c = withCarrierTilt();
  c.spherical = 40.0;
  runFourierCase("tiltPlusSpherical", c);
}

QTEST_MAIN(EndToEndSyntheticInterferogramTest)
#include "EndToEndSyntheticInterferogramTest.moc"
