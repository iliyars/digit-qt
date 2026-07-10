/**
 * @file PhaseReconstructor.cpp
 * @brief Решение уравнения Лапласа для восстановления фазы между
 * пронумерованными линиями полос. По мотивам WavefrontSolver
 * /IsoLinesToTopogram из оригинального проекта Digit.
 */
#include "PhaseReconstructor.h"

#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>
#include <algorithm>
#include <cmath>
#include <vector>


namespace digitqt::core::pipeline {

namespace {

/// Растеризация отрезка (алгоритм Брезенхэма) со значением value в
/// каждый проходимый пиксель сетки known (если он виден).
void rasterizeSegment(std::vector<double> &known, const std::vector<char> &visible, int width,
                      int height, int x0, int y0, int x1, int y1, double value) {
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = (x0 < x1) ? 1 : -1;
  const int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  int x = x0, y = y0;
  while (true) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
      const size_t idx =
          static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
      if (visible[idx])
        known[idx] = value;
    }
    if (x == x1 && y == y1)
      break;
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y += sy;
    }
  }
}

}  // namespace

PhaseMap PhaseReconstructor::reconstruct(int width, int height,
                                         const std::function<bool(int, int)> &isVisible,
                                         const std::vector<NumberedFringeLine> &lines,
                                         const PhaseReconstructionParams &params) {
  m_lastError.clear();

  if (width <= 0 || height <= 0) {
    m_lastError = QStringLiteral("Invalid grid size");
    return {};
  }

  const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);

  // 1. Маска видимости на сетке решения.
  std::vector<char> visible(n, 0);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (isVisible(x, y))
        visible[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = 1;

  // 2. Растеризация линий -- известные (Dirichlet) значения.
  std::vector<double> known(n, std::numeric_limits<double>::quiet_NaN());
  bool anyKnown = false;
  for (const auto &line : lines) {
    const auto &pts = line.points;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
      const int x0 = static_cast<int>(std::lround(pts[i].x));
      const int y0 = static_cast<int>(std::lround(pts[i].y));
      const int x1 = static_cast<int>(std::lround(pts[i + 1].x));
      const int y1 = static_cast<int>(std::lround(pts[i + 1].y));
      rasterizeSegment(known, visible, width, height, x0, y0, x1, y1, line.order);
      anyKnown = true;
    }
    if (pts.size() == 1) {
      const int x0 = static_cast<int>(std::lround(pts[0].x));
      const int y0 = static_cast<int>(std::lround(pts[0].y));
      if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
        const size_t idx =
            static_cast<size_t>(y0) * static_cast<size_t>(width) + static_cast<size_t>(x0);
        if (visible[idx]) {
          known[idx] = line.order;
          anyKnown = true;
        }
      }
    }
  }

  if (!anyKnown) {
    m_lastError = QStringLiteral("No numbered fringe lines to reconstruct from");
    return {};
  }

  // 3. Индексация неизвестных (видимых, но не известных) пикселей.
  std::vector<long> unknownIndex(n, -1);
  long unknownCount = 0;
  for (size_t i = 0; i < n; ++i) {
    if (visible[i] && !(known[i] == known[i]))  // NaN != NaN
      unknownIndex[i] = unknownCount++;
  }

  PhaseMap result(width, height);

  // Известные пиксели переносим в результат сразу.
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const size_t idx =
          static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
      if (visible[idx] && known[idx] == known[idx])
        result.setValue(x, y, known[idx]);
    }

  if (unknownCount == 0)
    return result;  // все видимые пиксели уже известны (или апертура пуста)

  // 4. Сборка разреженной матрицы уравнения Лапласа: для каждого
  // неизвестного пикселя p с соседями внутри апертуры:
  //   (число видимых соседей) * z_p - sum(z_соседей-неизвестных) =
  //     sum(значений соседей-известных)
  // Соседи вне апертуры просто не учитываются -- это и есть
  // естественное условие Неймана на краю апертуры.
  using SpMat = Eigen::SparseMatrix<double>;
  std::vector<Eigen::Triplet<double>> triplets;
  triplets.reserve(static_cast<size_t>(unknownCount) * 5);
  Eigen::VectorXd rhs = Eigen::VectorXd::Zero(unknownCount);

  static const int dx4[4] = {1, -1, 0, 0};
  static const int dy4[4] = {0, 0, 1, -1};

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t idx =
          static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
      const long row = unknownIndex[idx];
      if (row < 0)
        continue;

      double diagonal = 0.0;
      for (int k = 0; k < 4; ++k) {
        const int nx = x + dx4[k];
        const int ny = y + dy4[k];
        if (nx < 0 || nx >= width || ny < 0 || ny >= height)
          continue;
        const size_t nIdx =
            static_cast<size_t>(ny) * static_cast<size_t>(width) + static_cast<size_t>(nx);
        if (!visible[nIdx])
          continue;

        diagonal += 1.0;
        if (known[nIdx] == known[nIdx]) {
          rhs(row) += known[nIdx];
        } else {
          triplets.emplace_back(row, unknownIndex[nIdx], -1.0);
        }
      }

      if (diagonal <= 0.0) {
        // Полностью изолированный пиксель (нет ни одного видимого
        // соседа) -- избегаем вырожденной строки, фиксируем в 0.
        diagonal = 1.0;
        rhs(row) = 0.0;
      }
      triplets.emplace_back(row, row, diagonal);
    }
  }

  SpMat A(unknownCount, unknownCount);
  A.setFromTriplets(triplets.begin(), triplets.end());

  Eigen::ConjugateGradient<SpMat, Eigen::Lower | Eigen::Upper> solver;
  solver.setTolerance(params.cgTolerance);
  solver.setMaxIterations(params.maxIterations);
  solver.compute(A);
  const Eigen::VectorXd solution = solver.solve(rhs);

  // 5. Записываем решение в результат (даже если решатель не сошёлся
  // до заданного допуска -- частично сошедшийся результат обычно всё
  // ещё осмысленный, а не фатальная ошибка).
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t idx =
          static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
      const long row = unknownIndex[idx];
      if (row >= 0)
        result.setValue(x, y, solution(row));
    }
  }

  return result;
}

}  // namespace digitqt::core::pipeline
