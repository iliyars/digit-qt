#include "ModalAnalysisStage.h"

#include "core/Measurement.h"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <vector>

namespace digitqt::core::pipeline {

namespace {

/// Один член базиса: функция от нормализованных пупильных координат +
/// указатель на коэффициент в ModalCoefficients + указатель на флажок в
/// ModalTermSelection, который решает, входит ли термин в подгонку
/// (nullptr для пистона -- он убирается всегда).
struct TermDef {
  double (*basis)(double x, double y);
  double digitqt::core::ModalCoefficients::*coeff;
  bool digitqt::core::ModalTermSelection::*flag;
};

/// Полная иерархия термов в фиксированном порядке (по возрастанию
/// степени полинома; внутри одной степени -- в порядке, в котором члены
/// перечислены в ModalCoefficients). Порядок здесь задаёт порядок
/// ортогонализации Грама-Шмидта ниже -- поэтому важно, чтобы термы
/// низкого порядка (пистон, наклон) шли раньше членов, с которыми они
/// геометрически коррелируют на реальной (не обязательно круглой)
/// апертуре: дефокус/сферическая (обе -- функции только r²) и
/// наклон/кома (обе линейны по x или y на оси).
std::vector<TermDef> buildTermHierarchy() {
  using digitqt::core::ModalCoefficients;
  using digitqt::core::ModalTermSelection;
  return {
      {[](double, double) { return 1.0; }, &ModalCoefficients::piston, nullptr},
      {[](double x, double) { return x; }, &ModalCoefficients::tiltX, &ModalTermSelection::tilt},
      {[](double, double y) { return y; }, &ModalCoefficients::tiltY, &ModalTermSelection::tilt},
      {[](double x, double y) { return x * x + y * y; }, &ModalCoefficients::defocus,
       &ModalTermSelection::defocus},
      {[](double x, double y) { return x * x - y * y; }, &ModalCoefficients::astigX,
       &ModalTermSelection::astigmatism},
      {[](double x, double y) { return x * y; }, &ModalCoefficients::astigY,
       &ModalTermSelection::astigmatism},
      {[](double x, double y) { return x * (x * x + y * y); }, &ModalCoefficients::comaX,
       &ModalTermSelection::coma},
      {[](double x, double y) { return y * (x * x + y * y); }, &ModalCoefficients::comaY,
       &ModalTermSelection::coma},
      {[](double x, double y) { return x * x * x - 3.0 * x * y * y; }, &ModalCoefficients::trefoilX,
       &ModalTermSelection::trefoil},
      {[](double x, double y) { return 3.0 * x * x * y - y * y * y; }, &ModalCoefficients::trefoilY,
       &ModalTermSelection::trefoil},
      {[](double x, double y) {
         const double r2 = x * x + y * y;
         return r2 * r2;
       },
       &ModalCoefficients::spherical, &ModalTermSelection::spherical},
  };
}

bool isActive(const TermDef &term, const digitqt::core::ModalTermSelection &sel) {
  return term.flag == nullptr || sel.*(term.flag);
}

}  // namespace

bool ModalAnalysisStage::doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) {
  const auto &wavefront = measurement.wavefrontMap();
  if (wavefront.isEmpty()) {
    errorMessage = QStringLiteral("No wavefront map. Run Wavefront Reconstruction (S4) first.");
    return false;
  }

  const int w = wavefront.width();
  const int h = wavefront.height();

  // Центр и радиус апертуры (не изображения!) -- "зрачковые координаты",
  // где край апертуры всегда ровно at radius=1, как принято в оптике.
  // Раньше нормировали по размеру картинки; если апертура не занимает
  // всё изображение (обычно так и есть), это давало ложную крутизну
  // членов высокой степени (кома/трилистник/сферическая) ближе к
  // фактическому краю апертуры -- полиномы высокой степени особенно
  // чувствительны к такой ошибке масштаба у границы.
  //
  // Берём охват НЕПОСРЕДСТВЕННО из самой карты (bounding box непустых
  // пикселей), а не из Measurement::boundaries() -- те заданы в
  // координатах полного изображения, а карта хранится на уменьшенной
  // сетке (см. PhaseReconstructionStage), так что координаты были бы
  // не в том масштабе.
  int minX = w, maxX = -1, minY = h, maxY = -1;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!wavefront.hasValue(x, y))
        continue;
      minX = std::min(minX, x);
      maxX = std::max(maxX, x);
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
    }
  }

  double centerX = w / 2.0, centerY = h / 2.0;
  double radius = std::max(w, h) / 2.0;
  if (maxX >= minX && maxY >= minY) {
    centerX = (minX + maxX) / 2.0;
    centerY = (minY + maxY) / 2.0;
    radius = std::max(maxX - minX, maxY - minY) / 2.0;
  }
  if (radius <= 0.0)
    radius = std::max(w, h) / 2.0;

  // Нормализованные координаты (x, y в [-1, 1] относительно апертуры),
  // чтобы подгонка была численно устойчива независимо от абсолютного
  // размера картинки в пикселях.
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
      const double nx = (x - centerX) / radius;
      const double ny = (y - centerY) / radius;
      samples.push_back({nx, ny, wavefront.value(x, y), x, y});
    }
  }

  const auto selection = measurement.modalTermSelection();
  const auto hierarchy = buildTermHierarchy();

  const auto activeCount = std::count_if(
      hierarchy.begin(), hierarchy.end(), [&](const TermDef &t) { return isActive(t, selection); });
  if (static_cast<size_t>(samples.size()) < static_cast<size_t>(activeCount)) {
    errorMessage =
        QStringLiteral("Not enough valid points to fit (need at least %1)").arg(activeCount);
    return false;
  }

  const auto n = static_cast<Eigen::Index>(samples.size());
  const auto numTerms = static_cast<Eigen::Index>(hierarchy.size());

  // Численная ортогонализация Грама-Шмидта базиса ПРЯМО по фактическим
  // точкам апертуры (а не аналитическими формулами Цернике для
  // идеального круга -- апертура здесь произвольной формы, с
  // возможными препятствиями/вырезами, так что даже "учебные" формулы
  // Цернике не были бы на ней ортогональны).
  //
  // Термы обрабатываются в порядке buildTermHierarchy() -- от пистона к
  // членам высокого порядка -- так что каждый следующий термин
  // избавляется от проекции на все предыдущие. Итог: члены одной
  // "радиальной семьи" (пистон/дефокус/сферическая; наклон/кома) больше
  // не коррелируют друг с другом, и включение/выключение чекбокса одного
  // термина в ParametersDock (см. modalTermSelection()) не меняет
  // коэффициент, вычисленный для другого -- каждый коэффициент есть
  // независимая проекция на свой (уже ортогональный) базисный вектор.
  //
  // Ведущий моном каждого термина ортогонализацией не трогается (у него
  // всегда коэффициент 1, вычитаются только проекции младших термов),
  // поэтому итоговые коэффициенты остаются в тех же физических единицах
  // (нм), что и раньше.
  std::vector<Eigen::VectorXd> basis(static_cast<size_t>(numTerms), Eigen::VectorXd(n));
  for (Eigen::Index t = 0; t < numTerms; ++t) {
    auto &col = basis[static_cast<size_t>(t)];
    const auto &term = hierarchy[static_cast<size_t>(t)];
    for (Eigen::Index i = 0; i < n; ++i)
      col(i) = term.basis(samples[static_cast<size_t>(i)].x, samples[static_cast<size_t>(i)].y);
  }
  for (Eigen::Index t = 0; t < numTerms; ++t) {
    auto &col = basis[static_cast<size_t>(t)];
    for (Eigen::Index p = 0; p < t; ++p) {
      const auto &prev = basis[static_cast<size_t>(p)];
      const double denom = prev.squaredNorm();
      if (denom > 1e-12)
        col -= (col.dot(prev) / denom) * prev;
    }
  }

  Eigen::VectorXd z(n);
  for (Eigen::Index i = 0; i < n; ++i)
    z(i) = samples[static_cast<size_t>(i)].z;

  digitqt::core::ModalCoefficients mc;  // невыбранные термы остаются 0.0 -- не подгонялись
  Eigen::VectorXd fit = Eigen::VectorXd::Zero(n);
  for (Eigen::Index t = 0; t < numTerms; ++t) {
    const auto &term = hierarchy[static_cast<size_t>(t)];
    if (!isActive(term, selection))
      continue;
    const auto &col = basis[static_cast<size_t>(t)];
    const double denom = col.squaredNorm();
    if (denom <= 1e-12)
      continue;  // вырожденный термин на этой апертуре (например, апертура -- почти линия)
    const double coeff = col.dot(z) / denom;
    mc.*(term.coeff) = coeff;
    fit += coeff * col;
  }

  digitqt::core::PhaseMap residual(w, h);
  double sumSqBefore = 0.0;
  double sumSqAfter = 0.0;
  for (Eigen::Index i = 0; i < n; ++i) {
    const auto &s = samples[static_cast<size_t>(i)];
    const double r = z(i) - fit(i);
    residual.setValue(s.px, s.py, r);
    sumSqBefore += s.z * s.z;
    sumSqAfter += r * r;
  }

  digitqt::core::ModalAnalysisResult result;
  result.coefficients = mc;
  result.selection = selection;
  result.residual = std::move(residual);
  result.rmsBefore = std::sqrt(sumSqBefore / static_cast<double>(samples.size()));
  result.rmsAfter = std::sqrt(sumSqAfter / static_cast<double>(samples.size()));

  measurement.modalAnalysis() = std::move(result);
  return true;
}

}  // namespace digitqt::core::pipeline
