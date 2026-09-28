#include "FiducialTransformFit.h"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

namespace digitqt::core {

FiducialTransformFit fitFiducialTransform(const std::vector<Fiducial> &fiducials) {
  FiducialTransformFit result;

  std::vector<const Fiducial *> used;
  for (const auto &f : fiducials)
    if (f.hasReference)
      used.push_back(&f);

  if (used.size() < 3) {
    result.errorMessage =
        "Need at least 3 fiducials with a reference position (have " +
        std::to_string(used.size()) + ")";
    return result;
  }

  // x' = a*x + b*y + c и y' = d*x + e*y + f не делят неизвестных -- два
  // независимых МНК по одной и той же матрице A.
  const auto n = static_cast<Eigen::Index>(used.size());
  Eigen::MatrixXd A(n, 3);
  Eigen::VectorXd bx(n), by(n);
  for (Eigen::Index i = 0; i < n; ++i) {
    const auto &f = *used[static_cast<size_t>(i)];
    A(i, 0) = f.imageX;
    A(i, 1) = f.imageY;
    A(i, 2) = 1.0;
    bx(i) = f.referenceX;
    by(i) = f.referenceY;
  }

  const Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(A);
  if (qr.rank() < 3) {
    result.errorMessage = "Fiducials are (nearly) collinear -- cannot fit a unique transform";
    return result;
  }

  const Eigen::VectorXd coeffX = qr.solve(bx);
  const Eigen::VectorXd coeffY = qr.solve(by);

  result.transform.a = coeffX(0);
  result.transform.b = coeffX(1);
  result.transform.c = coeffX(2);
  result.transform.d = coeffY(0);
  result.transform.e = coeffY(1);
  result.transform.f = coeffY(2);

  double sumSq = 0.0, maxErr = 0.0;
  result.residuals.reserve(used.size());
  for (const auto *f : used) {
    const auto mapped = result.transform.apply({f->imageX, f->imageY});
    const double dx = mapped.x - f->referenceX;
    const double dy = mapped.y - f->referenceY;
    const double err = std::sqrt(dx * dx + dy * dy);
    result.residuals.push_back({f->id, err});
    sumSq += err * err;
    maxErr = std::max(maxErr, err);
  }

  result.usedCount = used.size();
  result.rmsError = std::sqrt(sumSq / static_cast<double>(used.size()));
  result.maxError = maxErr;
  result.ok = true;
  return result;
}

}  // namespace digitqt::core
