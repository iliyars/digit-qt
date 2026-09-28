#pragma once

#include "core/AffineTransform2D.h"
#include "core/Fiducial.h"

#include <string>
#include <vector>

namespace digitqt::core {

/**
 * @brief Результат подгонки G2 по реперам -- аналог TransformFit из
 * architecture.md (без "rejected fiducials": здесь всё, что не подошло по
 * форме входных данных, просто не участвует, а не отбраковывается по
 * невязке -- отбраковка выбросов сознательно не реализована в этой
 * версии).
 */
struct FiducialTransformFit {
  bool ok = false;
  std::string errorMessage;

  AffineTransform2D transform;

  /// По одному на каждый использованный репер (тот, у кого hasReference
  /// == true). residual -- расстояние между transform.apply(imageX,
  /// imageY) и (referenceX, referenceY) в единицах эталонной СК.
  struct Residual {
    int fiducialId = 0;
    double residual = 0.0;
  };
  std::vector<Residual> residuals;

  double rmsError = 0.0;
  double maxError = 0.0;
  size_t usedCount = 0;
};

/**
 * @brief Подгоняет G2 (СКИ -> СКОС) как аффинное преобразование по всем
 * реперам с hasReference == true. Нужно минимум 3 таких репера, не лежащих
 * на одной прямой (иначе errorMessage объяснит, чего не хватило).
 */
FiducialTransformFit fitFiducialTransform(const std::vector<Fiducial> &fiducials);

}  // namespace digitqt::core
