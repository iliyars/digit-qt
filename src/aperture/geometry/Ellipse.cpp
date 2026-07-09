/**
 * @file Ellipse.cpp
 * @brief Implementation of Ellipse class
 */

#include "geometry/Ellipse.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace aperture {

Ellipse::Ellipse(double semiMajorAxis, double semiMinorAxis, double centerX,
                 double centerY, double rotationDegrees, TypeLimits typeLimits,
                 CoordinateSystem spatialSystem, NormalizationState normState)
    : semiMajor_(semiMajorAxis),
      semiMinor_(semiMinorAxis),
      center_(centerX, centerY),
      rotationDeg_(rotationDegrees),
      rotationRad_(rotationDegrees * M_PI / 180.0),
      cosRot_(0.0),
      sinRot_(0.0) {
  typeLimits_ = typeLimits;
  spatialSystem_ = spatialSystem;
  normState_ = normState;
  updateRotationCache();
}

void Ellipse::updateRotationCache() {
  cosRot_ = std::cos(rotationRad_);
  sinRot_ = std::sin(rotationRad_);
}

Point Ellipse::toLocalCoordinates(const Point &point) const {
  // Translate to origin
  double dx = point.x - center_.x;
  double dy = point.y - center_.y;

  // Rotate by -rotation to align with axes
  return Point{dx * cosRot_ + dy * sinRot_, -dx * sinRot_ + dy * cosRot_};
}

Point Ellipse::toWorldCoordinates(const Point &point) const {
  // Rotate by +rotation
  double x = point.x * cosRot_ - point.y * sinRot_;
  double y = point.x * sinRot_ + point.y * cosRot_;

  // Translate to center
  return Point{x + center_.x, y + center_.y};
}

bool Ellipse::isInside(const Point &point) const {
  // Transform to ellipse-local coordinates
  Point local = toLocalCoordinates(point);

  // Check ellipse equation: (x/a)^2 + (y/b)^2 <= 1
  double term1 = (local.x * local.x) / (semiMajor_ * semiMajor_);
  double term2 = (local.y * local.y) / (semiMinor_ * semiMinor_);

  return (term1 + term2) <= 1.0;
}

Bounds Ellipse::getBounds() const {
  if (std::abs(rotationDeg_) < 1e-6) {
    // No rotation - simple case
    return Bounds{center_.x - semiMajor_, center_.y - semiMinor_,
                  center_.x + semiMajor_, center_.y + semiMinor_};
  }

  // For rotated ellipse, find the extreme points
  // The bounding box vertices are where dx/dt = 0 and dy/dt = 0
  // in the parametric equations:
  // x(t) = cx + a*cos(t)*cos(r) - b*sin(t)*sin(r)
  // y(t) = cy + a*cos(t)*sin(r) + b*sin(t)*cos(r)

  double a2_cos2 = semiMajor_ * semiMajor_ * cosRot_ * cosRot_;
  double b2_sin2 = semiMinor_ * semiMinor_ * sinRot_ * sinRot_;
  double a2_sin2 = semiMajor_ * semiMajor_ * sinRot_ * sinRot_;
  double b2_cos2 = semiMinor_ * semiMinor_ * cosRot_ * cosRot_;

  double halfWidth = std::sqrt(a2_cos2 + b2_sin2);
  double halfHeight = std::sqrt(a2_sin2 + b2_cos2);

  return Bounds{center_.x - halfWidth, center_.y - halfHeight,
                center_.x + halfWidth, center_.y + halfHeight};
}

std::vector<Point> Ellipse::getContour(double stepSize) const {
  // Calculate number of points based on perimeter and step size
  double perim = perimeter();
  int numPoints = std::max(8, static_cast<int>(perim / stepSize));

  std::vector<Point> contour;
  contour.reserve(numPoints + 1);  // +1 for closing point

  // Generate points parametrically
  double angleStep = 2.0 * M_PI / numPoints;

  for (int i = 0; i <= numPoints; ++i) {
    double t = i * angleStep;

    // Parametric ellipse in local coordinates
    Point local{semiMajor_ * std::cos(t), semiMinor_ * std::sin(t)};

    // Transform to world coordinates
    contour.push_back(toWorldCoordinates(local));
  }

  return contour;
}

bool Ellipse::isOnContour(const Point &point, double tolerance) const {
  // Transform to ellipse-local coordinates
  Point local = toLocalCoordinates(point);

  // Check if point is inside enlarged ellipse (axes + tolerance)
  double enlargedA = semiMajor_ + tolerance;
  double enlargedB = semiMinor_ + tolerance;
  double term1_enlarged = (local.x * local.x) / (enlargedA * enlargedA);
  double term2_enlarged = (local.y * local.y) / (enlargedB * enlargedB);

  if (term1_enlarged + term2_enlarged > 1.0) {
    return false;  // Outside enlarged ellipse - too far from contour
  }

  // Check if point is outside diminished ellipse (axes - tolerance)
  double diminishedA = std::max(0.0, semiMajor_ - tolerance);
  double diminishedB = std::max(0.0, semiMinor_ - tolerance);

  // If diminished ellipse is degenerate (tolerance >= axis),
  // and we're inside enlarged, then we're on the contour
  if (diminishedA < 1e-10 || diminishedB < 1e-10) {
    return true;
  }

  double term1_diminished = (local.x * local.x) / (diminishedA * diminishedA);
  double term2_diminished = (local.y * local.y) / (diminishedB * diminishedB);

  // Point is on contour if it's inside enlarged but outside diminished
  return (term1_diminished + term2_diminished) >= 1.0;
}

double Ellipse::perimeter() const {
  // Use Ramanujan's approximation for ellipse perimeter
  // P ? ? * (3(a + b) - sqrt((3a + b)(a + 3b)))
  // This is accurate to within 0.01% for most ellipses

  double a = semiMajor_;
  double b = semiMinor_;

  if (isCircle()) {
    // Exact for circles
    return 2.0 * M_PI * a;
  }

  // Ramanujan's second approximation
  double h = ((a - b) * (a - b)) / ((a + b) * (a + b));
  return M_PI * (a + b) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));
}

double Ellipse::area() const {
  return M_PI * semiMajor_ * semiMinor_;
}

double Ellipse::eccentricity() const {
  if (semiMajor_ < semiMinor_) {
    // b > a, swap for calculation
    double e2 = 1.0 - (semiMajor_ * semiMajor_) / (semiMinor_ * semiMinor_);
    return std::sqrt(std::max(0.0, e2));
  }

  double e2 = 1.0 - (semiMinor_ * semiMinor_) / (semiMajor_ * semiMajor_);
  return std::sqrt(std::max(0.0, e2));
}

double Ellipse::focalDistance() const {
  if (isCircle()) {
    return 0.0;
  }

  double a = std::max(semiMajor_, semiMinor_);
  double b = std::min(semiMajor_, semiMinor_);

  return std::sqrt(a * a - b * b);
}

std::unique_ptr<Shape> Ellipse::clone() const {
  return std::make_unique<Ellipse>(*this);
}

void Ellipse::normalize(double originX, double originY, double radius) {
  semiMajor_ /= radius;
  semiMinor_ /= radius;
  center_.x = (center_.x - originX) / radius;
  center_.y = (center_.y - originY) / radius;
  normState_ = NormalizationState::NORMALIZED;
}

void Ellipse::denormalize(double originX, double originY, double radius) {
  semiMajor_ *= radius;
  semiMinor_ *= radius;
  center_.x = center_.x * radius + originX;
  center_.y = center_.y * radius + originY;
  normState_ = NormalizationState::MEASURING;
}

void Ellipse::inverseY(double centerY) {
  center_.y = centerY - center_.y;
  rotationDeg_ = -rotationDeg_;
  rotationRad_ = -rotationRad_;
  updateRotationCache();
}

void Ellipse::shiftX(double deltaX) {
  center_.x += deltaX;
}

void Ellipse::shiftY(double deltaY) {
  center_.y += deltaY;
}

//===========================================================================
// Ellipse fitting constructor implementation
//===========================================================================

namespace {
// Helper: Solve NxN linear system using Gaussian elimination
// Returns true if solution found, false if singular
template <size_t N>
bool solveLinearSystemNxN(double A[N][N], double b[N], double x[N]) {
  constexpr double EPSILON = 1e-10;

  // Create augmented matrix
  double aug[N][N + 1];
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      aug[i][j] = A[i][j];
    }
    aug[i][N] = b[i];
  }

  // Forward elimination with partial pivoting
  for (int k = 0; k < N; k++) {
    // Find pivot
    int maxRow = k;
    double maxVal = std::abs(aug[k][k]);
    for (int i = k + 1; i < N; i++) {
      double val = std::abs(aug[i][k]);
      if (val > maxVal) {
        maxVal = val;
        maxRow = i;
      }
    }

    if (maxVal < EPSILON) {
      return false;  // Singular matrix
    }

    // Swap rows
    if (maxRow != k) {
      for (int j = 0; j < N + 1; j++) {
        std::swap(aug[k][j], aug[maxRow][j]);
      }
    }

    // Eliminate
    for (int i = k + 1; i < N; i++) {
      double factor = aug[i][k] / aug[k][k];
      for (int j = k; j < N + 1; j++) {
        aug[i][j] -= factor * aug[k][j];
      }
    }
  }

  // Back substitution
  for (int i = N - 1; i >= 0; i--) {
    double sum = aug[i][N];
    for (int j = i + 1; j < N; j++) {
      sum -= aug[i][j] * x[j];
    }
    x[i] = sum / aug[i][i];
  }

  return true;
}

// Helper: Convert conic coefficients to ellipse parameters
// Conic: ax² + bxy + cy² + dx + ey + f = 0
// Returns false if not an ellipse

bool conicToEllipse(double a, double b, double c, double d, double e, double f,
                    double &cx, double &cy, double &A, double &B,
                    double &rotDeg) {
  constexpr double EPS = 1e-12;

  double q11 = a;
  double q12 = 0.5 * b;
  double q22 = c;

  double detQ = q11 * q22 - q12 * q12;
  if (detQ <= EPS)
    return false;

  double invQ11 = q22 / detQ;
  double invQ12 = -q12 / detQ;
  double invQ22 = q11 / detQ;

  cx = -0.5 * (invQ11 * d + invQ12 * e);
  cy = -0.5 * (invQ12 * d + invQ22 * e);

  double k = q11 * cx * cx + 2 * q12 * cx * cy + q22 * cy * cy - f;

  if (k <= EPS)
    return false;

  double tr = q11 + q22;
  double diff = q11 - q22;
  double root = sqrt(diff * diff + 4 * q12 * q12);

  double l1 = 0.5 * (tr + root);
  double l2 = 0.5 * (tr - root);

  if (l1 <= EPS || l2 <= EPS)
    return false;

  double a1 = sqrt(k / l1);
  double a2 = sqrt(k / l2);

  if (a1 >= a2) {
    A = a1;
    B = a2;
    rotDeg = 0.5 * atan2(2 * q12, diff) * 180.0 / M_PI;
  } else {
    A = a2;
    B = a1;
    rotDeg = 0.5 * atan2(2 * q12, diff) * 180.0 / M_PI + 90.0;
  }
  if (rotDeg >= 180.)
    rotDeg -= 180;
  if (rotDeg <= -180.)
    rotDeg += 180.;

  return true;
}

// Helper: Inver 3x3 matrix
//
// static bool invert3x3(const double m[3][3], double inv[3][3])
//{
//    double det =
//        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
//        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
//        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

//    if (fabs(det) < 1e-12) return false;

//    double id = 1.0 / det;

//    inv[0][0] = id * (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
//    inv[0][1] = -id * (m[0][1] * m[2][2] - m[0][2] * m[2][1]);
//    inv[0][2] = id * (m[0][1] * m[1][2] - m[0][2] * m[1][1]);

//    inv[1][0] = -id * (m[1][0] * m[2][2] - m[1][2] * m[2][0]);
//    inv[1][1] = id * (m[0][0] * m[2][2] - m[0][2] * m[2][0]);
//    inv[1][2] = -id * (m[0][0] * m[1][2] - m[0][2] * m[1][0]);

//    inv[2][0] = id * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
//    inv[2][1] = -id * (m[0][0] * m[2][1] - m[0][1] * m[2][0]);
//    inv[2][2] = id * (m[0][0] * m[1][1] - m[0][1] * m[1][0]);

//    return true;
//}

//
// Helper: Small symmetric 3×3 Jacobi eigen solver
//
// static void eigenSym3(double A[3][3], double w[3], double V[3][3])
//{
//    for (int i = 0; i < 3; i++)
//        for (int j = 0; j < 3; j++)
//            V[i][j] = (i == j);

//    for (int it = 0; it < 32; it++)
//    {
//        int p = 0, q = 1;
//        double max = fabs(A[0][1]);
//        if (fabs(A[0][2]) > max) { p = 0; q = 2; max = fabs(A[0][2]); }
//        if (fabs(A[1][2]) > max) { p = 1; q = 2; max = fabs(A[1][2]); }
//        if (max < 1e-12) break;

//        double app = A[p][p];
//        double aqq = A[q][q];
//        double apq = A[p][q];

//        double phi = 0.5 * atan2(2 * apq, aqq - app);
//        double c = cos(phi);
//        double s = sin(phi);

//        for (int k = 0; k < 3; k++)
//        {
//            double aik = A[p][k];
//            double aqk = A[q][k];
//            A[p][k] = c * aik - s * aqk;
//            A[q][k] = s * aik + c * aqk;
//        }
//        for (int k = 0; k < 3; k++)
//        {
//            double akp = A[k][p];
//            double akq = A[k][q];
//            A[k][p] = c * akp - s * akq;
//            A[k][q] = s * akp + c * akq;
//        }
//        for (int k = 0; k < 3; k++)
//        {
//            double vip = V[k][p];
//            double viq = V[k][q];
//            V[k][p] = c * vip - s * viq;
//            V[k][q] = s * vip + c * viq;
//        }
//    }

//    for (int i = 0; i < 3; i++)
//        w[i] = A[i][i];
//}

}  // namespace

Ellipse::Ellipse(const std::vector<Point> &points, TypeLimits typeLimits,
                 CoordinateSystem spatialSystem, NormalizationState normState)
    : semiMajor_(0.0),
      semiMinor_(0.0),
      center_(0.0, 0.0),
      rotationDeg_(0.0),
      rotationRad_(0.0),
      cosRot_(1.0),
      sinRot_(0.0) {
  typeLimits_ = typeLimits;
  spatialSystem_ = spatialSystem;
  normState_ = normState;

  constexpr double EPSILON = 1e-10;
  const size_t n = points.size();

  if (n == 0) {
    // Empty - leave at origin with zero radii
    return;
  }

  if (n == 1) {
    // Single point - degenerate ellipse
    center_ = points[0];
    return;
  }

  if (n == 2) {
    // Two points define circle diameter
    center_.x = (points[0].x + points[1].x) / 2.0;
    center_.y = (points[0].y + points[1].y) / 2.0;
    double radius = points[0].distanceTo(points[1]) / 2.0;
    semiMajor_ = radius;
    semiMinor_ = radius;
    return;
  }

  if (n == 3) {
    // Three points define a circle (geometric fit)
    double x1 = points[0].x, y1 = points[0].y;
    double x2 = points[1].x, y2 = points[1].y;
    double x3 = points[2].x, y3 = points[2].y;

    double A = x1 * (y2 - y3) - y1 * (x2 - x3) + x2 * y3 - x3 * y2;

    if (std::abs(A) < EPSILON) {
      // Collinear points - use centroid
      center_.x = (x1 + x2 + x3) / 3.0;
      center_.y = (y1 + y2 + y3) / 3.0;
      return;
    }

    double B = (x1 * x1 + y1 * y1) * (y3 - y2) +
               (x2 * x2 + y2 * y2) * (y1 - y3) +
               (x3 * x3 + y3 * y3) * (y2 - y1);
    double C = (x1 * x1 + y1 * y1) * (x2 - x3) +
               (x2 * x2 + y2 * y2) * (x3 - x1) +
               (x3 * x3 + y3 * y3) * (x1 - x2);

    center_.x = -B / (2.0 * A);
    center_.y = -C / (2.0 * A);

    double radius = std::sqrt((x1 - center_.x) * (x1 - center_.x) +
                              (y1 - center_.y) * (y1 - center_.y));
    semiMajor_ = radius;
    semiMinor_ = radius;
    return;
  }

  if (n == 4) {
    // For axis-aligned ellipse: (x-x0)^2/a^2 + (y-y0)^2/b^2 = 1

    // We have 4 unknowns: x0, y0, a, b
    // Need to solve system of 4 equations

    // Rearranged form: (x-x0)^2 * b^2 + (y-y0)^2 * a^2 = a^2 * b^2
    // Let's use numerical optimization (Gauss-Newton)

    // Initial guess: center as mean of points
    double sumX = 0, sumY = 0;
    for (int i = 0; i < 4; i++) {
      sumX += points[i].x;
      sumY += points[i].y;
    }
    center_.x = sumX / 4.0;
    center_.y = sumY / 4.0;

    // Initial guess for axes: based on point distances from center
    double maxDistX = 0, maxDistY = 0;
    for (int i = 0; i < 4; i++) {
      maxDistX = std::max(maxDistX, std::abs(points[i].x - center_.x));
      maxDistY = std::max(maxDistY, std::abs(points[i].y - center_.y));
    }
    semiMajor_ = maxDistX;
    semiMinor_ = maxDistY;

    // Refine using least squares (Gauss-Newton iteration)
    for (int iter = 0; iter < 10; iter++) {
      double J[4][4] = {{0}};  // Jacobian
      double r[4];             // Residuals
      double sumJTJ[4][4] = {{0}};
      double sumJTr[4] = {0};

      for (int i = 0; i < 4; i++) {
        double dx = points[i].x - center_.x;
        double dy = points[i].y - center_.y;

        // Residual: (dx^2/a^2 + dy^2/b^2 - 1)
        double val = dx * dx / (semiMajor_ * semiMajor_) +
                     dy * dy / (semiMinor_ * semiMinor_) - 1.0;
        r[i] = val;

        // Jacobian entries
        J[i][0] = -2.0 * dx / (semiMajor_ * semiMajor_);  // d/dx0
        J[i][1] = -2.0 * dy / (semiMinor_ * semiMinor_);  // d/dy0
        J[i][2] =
            -2.0 * dx * dx / (semiMajor_ * semiMajor_ * semiMajor_);  // d/da
        J[i][3] =
            -2.0 * dy * dy / (semiMinor_ * semiMinor_ * semiMinor_);  // d/db

        // Build normal equations
        for (int j = 0; j < 4; j++) {
          for (int k = 0; k < 4; k++) {
            sumJTJ[j][k] += J[i][j] * J[i][k];
          }
          sumJTr[j] += J[i][j] * r[i];
        }
      }

      // In the Gauss-Newton iteration, after building sumJTJ:
      double lambda = 1e-6;  // Levenberg-Marquardt regularization
      for (int i = 0; i < 4; i++) {
        sumJTJ[i][i] *= (1.0 + lambda);  // Add to diagonal
      }

      // Solve JTJ * delta = -JTr
      double delta[4];
      if (!solveLinearSystemNxN<4>(sumJTJ, sumJTr, delta)) {
        // If still singular, increase regularization
        lambda *= 10.0;
        continue;
        // Or break if too many iterations
      }

      // Update parameters
      center_.x -= delta[0];
      center_.y -= delta[1];
      semiMajor_ -= delta[2];
      semiMinor_ -= delta[3];

      // Ensure positive axes
      semiMajor_ = std::max(semiMajor_, 1e-6);
      semiMinor_ = std::max(semiMinor_, 1e-6);
    }

    return;
  }

  if (n == 5) {
    // Build D^T D  (6x6 symmetric)
    double S[6][6] = {0};

    for (int i = 0; i < 5; i++) {
      double x = points[i].x;
      double y = points[i].y;
      double d[6] = {x * x, x * y, y * y, x, y, 1.0};

      for (int r = 0; r < 6; r++)
        for (int c = 0; c <= r; c++)
          S[r][c] += d[r] * d[c];
    }
    for (int r = 0; r < 6; r++)
      for (int c = r + 1; c < 6; c++)
        S[r][c] = S[c][r];

    // Jacobi eigen: smallest eigenvector of S
    double V[6][6] = {0};
    for (int i = 0; i < 6; i++)
      V[i][i] = 1;

    for (int it = 0; it < 50; it++) {
      int p = 0, q = 1;
      double m = fabs(S[p][q]);
      for (int i = 0; i < 6; i++)
        for (int j = i + 1; j < 6; j++)
          if (fabs(S[i][j]) > m) {
            m = fabs(S[i][j]);
            p = i;
            q = j;
          }
      if (m < 1e-12)
        break;

      double phi = 0.5 * atan2(2 * S[p][q], S[q][q] - S[p][p]);
      double c = cos(phi), s = sin(phi);

      for (int k = 0; k < 6; k++) {
        double Spk = S[p][k], Sqk = S[q][k];
        S[p][k] = c * Spk - s * Sqk;
        S[q][k] = s * Spk + c * Sqk;
      }
      for (int k = 0; k < 6; k++) {
        double Skp = S[k][p], Skq = S[k][q];
        S[k][p] = c * Skp - s * Skq;
        S[k][q] = s * Skp + c * Skq;
      }
      for (int k = 0; k < 6; k++) {
        double Vkp = V[k][p], Vkq = V[k][q];
        V[k][p] = c * Vkp - s * Vkq;
        V[k][q] = s * Vkp + c * Vkq;
      }
    }

    int idx = 0;
    for (int i = 1; i < 6; i++)
      if (S[i][i] < S[idx][idx])
        idx = i;

    double a = V[0][idx];
    double bxy = V[1][idx];
    double c = V[2][idx];
    double d = V[3][idx];
    double e = V[4][idx];
    double f = V[5][idx];

    if (4 * a * c - bxy * bxy > 0 &&
        conicToEllipse(a, bxy, c, d, e, f, center_.x, center_.y, semiMajor_,
                       semiMinor_, rotationDeg_)) {
      rotationRad_ = rotationDeg_ * M_PI / 180.0;
      updateRotationCache();
      return;
    }

    // fallback
    std::vector<Point> fourPoints(points.begin(), points.begin() + 4);
    *this = Ellipse(fourPoints, typeLimits, spatialSystem, normState);
    return;
  }

  // n > 5: Least squares ellipse fit
  // !!! This is only temporary stub - must be replaced in production !!!
  // TODO: !!! redesign - the solution is mathematically unstable !!!
  // Build design matrix D and scatter matrix S = D'*D

  double S[5][5] = {{0}};  // Scatter matrix (symmetric)

  // S = D'*D where D = [x² xy y² x y]

  for (size_t k = 0; k < n; k++) {
    double x = points[k].x;
    double y = points[k].y;
    double x2 = x * x;
    double xy = x * y;
    double y2 = y * y;
    double row[5] = {x2, xy, y2, x, y};
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 5; j++) {
        S[i][j] += row[i] * row[j];
      }
    }
  }

  // Right-hand side: D' * (-ones(n,1))
  // We're solving D'*D*coeff = -D'*ones
  // because conic equation is: Ax² + Bxy + Cy² + Dx + Ey + F = 0
  // with F = -1 (normalization), so: Ax² + Bxy + Cy² + Dx + Ey = 1

  double rhs[5] = {0};
  for (size_t k = 0; k < n; k++) {
    double x = points[k].x;
    double y = points[k].y;
    double x2 = x * x;
    double xy = x * y;
    double y2 = y * y;

    // Each row of D contributes to rhs: D'*ones = sum of each column
    rhs[0] += x2;
    rhs[1] += xy;
    rhs[2] += y2;
    rhs[3] += x;
    rhs[4] += y;
  }

  // Solve S * solution = rhs
  double solution[5];
  if (solveLinearSystemNxN<5>(S, rhs, solution)) {
    double a = solution[0];
    double bxy = solution[1];
    double c = solution[2];
    double d = solution[3];
    double e = solution[4];
    double f = -1.0;

    if (conicToEllipse(a, bxy, c, d, e, f, center_.x, center_.y, semiMajor_,
                       semiMinor_, rotationDeg_)) {
      rotationRad_ = rotationDeg_ * M_PI / 180.0;
      updateRotationCache();
      return;
    }
  }

  // Final fallback to 5-point method
  std::vector<Point> fivePoints(points.begin(), points.begin() + 5);
  *this = Ellipse(fivePoints, typeLimits, spatialSystem, normState);
}

// ========================================================================
// Handle Enumeration (Interactive Editing - UX spec §4)
// ========================================================================

void Ellipse::EnumerateHandles(std::vector<HandleDesc> &out) const {
  // §2.1, §4.2 - Move handle at center
  out.push_back(HandleDesc{HandleType::Move, -1, center_});

  // §2.2, §4.3 - Rotation handle offset along major axis normal
  // Offset along perpendicular to major axis (minor axis direction)
  double boundsRadius =
      std::sqrt(semiMajor_ * semiMajor_ + semiMinor_ * semiMinor_);
  double rotHandleOffset = std::max(boundsRadius * 0.2, 20.0);

  // Minor axis direction (perpendicular to major axis)
  Point rotHandlePos{
      center_.x - rotHandleOffset * sinRot_,  // -sin for +Y rotation
      center_.y + rotHandleOffset * cosRot_   // +cos for +Y rotation
  };
  out.push_back(HandleDesc{HandleType::Rotate, -1, rotHandlePos});

  // §4.1 - 4 Axis resize handles (2 major + 2 minor)

  // Major axis endpoints (index 0, 1)
  Point majorEnd1{center_.x + semiMajor_ * cosRot_,
                  center_.y + semiMajor_ * sinRot_};
  Point majorEnd2{center_.x - semiMajor_ * cosRot_,
                  center_.y - semiMajor_ * sinRot_};

  // Normal for major axis = direction of major axis
  Point majorNormal{cosRot_, sinRot_};

  out.push_back(HandleDesc{HandleType::AxisResize, 0, majorEnd1, majorNormal});
  out.push_back(HandleDesc{HandleType::AxisResize, 1, majorEnd2,
                           Point{-majorNormal.x, -majorNormal.y}});

  // Minor axis endpoints (index 2, 3)
  Point minorEnd1{center_.x - semiMinor_ * sinRot_,
                  center_.y + semiMinor_ * cosRot_};
  Point minorEnd2{center_.x + semiMinor_ * sinRot_,
                  center_.y - semiMinor_ * cosRot_};

  // Normal for minor axis = direction perpendicular to major
  Point minorNormal{-sinRot_, cosRot_};

  out.push_back(HandleDesc{HandleType::AxisResize, 2, minorEnd1, minorNormal});
  out.push_back(HandleDesc{HandleType::AxisResize, 3, minorEnd2,
                           Point{-minorNormal.x, -minorNormal.y}});
}

void Ellipse::ApplyHandleDrag(const HandleDesc &handle,
                              const DragContext &drag) {
  switch (handle.type) {
    case HandleType::Move:
      // Simple translation
      center_.x += drag.deltaWorld.x;
      center_.y += drag.deltaWorld.y;
      break;

    case HandleType::Rotate: {
      // Calculate angle from center to current drag position
      double dx = drag.dragCurrentWorld.x - center_.x;
      double dy = drag.dragCurrentWorld.y - center_.y;
      double newAngle = std::atan2(dy, dx);

      // Initial angle from center to drag start
      double dx0 = drag.dragStartWorld.x - center_.x;
      double dy0 = drag.dragStartWorld.y - center_.y;
      double initialAngle = std::atan2(dy0, dx0);

      // Apply rotation delta
      double deltaAngle = newAngle - initialAngle;
      rotationRad_ += deltaAngle;
      rotationDeg_ = rotationRad_ * 180.0 / M_PI;

      // Normalize to [0, 360)
      while (rotationDeg_ < 0.0)
        rotationDeg_ += 360.0;
      while (rotationDeg_ >= 360.0)
        rotationDeg_ -= 360.0;

      updateRotationCache();
      break;
    }

    case HandleType::AxisResize: {
      // Determine which axis is being dragged
      bool isMajorAxis = (handle.index == 0 || handle.index == 1);

      // Project drag delta onto handle normal to get resize amount
      double resize = (drag.deltaWorld.x * handle.normal.x +
                       drag.deltaWorld.y * handle.normal.y) /
                      2.0;

      if (isMajorAxis) {
        semiMajor_ += resize;
        if (handle.index == 0)
          center_.x += resize;
        else
          center_.x -= resize;
        semiMajor_ = std::max(semiMajor_, 1.0);  // Minimum size
      } else {
        semiMinor_ += resize;
        if (handle.index == 2)
          center_.y += resize;
        else
          center_.y -= resize;
        semiMinor_ = std::max(semiMinor_, 1.0);  // Minimum size
      }
      break;
    }

    default:
      break;
  }
}

// ========================================================================
// Static Factory Methods (Phase 1 - Explicit Circle/Ellipse Fitting)
// ========================================================================

// Solve for circle parameters using algebraic method
// (x² + y²) + Ax + By + C = 0
// Center = (-A/2, -B/2), Radius = sqrt((A² + B²)/4 - C)

std::unique_ptr<Ellipse> Ellipse::FitCircle(const std::vector<Point> &points,
                                            TypeLimits typeLimits,
                                            CoordinateSystem spatialSystem,
                                            NormalizationState normState) {
  if (points.size() < 3) {
    // Degenerate case: return zero-radius circle at origin
    return std::make_unique<Ellipse>(0.0, 0.0, 0.0, 0.0, 0.0, typeLimits,
                                     spatialSystem, normState);
  }

  size_t n = points.size();

  // Build linear system: [x²+y², x, y, 1] * [1, A, B, C]ᵀ = 0
  // Using D = -(x²+y²) to move to right-hand side

  double sum_x = 0, sum_y = 0, sum_x2 = 0, sum_y2 = 0;
  double sum_xy = 0, sum_x3 = 0, sum_y3 = 0, sum_xy2 = 0, sum_x2y = 0;
  double sum_x2_y2 = 0.0;  // sum of (x² + y²)

  for (const auto &p : points) {
    double x = p.x;
    double y = p.y;
    double x2 = x * x;
    double y2 = y * y;
    double xy = x * y;
    double x2_y2 = x2 + y2;

    sum_x += x;
    sum_y += y;
    sum_x2 += x2;
    sum_y2 += y2;
    sum_xy += xy;
    sum_x3 += x2 * x;
    sum_y3 += y2 * y;
    sum_xy2 += x * y2;
    sum_x2y += x2 * y;
    sum_x2_y2 += x2_y2;
  }

  // Build the normal matrix (3x3 symmetric)
  double M11 = sum_x2;
  double M12 = sum_xy;
  double M13 = sum_x;
  double M22 = sum_y2;
  double M23 = sum_y;
  double M33 = static_cast<double>(n);

  // Build the right-hand side
  double b1 = -(sum_x3 + sum_xy2);
  double b2 = -(sum_x2y + sum_y3);
  double b3 = -sum_x2_y2;

  // Solve the 3x3 system M * [A, B, C]ᵀ = b
  // Using Cramer's rule or a small solver. Here we use a simple direct method.
  double det = M11 * (M22 * M33 - M23 * M23) - M12 * (M12 * M33 - M23 * M13) +
               M13 * (M12 * M23 - M22 * M13);

  if (std::abs(det) < 1e-10) {
    // Points are collinear or degenerate
    return std::make_unique<Ellipse>(0.0, 0.0, 0.0, 0.0, 0.0, typeLimits,
                                     spatialSystem, normState);
  }

  double invDet = 1.0 / det;

  double A = (b1 * (M22 * M33 - M23 * M23) - M12 * (b2 * M33 - M23 * b3) +
              M13 * (b2 * M23 - M22 * b3)) *
             invDet;

  double B = (M11 * (b2 * M33 - M23 * b3) - b1 * (M12 * M33 - M23 * M13) +
              M13 * (M12 * b3 - b2 * M13)) *
             invDet;

  double C = (M11 * (M22 * b3 - M23 * b2) - M12 * (M12 * b3 - M13 * b2) +
              b1 * (M12 * M23 - M22 * M13)) *
             invDet;

  double cx = -A / 2.0;
  double cy = -B / 2.0;
  double radius = std::sqrt(cx * cx + cy * cy - C);

  // Ensure radius is non‑negative (should be, but guard against numerical
  // issues)
  if (radius < 0.0)
    radius = 0.0;

  return std::make_unique<Ellipse>(radius, radius, cx, cy, 0.0, typeLimits,
                                   spatialSystem, normState);
}

std::unique_ptr<Ellipse> Ellipse::FitEllipse(const std::vector<Point> &points,
                                             TypeLimits typeLimits,
                                             CoordinateSystem spatialSystem,
                                             NormalizationState normState) {
  // Delegate to existing Ellipse(vector<Point>) constructor
  // This static method exists for API clarity and consistency with FitCircle
  return std::make_unique<Ellipse>(points, typeLimits, spatialSystem,
                                   normState);
}

}  // namespace aperture
