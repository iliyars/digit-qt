#include "MiddleAlgorithm.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace digitqt::core::tracing::scanline_extremum {

namespace {

/// Zeroes pixels between [end1, end2) that fall below the linear
/// background line y = aa*x + bb. Port of the original delet_u().
void applyLinearBackground(uint8_t *line, int end1, int end2, double aa,
                           double bb) {
  for (int j = end1; j < end2; ++j) {
    const double background = j * aa + bb;
    if (background >= static_cast<double>(line[j]))
      line[j] = 0;
  }
}

} // namespace

void removeBackground(uint8_t *line, int leftIdx, int rightIdx) {
  const int segmentLength = rightIdx - leftIdx;
  constexpr int kLongSegmentThreshold = 70;
  constexpr int kNumParts = 5;

  if (segmentLength > kLongSegmentThreshold) {
    // 1. Partition boundaries (inclusive end indices of each part).
    const double partWidth = segmentLength / static_cast<double>(kNumParts);
    int partEnds[kNumParts];
    for (int i = 0; i < kNumParts - 1; ++i)
      partEnds[i] = static_cast<int>((i + 1) * partWidth) + leftIdx;
    partEnds[kNumParts - 1] = rightIdx;

    // 2. Average intensity of non-zero pixels in each part.
    int partMeans[kNumParts] = {0};
    int currentPos = leftIdx;
    for (int part = 0; part < kNumParts; ++part) {
      int sum = 0;
      int count = 0;
      while (currentPos <= partEnds[part]) {
        const uint8_t pixel = line[currentPos];
        if (pixel != 0) {
          sum += static_cast<int>(pixel);
          ++count;
        }
        ++currentPos;
      }
      if (count != 0)
        partMeans[part] = static_cast<int>(sum / static_cast<double>(count));
    }

    // 3. Midpoints of each part (X-coordinates for the means).
    int midPoints[kNumParts];
    for (int i = 0; i < kNumParts; ++i)
      midPoints[i] =
          static_cast<int>(i * partWidth + partWidth / 2.0 + leftIdx + 0.5);

    // 4. Fit a line between each pair of consecutive midpoints.
    double slopes[kNumParts - 1];
    double intercepts[kNumParts - 1];
    for (int i = 0; i < kNumParts - 1; ++i) {
      const double dx = static_cast<double>(midPoints[i + 1] - midPoints[i]);
      slopes[i] = (dx != 0.0) ? (partMeans[i + 1] - partMeans[i]) / dx : 0.0;
      intercepts[i] = partMeans[i] - slopes[i] * midPoints[i];
    }

    // 5. Apply background subtraction piecewise.
    applyLinearBackground(line, leftIdx, midPoints[1], slopes[0],
                          intercepts[0]);
    applyLinearBackground(line, midPoints[1], midPoints[2], slopes[1],
                          intercepts[1]);
    applyLinearBackground(line, midPoints[2], midPoints[3], slopes[2],
                          intercepts[2]);
    applyLinearBackground(line, midPoints[3], rightIdx, slopes[3],
                          intercepts[3]);
  } else {
    // Short segment: one constant background = mean of non-zero pixels.
    int sum = 0;
    int count = 0;
    for (int pos = leftIdx; pos <= rightIdx; ++pos) {
      const uint8_t pixel = line[pos];
      if (pixel != 0) {
        sum += static_cast<int>(pixel);
        ++count;
      }
    }
    const double meanBackground =
        (count != 0) ? static_cast<double>(sum) / count : 0.0;
    applyLinearBackground(line, leftIdx, rightIdx, 0.0, meanBackground);
  }
}

double fitQuadraticPeak(int *n, int *x, int *y) {
  const int N = *n;
  if (N < 3) {
    *n = -1; // Not enough points
    return 0.0;
  }

  double Sx = 0.0, Sx2 = 0.0, Sx3 = 0.0, Sx4 = 0.0;
  double Sy = 0.0, Syx = 0.0, Syx2 = 0.0;

  for (int i = 0; i < N; ++i) {
    const double xi = static_cast<double>(x[i]);
    const double yi = static_cast<double>(y[i]);
    const double xi2 = xi * xi;

    Sx += xi;
    Sx2 += xi2;
    Sx3 += xi2 * xi;
    Sx4 += xi2 * xi2;
    Sy += yi;
    Syx += yi * xi;
    Syx2 += yi * xi2;
  }

  double A[3][3] = {
      {static_cast<double>(N), Sx, Sx2},
      {Sx, Sx2, Sx3},
      {Sx2, Sx3, Sx4},
  };
  double b[3] = {Sy, Syx, Syx2};

  // 3x3 Gaussian elimination with partial pivoting.
  constexpr double kEps = 1e-12;
  int pivotRow = 0;

  for (int col = 0; col < 3; ++col) {
    int maxRow = pivotRow;
    double maxVal = std::fabs(A[pivotRow][col]);
    for (int row = pivotRow + 1; row < 3; ++row) {
      if (std::fabs(A[row][col]) > maxVal) {
        maxVal = std::fabs(A[row][col]);
        maxRow = row;
      }
    }

    if (maxVal < kEps) {
      *n = -2; // Singular matrix
      return 0.0;
    }

    if (maxRow != pivotRow) {
      std::swap(A[pivotRow], A[maxRow]);
      std::swap(b[pivotRow], b[maxRow]);
    }

    for (int row = pivotRow + 1; row < 3; ++row) {
      const double factor = A[row][col] / A[pivotRow][col];
      for (int k = col; k < 3; ++k)
        A[row][k] -= factor * A[pivotRow][k];
      b[row] -= factor * b[pivotRow];
    }

    ++pivotRow;
  }

  double c[3] = {0.0, 0.0, 0.0};
  for (int row = 2; row >= 0; --row) {
    double sum = b[row];
    for (int k = row + 1; k < 3; ++k)
      sum -= A[row][k] * c[k];
    if (std::fabs(A[row][row]) < kEps) {
      *n = -2;
      return 0.0;
    }
    c[row] = sum / A[row][row];
  }

  // Quadratic coefficient must be non-zero for a valid peak.
  if (std::fabs(c[2]) < kEps) {
    *n = -3;
    return 0.0;
  }

  const double vertex = -c[1] / (2.0 * c[2]);
  return vertex + 0.5; // sub-pixel position, original convention
}

std::vector<double>
detectPeaks(const uint8_t *line, std::size_t nx, int y,
            const std::function<bool(int, int)> &isVisible) {
  std::vector<double> results;
  std::size_t i = 0;

  while (i < nx) {
    if (line[i] != 0 && isVisible(static_cast<int>(i), y)) {
      const std::size_t start = i;
      while (i < nx && line[i] != 0 && isVisible(static_cast<int>(i), y))
        ++i;
      const std::size_t end = i; // exclusive

      std::array<int, 300> ax{}, ay{};
      int maxVal = 0, firstMaxIdx = 0, n = 0;
      for (std::size_t j = start; j < end && n < 300; ++j) {
        const int val = line[j];
        if (val > maxVal) {
          maxVal = val;
          firstMaxIdx = static_cast<int>(j);
        }
        ay[static_cast<size_t>(n)] = val;
        ax[static_cast<size_t>(n)] = static_cast<int>(j);
        ++n;
      }

      if (n < 3)
        continue; // not enough points for a reliable estimate

      double tmpf = 0.0;
      int half = firstMaxIdx;

      if (n > 4) {
        int nLocal = n;
        tmpf = fitQuadraticPeak(&nLocal, ax.data(), ay.data());
        if (nLocal < 0)
          continue; // fit failed
        n = nLocal;

        if (tmpf > ax[0] && tmpf < ax[static_cast<size_t>(n - 1)])
          half = std::lround(tmpf);
        else
          tmpf = half + 0.5;
      } else {
        // Short run: treat as a flat plateau.
        int kol = half;
        while (kol < static_cast<int>(end) && line[kol] == maxVal)
          ++kol;
        half += (kol - half) / 2;
        tmpf = half + 0.5;
      }

      // Reject if the estimated position coincides with run boundaries.
      if (half == ax[0] || half == ax[static_cast<size_t>(n - 1)])
        continue;

      results.push_back(tmpf);
    } else {
      ++i;
    }
  }

  return results;
}

} // namespace digitqt::core::tracing::scanline_extremum
