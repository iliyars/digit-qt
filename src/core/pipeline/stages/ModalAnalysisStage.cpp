#include "ModalAnalysisStage.h"

#include "core/ApertureSamples.h"
#include "core/Measurement.h"
#include "core/PolynomialBasis.h"
#include "core/SequentialSereginResult.h"

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
/// JointLeastSquares решает по этому базису совместный МНК без
/// ортогонализации (как и сам Seregin, только не по стадиям, а сразу для
/// всех выбранных термов -- корректный суммарный остаток при любом
/// подмножестве выбранных термов), а GramSchmidtOnAperture дополнительно
/// ортогонализует этот же базис численно по фактическим точкам апертуры
/// (см. doCompute) -- после чего коэффициенты уже не совпадают с DAPPSIM
/// напрямую, только суммарный остаток (RMS/PV) совпадает с
/// JointLeastSquares.
std::vector<TermDef> buildSereginHierarchy() {
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

/// Classical (textbook) unnormalized Zernike polynomials, in the SAME
/// pupil coordinates as buildSereginHierarchy() (x,y in [-1,1], edge at
/// radius 1) -- but WITHOUT Seregin's optical-Y-up sign flip: y here is
/// the raw sample coordinate (grows down the image), matching the
/// convention most external tools/generators use. That means a given
/// physical wavefront's tiltY/astigY/comaY/trefoilY come out with the
/// OPPOSITE sign here vs. buildSereginHierarchy() for the same data --
/// expected, not a bug (see notes/uchebnik-interferometriya-i-digitqt.md
/// §7.2 for the derivation connecting the two bases term-by-term).
///
/// Trefoil is the plain textbook form (x³-3xy², 3x²y-y³, no extra
/// scaling) -- verified algebraically to convert to Seregin's
/// trefoilX/Y with a clean ±1 factor, so no empirical correction is
/// needed here despite some external tools carrying one to match
/// DAPPSIM's own display (that discrepancy is between THAT tool and
/// DAPPSIM, not between Zernike and Seregin as implemented here).
std::vector<TermDef> buildZernikeHierarchy() {
  using digitqt::core::ModalCoefficients;
  using digitqt::core::ModalTermSelection;
  return {
      {[](double, double) { return 1.0; }, &ModalCoefficients::piston, nullptr},
      {[](double x, double) { return x; }, &ModalCoefficients::tiltX, &ModalTermSelection::tilt},
      {[](double, double y) { return y; }, &ModalCoefficients::tiltY, &ModalTermSelection::tilt},
      {[](double x, double y) { return 2.0 * (x * x + y * y) - 1.0; }, &ModalCoefficients::defocus,
       &ModalTermSelection::defocus},
      {[](double x, double y) { return x * x - y * y; }, &ModalCoefficients::astigX,
       &ModalTermSelection::astigmatism},
      {[](double x, double y) { return 2.0 * x * y; }, &ModalCoefficients::astigY,
       &ModalTermSelection::astigmatism},
      {[](double x, double y) { return (3.0 * (x * x + y * y) - 2.0) * x; },
       &ModalCoefficients::comaX, &ModalTermSelection::coma},
      {[](double x, double y) { return (3.0 * (x * x + y * y) - 2.0) * y; },
       &ModalCoefficients::comaY, &ModalTermSelection::coma},
      {[](double x, double y) { return x * x * x - 3.0 * x * y * y; },
       &ModalCoefficients::trefoilX, &ModalTermSelection::trefoil},
      {[](double x, double y) { return 3.0 * x * x * y - y * y * y; },
       &ModalCoefficients::trefoilY, &ModalTermSelection::trefoil},
      {[](double x, double y) {
         const double r2 = x * x + y * y;
         return 6.0 * r2 * r2 - 6.0 * r2 + 1.0;
       },
       &ModalCoefficients::spherical, &ModalTermSelection::spherical},
  };
}

std::vector<TermDef> buildTermHierarchy(digitqt::core::PolynomialBasis basis) {
  return basis == digitqt::core::PolynomialBasis::Zernike ? buildZernikeHierarchy()
                                                          : buildSereginHierarchy();
}

/// Прямая транскрипция последовательной схемы DAPPSIM (Includes/
/// Fitting.cpp, GetTiltSeregin -> GetPowerSeregin -> GetAstigSeregin ->
/// GetComaSeregin -> GetS3Seregin -> GetS5Seregin -> GetS7Seregin). См.
/// SequentialSereginResult.h и ModalFitMethod::SequentialSeregin про то,
/// что из результата подтверждено, а что нет. x/y -- нормализованные
/// пупильные координаты сэмплов (растёт вниз по строкам, как везде в
/// этом файле); интерсепт каждой локальной стадии-МНК (кроме самой
/// первой, tilt) намеренно отбрасывается и НЕ переносится в аккумулятор
/// -- ровно как в оригинале (Coeff_mnk.Get(0,0) там просто не читается).
struct SequentialFitOutput {
  digitqt::core::SequentialSereginResult coeffs;
  Eigen::VectorXd residual;
};

SequentialFitOutput fitSequentialSeregin(const Eigen::VectorXd &x, const Eigen::VectorXd &y,
                                          const Eigen::VectorXd &z) {
  using digitqt::core::SequentialSereginResult;
  const Eigen::Index n = z.size();
  SequentialSereginResult out;
  Eigen::VectorXd aber = Eigen::VectorXd::Zero(n);
  const Eigen::VectorXd ones = Eigen::VectorXd::Ones(n);

  auto rms = [&](const Eigen::VectorXd &r) {
    return std::sqrt(r.squaredNorm() / static_cast<double>(r.size()));
  };
  // Возвращаемый тип указан явно (Eigen::VectorXd), а не auto: без этого
  // лямбда возвращала бы ленивое выражение Eigen::Solve<...>, которое
  // хранит ссылку на ЛОКАЛЬНУЮ матрицу M -- к моменту, когда вызывающий
  // код читает результат, M уже разрушена (лямбда вернулась), и мы читаем
  // висячую ссылку. На практике это проявлялось как попытка выделить
  // ~400 ГБ памяти (мусорные размеры из разрушенного объекта) и падение
  // приложения. С явным типом возврата Eigen вычисляет результат В
  // конкретный VectorXd ДО выхода из лямбды, пока M ещё жива.
  auto solveStage = [&](const std::vector<Eigen::VectorXd> &cols) -> Eigen::VectorXd {
    const auto k = static_cast<Eigen::Index>(cols.size());
    Eigen::MatrixXd M(n, k);
    for (Eigen::Index c = 0; c < k; ++c)
      M.col(c) = cols[static_cast<size_t>(c)];
    const Eigen::VectorXd target = z - aber;
    return M.colPivHouseholderQr().solve(target);
  };

  out.rmsInitial = rms(z);

  // ВАЖНО: с стадии 2 и далее DAPPSIM (Includes/Fitting.cpp) считает
  // локальный МНК-интерсепт каждой стадии, но сохраняет в m_Coeff и
  // переносит в "aber" следующих стадий ТОЛЬКО перечисленные
  // коэффициенты -- сам интерсепт даже не читается (Coeff_mnk.Get(0,0)
  // просто отбрасывается). У ρ²/ρ⁴/ρ⁶/ρ⁸ ненулевое среднее по кругу,
  // поэтому этот интерсепт почти никогда не ноль -- если его не учитывать
  // вообще, остаток (RMS after) искусственно растёт и застревает,
  // хотя реальный DAPPSIM (и на практике, и по логике МНК) сходится к
  // ~0. Поэтому здесь интерсепт КАЖДОЙ стадии со 2-й и далее всё равно
  // накапливается в "aber" (чтобы остаток считался честно), просто ОТДЕЛЬНО
  // от репортируемых B0-B8-подобных коэффициентов -- те остаются точно
  // такими же, как если бы интерсепт был отброшен (и как подтверждено
  // побайтово против report.txt), их эта поправка не трогает.
  double discardedIntercepts = 0.0;

  // GetTiltSeregin: {1, x, -y} -- единственная стадия, где интерсепт
  // (пистон) действительно сохраняется и переносится дальше.
  const Eigen::VectorXd negY = -y;
  {
    const std::vector<Eigen::VectorXd> cols = {ones, x, negY};
    const Eigen::VectorXd c = solveStage(cols);
    out.piston = c(0);
    out.tiltX = c(1);
    out.tiltY = c(2);
    aber += out.piston * cols[0] + out.tiltX * cols[1] + out.tiltY * cols[2];
  }

  // GetPowerSeregin: {1, ρ²} -- в отчёт идёт только коэффициент при ρ²
  // (как у DAPPSIM), но интерсепт этой стадии всё равно добавляется в
  // aber, чтобы не терять его при подсчёте остатка (см. комментарий выше).
  const Eigen::VectorXd rho2 = x.array().square() + y.array().square();
  {
    const std::vector<Eigen::VectorXd> cols = {ones, rho2};
    const Eigen::VectorXd c = solveStage(cols);
    out.defocus = c(1);
    discardedIntercepts += c(0);
    aber += c(0) * ones + out.defocus * rho2;
  }
  out.rmsAfterTiltDefocus = rms(z - aber);

  // GetAstigSeregin: {1, x, -y, (3x²-1)/2, -xy, (3y²-1)/2} -- в отчёт
  // идут только последние 3 коэффициента, интерсепт -- в aber (см. выше).
  const Eigen::VectorXd a4 = (3.0 * x.array().square() - 1.0) / 2.0;
  const Eigen::VectorXd a5 = -(x.array() * y.array());
  const Eigen::VectorXd a6 = (3.0 * y.array().square() - 1.0) / 2.0;
  {
    const std::vector<Eigen::VectorXd> cols = {ones, x, negY, a4, a5, a6};
    const Eigen::VectorXd c = solveStage(cols);
    out.astig4 = c(3);
    out.astig5 = c(4);
    out.astig6 = c(5);
    discardedIntercepts += c(0);
    aber += c(0) * ones + out.astig4 * a4 + out.astig5 * a5 + out.astig6 * a6;
  }
  out.rmsAfterAstig = rms(z - aber);

  // GetComaSeregin: {1, x, -y, comaX, comaY, trefoilX, trefoilY} --
  // остаточные x/-y ЗДЕСЬ сохраняются и переносятся дальше (в отличие от
  // предыдущих стадий) -- ровно как в оригинале (m_Coeff[7]/[8] входят в
  // "aber" следующих стадий); интерсепт -- в aber отдельно (см. выше).
  const Eigen::VectorXd comaXCol =
      x.array().cube() + x.array() * y.array().square() - (2.0 / 3.0) * x.array();
  const Eigen::VectorXd comaYCol =
      (2.0 / 3.0) * y.array() - y.array() * (x.array().square() + y.array().square());
  const Eigen::VectorXd trefXCol = 3.0 * x.array() * y.array().square() - x.array().cube();
  const Eigen::VectorXd trefYCol = 3.0 * x.array().square() * y.array() - y.array().cube();
  {
    const std::vector<Eigen::VectorXd> cols = {ones, x, negY, comaXCol, comaYCol, trefXCol, trefYCol};
    const Eigen::VectorXd c = solveStage(cols);
    out.comaResidualX = c(1);
    out.comaResidualY = c(2);
    out.comaX = c(3);
    out.comaY = c(4);
    out.trefoilX = c(5);
    out.trefoilY = c(6);
    discardedIntercepts += c(0);
    aber += c(0) * ones + out.comaResidualX * cols[1] + out.comaResidualY * cols[2] +
            out.comaX * comaXCol + out.comaY * comaYCol + out.trefoilX * trefXCol +
            out.trefoilY * trefYCol;
  }
  out.rmsAfterComa = rms(z - aber);

  // GetS3Seregin: {1, ρ², ρ⁴} после вычитания ВСЕХ 12 предыдущих термов
  // (+ их интерсептов). Интерсепт этой стадии -- в aber отдельно.
  const Eigen::VectorXd rho4 = rho2.array().square();
  {
    const std::vector<Eigen::VectorXd> cols = {ones, rho2, rho4};
    const Eigen::VectorXd c = solveStage(cols);
    out.s3rho2 = c(1);
    out.s3rho4 = c(2);
    discardedIntercepts += c(0);
    aber += c(0) * ones + out.s3rho2 * rho2 + out.s3rho4 * rho4;
  }
  out.rmsAfterS3 = rms(z - aber);

  // GetS5Seregin: {1, ρ², ρ⁴, ρ⁶} после вычитания предыдущих 15 термов.
  const Eigen::VectorXd rho6 = rho4.array() * rho2.array();
  {
    const std::vector<Eigen::VectorXd> cols = {ones, rho2, rho4, rho6};
    const Eigen::VectorXd c = solveStage(cols);
    out.s5rho2 = c(1);
    out.s5rho4 = c(2);
    out.s5rho6 = c(3);
    discardedIntercepts += c(0);
    aber += c(0) * ones + out.s5rho2 * rho2 + out.s5rho4 * rho4 + out.s5rho6 * rho6;
  }
  out.rmsAfterS5 = rms(z - aber);

  // GetS7Seregin: {1, ρ², ρ⁴, ρ⁶, ρ⁸} после вычитания предыдущих 18 термов.
  const Eigen::VectorXd rho8 = rho4.array().square();
  {
    const std::vector<Eigen::VectorXd> cols = {ones, rho2, rho4, rho6, rho8};
    const Eigen::VectorXd c = solveStage(cols);
    out.s7rho2 = c(1);
    out.s7rho4 = c(2);
    out.s7rho6 = c(3);
    out.s7rho8 = c(4);
    discardedIntercepts += c(0);
    aber += c(0) * ones + out.s7rho2 * rho2 + out.s7rho4 * rho4 + out.s7rho6 * rho6 +
            out.s7rho8 * rho8;
  }
  out.rmsAfterS7 = rms(z - aber);
  out.discardedIntercepts = discardedIntercepts;

  return {out, z - aber};
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

  // Центр/радиус апертуры и сами сэмплы -- см. ApertureSamples.h/.cpp
  // (вынесено туда, чтобы внешние инструменты сравнения/диагностики
  // могли использовать РОВНО ту же нормализацию координат, что и эта
  // стадия, без риска рассинхронизации с копией логики).
  const digitqt::core::ApertureGeometry geometry =
      digitqt::core::computeApertureGeometry(wavefront);
  const int edgeErosionPixels = std::max(0, measurement.edgeErosionPixels());
  const std::vector<digitqt::core::ApertureSample> samples =
      digitqt::core::collectApertureSamples(wavefront, geometry, edgeErosionPixels);

  const auto selection = measurement.modalTermSelection();
  const auto method = measurement.modalFitMethod();
  const auto polynomialBasis = measurement.polynomialBasis();
  const auto n = static_cast<Eigen::Index>(samples.size());

  Eigen::VectorXd z(n);
  for (Eigen::Index i = 0; i < n; ++i)
    z(i) = samples[static_cast<size_t>(i)].z;

  digitqt::core::ModalCoefficients mc;  // невыбранные термы остаются 0.0 -- не подгонялись
  digitqt::core::SequentialSereginResult seqResult;  // заполняется только способом 3
  Eigen::VectorXd fit = Eigen::VectorXd::Zero(n);

  if (method == digitqt::core::ModalFitMethod::JointLeastSquares) {
    // --- Способ 1: обычный МНК на фиксированном базисе ---
    const auto hierarchy = buildTermHierarchy(polynomialBasis);

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
  } else if (method == digitqt::core::ModalFitMethod::SequentialSeregin) {
    // --- Способ 3: точная последовательная схема DAPPSIM ---
    // Игнорирует чекбоксы выбора термов (ModalTermSelection) -- сам
    // референсный алгоритм всегда подгоняет все 22 коэффициента сразу,
    // без возможности выборочно отключить терм; выбор нужен только для
    // способов 1/2. Базис (Seregin/Zernike) тоже не применяется -- это
    // ровно формулы Seregin, как в самом DAPPSIM.
    if (samples.size() < 22) {
      errorMessage = QStringLiteral("Not enough valid points to fit (need at least 22)");
      return false;
    }
    Eigen::VectorXd sx(n), sy(n);
    for (Eigen::Index i = 0; i < n; ++i) {
      sx(i) = samples[static_cast<size_t>(i)].x;
      sy(i) = samples[static_cast<size_t>(i)].y;
    }
    const SequentialFitOutput seq = fitSequentialSeregin(sx, sy, z);
    fit = z - seq.residual;

    mc.piston = seq.coeffs.piston;
    mc.tiltX = seq.coeffs.tiltX;
    mc.tiltY = seq.coeffs.tiltY;
    mc.defocus = seq.coeffs.defocus;
    // "Чистый" астигматизм -- антисимметричная часть двух Seregin-термов
    // (см. комментарий у ModalCoefficients::astigX); симметричная часть
    // (их полусумма) -- дефокусная примесь, здесь не переносится в
    // mc.defocus, чтобы mc.defocus оставался равен ИМЕННО побайтово
    // подтверждённому seq.defocus (см. SequentialSereginResult.h).
    mc.astigX = (seq.coeffs.astig4 - seq.coeffs.astig6) / 2.0;
    mc.astigY = seq.coeffs.astig5;
    mc.comaX = seq.coeffs.comaX;
    mc.comaY = seq.coeffs.comaY;
    mc.trefoilX = seq.coeffs.trefoilX;
    mc.trefoilY = seq.coeffs.trefoilY;
    mc.spherical = seq.coeffs.s3rho4;  // первый "настоящий" сферический терм (стадия S3)

    seqResult = seq.coeffs;
  } else {
    // --- Способ 2: Грам-Шмидт по фактическим точкам апертуры ---
    const auto hierarchy = buildTermHierarchy(polynomialBasis);
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
  result.basis = polynomialBasis;
  result.sequential = seqResult;
  result.residual = std::move(residual);
  result.rmsBefore = std::sqrt(sumSqBefore / static_cast<double>(samples.size()));
  result.rmsAfter = std::sqrt(sumSqAfter / static_cast<double>(samples.size()));

  measurement.modalAnalysis() = std::move(result);
  return true;
}

}  // namespace digitqt::core::pipeline
