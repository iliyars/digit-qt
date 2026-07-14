#include "ModalAnalysisStage.h"

#include "core/Measurement.h"
#include "core/ModalAnalysisResult.h"

#include <Eigen/Dense>
#include <cmath>
#include <vector>

namespace digitqt::core::pipeline {

bool ModalAnalysisStage::doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) {
  const auto &wavefront = measurement.wavefrontMap();
  if (wavefront.isEmpty()) {
    errorMessage = QStringLiteral("No wavefront map. Run Wavefront Reconstruction (S4) first.");
    return false;
  }

  const int w = wavefront.width();
  const int h = wavefront.height();

  // Нормализованные координаты (x, y в [-1, 1], центр сетки -- 0), чтобы
  // подгонка была численно устойчива независимо от абсолютного размера
  // картинки в пикселях.
  struct Sample {
    double x, y, z;
    int px, py;
  };
  std::vector<Sample> samples;
  samples.reserve(static_cast<size_t>(w) * static_cast<size_t>(h) / 4);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!wavefront.hasValue(x, y))
        continue;
      const double nx = (w > 1) ? (2.0 * x / (w - 1) - 1.0) : 0.0;
      const double ny = (h > 1) ? (2.0 * y / (h - 1) - 1.0) : 0.0;
      samples.push_back({nx, ny, wavefront.value(x, y), x, y});
    }
  }

  if (samples.size() < 11) {
    errorMessage = QStringLiteral("Not enough valid points to fit (need at least 11)");
    return false;
  }

  // Базис: 1, x, y, x²+y² (дефокус), x²-y² и xy (астигматизм),
  // x³+xy² и y³+x²y (кома), x³-3xy² и 3x²y-y³ (трилистник), (x²+y²)²
  // (сферическая аберрация 3-го порядка). См. ModalCoefficients за
  // подробным разбором каждого члена.
  constexpr int kNumTerms = 11;
  const auto n = static_cast<Eigen::Index>(samples.size());
  Eigen::MatrixXd A(n, kNumTerms);
  Eigen::VectorXd b(n);
  for (Eigen::Index i = 0; i < n; ++i) {
    const auto &s = samples[static_cast<size_t>(i)];
    const double x = s.x, y = s.y;
    const double r2 = x * x + y * y;
    A(i, 0) = 1.0;
    A(i, 1) = x;
    A(i, 2) = y;
    A(i, 3) = r2;
    A(i, 4) = x * x - y * y;
    A(i, 5) = x * y;
    A(i, 6) = x * r2;  // x³+xy²
    A(i, 7) = y * r2;  // y³+x²y
    A(i, 8) = x * x * x - 3 * x * y * y;
    A(i, 9) = 3 * x * x * y - y * y * y;
    A(i, 10) = r2 * r2;
    b(i) = s.z;
  }

  const Eigen::VectorXd coeffs = A.colPivHouseholderQr().solve(b);

  digitqt::core::ModalCoefficients mc;
  mc.piston = coeffs(0);
  mc.tiltX = coeffs(1);
  mc.tiltY = coeffs(2);
  mc.defocus = coeffs(3);
  mc.astigX = coeffs(4);
  mc.astigY = coeffs(5);
  mc.comaX = coeffs(6);
  mc.comaY = coeffs(7);
  mc.trefoilX = coeffs(8);
  mc.trefoilY = coeffs(9);
  mc.spherical = coeffs(10);

  digitqt::core::PhaseMap residual(w, h);
  double sumSqBefore = 0.0;
  double sumSqAfter = 0.0;
  for (const auto &s : samples) {
    const double x = s.x, y = s.y;
    const double r2 = x * x + y * y;
    const double fit = mc.piston + mc.tiltX * x + mc.tiltY * y + mc.defocus * r2 +
                       mc.astigX * (x * x - y * y) + mc.astigY * (x * y) + mc.comaX * (x * r2) +
                       mc.comaY * (y * r2) + mc.trefoilX * (x * x * x - 3 * x * y * y) +
                       mc.trefoilY * (3 * x * x * y - y * y * y) + mc.spherical * (r2 * r2);
    const double r = s.z - fit;
    residual.setValue(s.px, s.py, r);
    sumSqBefore += s.z * s.z;
    sumSqAfter += r * r;
  }

  digitqt::core::ModalAnalysisResult result;
  result.coefficients = mc;
  result.residual = std::move(residual);
  result.rmsBefore = std::sqrt(sumSqBefore / static_cast<double>(samples.size()));
  result.rmsAfter = std::sqrt(sumSqAfter / static_cast<double>(samples.size()));

  measurement.modalAnalysis() = std::move(result);
  return true;
}

}  // namespace digitqt::core::pipeline
