#include "ModalAnalysisStage.h"

#include "core/Measurement.h"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <vector>

namespace digitqt::core::pipeline {

namespace {

/// Один член базиса: функция от нормализованных пупильных координат +
/// указатель, куда записать подогнанный коэффициент в ModalCoefficients,
/// + указатель на чекбокс выбора (nullptr для пистона -- он убирается
/// всегда, отдельно не выбирается, как и в референсном инструменте).
struct TermDef {
  double (*basis)(double x, double y);
  double digitqt::core::ModalCoefficients::*coeff;
  bool digitqt::core::ModalTermSelection::*flag;
};

bool isActive(const TermDef &term, const digitqt::core::ModalTermSelection &sel) {
  return term.flag == nullptr || sel.*(term.flag);
}

/// Базис термов -- прямая транскрипция полиномов референсного
/// инструмента (DAPPSIM, Includes/Fitting.cpp, метод Seregin:
/// GetTiltSeregin/GetPowerSeregin/GetAstigSeregin/GetComaSeregin/
/// GetS3Seregin), а не "учебниковых" нормированных полиномов Цернике --
/// коэффициенты выходят в тех же единицах и с тем же знаком, что и у
/// референсного инструмента.
///
/// DAPPSIM переворачивает ось Y перед вычислением термов
/// (Y = -(y-y0)/r0 -- оптическое соглашение "Y вверх", тогда как
/// нормализованная координата сэмпла y здесь растёт вниз по строкам
/// изображения); тот же переворот сделан прямо в формулах ниже (там, где
/// Y входит в нечётной степени: tiltY, astigY, comaY, trefoilY), не
/// трогая сами координаты сэмплов.
///
/// Астигматизм у DAPPSIM/Seregin на самом деле подгоняется тремя
/// коэффициентами ((3X²-1)/2, XY, (3Y²-1)/2), а не двумя -- первый и
/// третий не ортогональны дефокусу (в сумме дают ещё один
/// дефокус-подобный член), это исторический артефакт последовательной (а
/// не Грам-Шмидт) схемы подгонки DAPPSIM. Здесь вместо третьего
/// коэффициента используется разность двух Seregin-термов
/// ((3X²-1)/2 - (3Y²-1)/2 = 1.5(X²-Y²)) -- эквивалент "чистого"
/// астигматизма без дефокусной примеси; численно совпадает с суммой
/// соответствующих коэффициентов Seregin, если их собственная дефокусная
/// примесь в данных мала (как и должно быть). Крест-терм (XY, с тем же
/// переворотом Y) переносится как есть.
///
/// Используется в обоих способах (см. Measurement::modalFitMethod()):
/// AnalyticZernike решает по этому базису совместный МНК без
/// ортогонализации (как и сам Seregin, только не по стадиям, а сразу для
/// всех выбранных термов -- корректный суммарный остаток при любом
/// подмножестве выбранных термов), а GramSchmidtOnAperture дополнительно
/// ортогонализует этот же базис численно по фактическим точкам апертуры
/// (см. doCompute) -- после чего коэффициенты уже не совпадают с DAPPSIM
/// напрямую, только суммарный остаток (RMS/PV) совпадает с
/// AnalyticZernike.
std::vector<TermDef> buildTermHierarchy() {
  using digitqt::core::ModalCoefficients;
  using digitqt::core::ModalTermSelection;
  return {
      {[](double, double) { return 1.0; }, &ModalCoefficients::piston, nullptr},
      {[](double x, double) { return x; }, &ModalCoefficients::tiltX, &ModalTermSelection::tilt},
      {[](double, double y) { return -y; }, &ModalCoefficients::tiltY, &ModalTermSelection::tilt},
      {[](double x, double y) { return x * x + y * y; }, &ModalCoefficients::defocus,
       &ModalTermSelection::defocus},
      {[](double x, double y) { return 1.5 * (x * x - y * y); }, &ModalCoefficients::astigX,
       &ModalTermSelection::astigmatism},
      {[](double x, double y) { return -x * y; }, &ModalCoefficients::astigY,
       &ModalTermSelection::astigmatism},
      {[](double x, double y) { return x * x * x + x * y * y - (2.0 / 3.0) * x; },
       &ModalCoefficients::comaX, &ModalTermSelection::coma},
      {[](double x, double y) { return (2.0 / 3.0) * y - y * (x * x + y * y); },
       &ModalCoefficients::comaY, &ModalTermSelection::coma},
      {[](double x, double y) { return 3.0 * x * y * y - x * x * x; },
       &ModalCoefficients::trefoilX, &ModalTermSelection::trefoil},
      {[](double x, double y) { return 3.0 * x * x * y - y * y * y; },
       &ModalCoefficients::trefoilY, &ModalTermSelection::trefoil},
      {[](double x, double y) {
         const double r2 = x * x + y * y;
         return r2 * r2;
       },
       &ModalCoefficients::spherical, &ModalTermSelection::spherical},
  };
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

  // Нормализованные координаты (x, y в [-1, 1] относительно апертуры).
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
  const auto method = measurement.modalFitMethod();
  const auto n = static_cast<Eigen::Index>(samples.size());

  Eigen::VectorXd z(n);
  for (Eigen::Index i = 0; i < n; ++i)
    z(i) = samples[static_cast<size_t>(i)].z;

  digitqt::core::ModalCoefficients mc;  // невыбранные термы остаются 0.0 -- не подгонялись
  Eigen::VectorXd fit = Eigen::VectorXd::Zero(n);

  if (method == digitqt::core::ModalFitMethod::AnalyticZernike) {
    // --- Способ 1: обычный МНК на фиксированном базисе Seregin ---
    const auto hierarchy = buildTermHierarchy();

    std::vector<const TermDef *> active;
    for (const auto &t : hierarchy)
      if (isActive(t, selection))
        active.push_back(&t);

    if (samples.size() < active.size()) {
      errorMessage =
          QStringLiteral("Not enough valid points to fit (need at least %1)").arg(active.size());
      return false;
    }

    const auto numActive = static_cast<Eigen::Index>(active.size());
    Eigen::MatrixXd A(n, numActive);
    for (Eigen::Index i = 0; i < n; ++i) {
      const auto &s = samples[static_cast<size_t>(i)];
      for (Eigen::Index t = 0; t < numActive; ++t)
        A(i, t) = active[static_cast<size_t>(t)]->basis(s.x, s.y);
    }

    const Eigen::VectorXd coeffs = A.colPivHouseholderQr().solve(z);
    for (Eigen::Index t = 0; t < numActive; ++t)
      mc.*(active[static_cast<size_t>(t)]->coeff) = coeffs(t);
    fit = A * coeffs;
  } else {
    // --- Способ 2: Грам-Шмидт по фактическим точкам апертуры ---
    const auto hierarchy = buildTermHierarchy();
    const auto numTerms = static_cast<Eigen::Index>(hierarchy.size());

    const auto activeCount =
        std::count_if(hierarchy.begin(), hierarchy.end(),
                      [&](const TermDef &t) { return isActive(t, selection); });
    if (static_cast<Eigen::Index>(samples.size()) < activeCount) {
      errorMessage =
          QStringLiteral("Not enough valid points to fit (need at least %1)").arg(activeCount);
      return false;
    }

    // Термы обрабатываются в фиксированном порядке (пистон -> наклон
    // -> ... -> сферическая); каждый следующий избавляется от
    // проекции на все предыдущие, так что члены одной "радиальной
    // семьи" (пистон/дефокус/сферическая; наклон/кома) перестают
    // коррелировать друг с другом на этой конкретной апертуре.
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

    for (Eigen::Index t = 0; t < numTerms; ++t) {
      const auto &term = hierarchy[static_cast<size_t>(t)];
      if (!isActive(term, selection))
        continue;  // остаётся невычтенным в остатке, коэффициент не репортится
      const auto &col = basis[static_cast<size_t>(t)];
      const double denom = col.squaredNorm();
      if (denom <= 1e-12)
        continue;  // вырожденный термин на этой апертуре
      const double coeff = col.dot(z) / denom;
      mc.*(term.coeff) = coeff;
      fit += coeff * col;
    }
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
