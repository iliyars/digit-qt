#include "PhaseShifting.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace digitqt::core::psi {

namespace {

// Numerical guards for the current 8-bit data model. Neither is an instrument
// accuracy specification: the S2 stage additionally applies its modulation mask.
constexpr double kMaxFitCondition = 1e4;
constexpr double kMinPhaseAmplitude = 1.0;

bool allFinite(const std::vector<double> &values) {
  return std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); });
}

bool checkFrames(const std::vector<Bitmap> &frames, std::string &error) {
  if (frames.size() < 3) {
    error = "Phase shifting needs at least 3 frames, got " + std::to_string(frames.size());
    return false;
  }
  for (const Bitmap &frame : frames) {
    if (frame.isNull() || frame.width() != frames[0].width() ||
        frame.height() != frames[0].height()) {
      error = "All frames must be non-empty and of the same size";
      return false;
    }
  }
  return true;
}

/// Модель шага 1 при известных сдвигах: строки [1, cos(delta_n),
/// -sin(delta_n)]. Factor A itself once with SVD, never A^T*A: forming the
/// normal matrix squares its condition number. The pseudoinverse is reused
/// across pixels; its columns also supply the variable-projection operator.
struct ShiftModel {
  std::vector<double> cosShift, sinShift;
  std::vector<std::array<double, 3>> pseudoColumns;
  double condition = std::numeric_limits<double>::infinity();

  bool build(const std::vector<double> &shifts) {
    if (shifts.size() < 3 || !allFinite(shifts))
      return false;
    const size_t frameCount = shifts.size();
    cosShift.resize(frameCount);
    sinShift.resize(frameCount);
    cv::Mat design(static_cast<int>(frameCount), 3, CV_64F);
    for (size_t n = 0; n < frameCount; ++n) {
      cosShift[n] = std::cos(shifts[n]);
      sinShift[n] = std::sin(shifts[n]);
      design.at<double>(static_cast<int>(n), 0) = 1.0;
      design.at<double>(static_cast<int>(n), 1) = cosShift[n];
      design.at<double>(static_cast<int>(n), 2) = -sinShift[n];
    }
    cv::SVD svd(design);
    const double largest = svd.w.at<double>(0), smallest = svd.w.at<double>(2);
    condition = smallest > 0.0 ? largest / smallest : std::numeric_limits<double>::infinity();
    if (!std::isfinite(condition) || condition > kMaxFitCondition)
      return false;
    pseudoColumns.resize(frameCount);
    for (size_t n = 0; n < frameCount; ++n)
      for (int parameter = 0; parameter < 3; ++parameter) {
        double weight = 0.0;
        for (int k = 0; k < 3; ++k)
          weight += svd.vt.at<double>(k, parameter) * svd.u.at<double>(static_cast<int>(n), k) /
                    svd.w.at<double>(k);
        pseudoColumns[n][parameter] = weight;
      }
    return true;
  }

  /// Подгонка a + c*cos(delta) - s*sin(delta) к кадрам в точке (x, y).
  void fit(const std::vector<Bitmap> &frames, int x, int y, double &a, double &c,
           double &s) const {
    double rhs[3] = {};
    for (size_t n = 0; n < frames.size(); ++n) {
      const double intensity = frames[n].pixel(x, y);
      for (int p = 0; p < 3; ++p)
        rhs[p] += pseudoColumns[n][p] * intensity;
    }
    a = rhs[0];
    c = rhs[1];
    s = rhs[2];
  }
};

/// Наклоны кадров (PsiResult::tiltX/tiltY), пустые -- без наклона.
struct Tilts {
  const std::vector<double> &x;
  const std::vector<double> &y;
  bool empty() const { return x.empty(); }
};

/// Координата от центра кадра -- ось, вокруг которой поворачивается
/// плоскость сдвига (PsiResult::tiltX).
double centered(int coordinate, int size) { return coordinate - (size - 1) / 2.0; }

/// Шаг 2 по точкам маски, где фаза определена: сырые atan2 для каждого
/// кадра, без приведения к shifts[0] == 0. usedPoints -- сколько точек
/// пошло в подгонку. С наклонами ищется сдвиг в центре кадра при
/// известных наклонах.
bool fitFrameShifts(const std::vector<Bitmap> &frames, const PhaseMap &phase,
                    const PointMask &points, const Tilts &tilts, std::vector<double> &raw,
                    int &usedPoints, std::string &error) {
  raw.clear();
  usedPoints = 0;
  for (size_t n = 0; n < frames.size(); ++n) {
    // Та же подгонка, что в computePhase(), только уравнения -- по
    // пикселям, а theta -- фаза в пикселе: I = a + (b*cos d)*cos(phi) - (b*sin d)*sin(phi).
    CosineFit fit;
    for (int y = 0; y < phase.height(); ++y) {
      const size_t row = static_cast<size_t>(y) * static_cast<size_t>(phase.width());
      for (int x = 0; x < phase.width(); ++x) {
        if (!points[row + x] || !phase.hasValue(x, y))
          continue;
        double theta = phase.value(x, y);
        if (!tilts.empty())
          theta += tilts.x[n] * centered(x, phase.width()) +
                   tilts.y[n] * centered(y, phase.height());
        fit.add(theta, frames[n].pixel(x, y));
      }
    }
    double a = 0.0, c = 0.0, s = 0.0;
    if (!fit.solve(a, c, s)) {
      error = "Cannot estimate the phase shift of frame " + std::to_string(n + 1) + ": " +
              std::to_string(fit.count()) +
              " usable point(s) with fringes -- too few, or all at the same phase";
      return false;
    }
    raw.push_back(std::atan2(s, c));  // (15)
    usedPoints = fit.count();
  }
  return true;
}

// Diagnostic shift fit with per-pixel photometry fixed at the current solution.
// Keep the same illumination model in the full-field solve and half-field check.
bool fitPhotometricShifts(const std::vector<Bitmap> &frames, const PsiResult &current,
                          const PointMask &points, std::vector<double> &raw) {
  ShiftModel model;
  if (!model.build(current.shifts))
    return false;
  double cc = 0.0, cs = 0.0, ss = 0.0;
  std::vector<double> rhsC(frames.size(), 0.0), rhsS(frames.size(), 0.0);
  for (int y = 0; y < frames[0].height(); ++y) {
    for (int x = 0; x < frames[0].width(); ++x) {
      if (!points[static_cast<size_t>(y) * frames[0].width() + x])
        continue;
      double a = 0.0, c = 0.0, s = 0.0;
      model.fit(frames, x, y, a, c, s);
      cc += c * c;
      cs += c * s;
      ss += s * s;
      for (size_t n = 0; n < frames.size(); ++n) {
        const double value = frames[n].pixel(x, y) - a;
        rhsC[n] += c * value;
        rhsS[n] -= s * value;
      }
    }
  }
  const double determinant = cc * ss - cs * cs;
  if (!std::isfinite(determinant) || determinant <= 1e-12 * (cc + ss) * (cc + ss))
    return false;
  raw.resize(frames.size());
  for (size_t n = 0; n < frames.size(); ++n) {
    const double cosine = (ss * rhsC[n] + cs * rhsS[n]) / determinant;
    const double sine = (cs * rhsC[n] + cc * rhsS[n]) / determinant;
    raw[n] = std::atan2(sine, cosine);
  }
  return true;
}

/// Медиана; vector портится (nth_element).
double median(std::vector<double> &values) {
  if (values.empty())
    return 0.0;
  const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), middle, values.end());
  return *middle;
}

/// Расхождение сдвигов, найденных по двум половинам точек (разрез по
/// медиане координаты), при одной и той же фазе -- СКО по кадрам, градусы.
/// NaN, если на половину не хватило точек.
double halvesDisagreementDeg(const std::vector<Bitmap> &frames, const PsiResult &result,
                             const PointMask &usable, const Tilts &tilts, bool alongX) {
  const PhaseMap &phase = result.wrappedPhase;
  std::vector<double> coordinates;
  for (int y = 0; y < phase.height(); ++y)
    for (int x = 0; x < phase.width(); ++x)
      if (usable[static_cast<size_t>(y) * phase.width() + x])
        coordinates.push_back(alongX ? x : y);
  const double cut = median(coordinates);

  PointMask low(usable.size(), 0), high(usable.size(), 0);
  for (int y = 0; y < phase.height(); ++y) {
    for (int x = 0; x < phase.width(); ++x) {
      const size_t i = static_cast<size_t>(y) * phase.width() + x;
      if (usable[i])
        ((alongX ? x : y) < cut ? low : high)[i] = 1;
    }
  }
  std::vector<double> lowShifts, highShifts;
  int used = 0;
  std::string ignored;
  const auto fitHalf = [&](const PointMask &points, std::vector<double> &shifts) {
    return result.illuminationRefined && tilts.empty()
               ? fitPhotometricShifts(frames, result, points, shifts)
               : fitFrameShifts(frames, phase, points, tilts, shifts, used, ignored);
  };
  if (!fitHalf(low, lowShifts) || !fitHalf(high, highShifts))
    return std::numeric_limits<double>::quiet_NaN();

  const double twoPi = 2.0 * std::acos(-1.0);
  double sumSq = 0.0;
  for (size_t n = 1; n < frames.size(); ++n) {
    const double d = std::remainder((lowShifts[n] - lowShifts[0]) - (highShifts[n] - highShifts[0]),
                                    twoPi);
    sumSq += d * d;
  }
  return std::sqrt(sumSq / static_cast<double>(frames.size() - 1)) * 180.0 / std::acos(-1.0);
}

PsiResult::Diagnostics diagnose(const std::vector<Bitmap> &frames, const PsiResult &result,
                                const PointMask &points) {
  PsiResult::Diagnostics diagnostics;
  const PhaseMap &phase = result.wrappedPhase;
  PointMask usable(points.size(), 0);
  long long histogram[256] = {};
  long long sampleCount = 0;
  for (int y = 0; y < phase.height(); ++y) {
    for (int x = 0; x < phase.width(); ++x) {
      const size_t i = static_cast<size_t>(y) * phase.width() + x;
      if (points[i] && phase.hasValue(x, y)) {
        usable[i] = 1;
        ++diagnostics.shiftPoints;
        for (const auto &frame : frames) {
          ++histogram[frame.pixel(x, y)];
          ++sampleCount;
        }
      }
    }
  }

  // A video/camera pipeline may clip below 255 (the real big*.bmp series has
  // a narrow spike at 235). Do not hardcode that level or discard these data:
  // flag a prominent upper-end atom as a hypothesis for the operator to check.
  int upperMode = 192;
  for (int level = 193; level < 256; ++level)
    if (histogram[level] > histogram[upperMode])
      upperMode = level;
  long long neighbourMaximum = 0, above = 0;
  for (int level = std::max(0, upperMode - 3); level <= std::min(255, upperMode + 3); ++level)
    if (level != upperMode)
      neighbourMaximum = std::max(neighbourMaximum, histogram[level]);
  for (int level = upperMode + 1; level < 256; ++level)
    above += histogram[level];
  if (sampleCount > 0 && histogram[upperMode] >= 0.05 * sampleCount &&
      histogram[upperMode] > 3 * neighbourMaximum && above <= 0.01 * sampleCount) {
    diagnostics.upperPlateauLevel = upperMode;
    diagnostics.upperPlateauFraction = static_cast<double>(histogram[upperMode]) / sampleCount;
  }

  // С наклонами половины сравниваются по сдвигу в центре при уже
  // найденных наклонах: расхождение тогда -- то, что плоскость не
  // описала (или нехватка полос).
  const Tilts tilts{result.tiltX, result.tiltY};
  const double alongX = halvesDisagreementDeg(frames, result, usable, tilts, true);
  const double alongY = halvesDisagreementDeg(frames, result, usable, tilts, false);
  diagnostics.halvesDisagreementDeg = std::isnan(alongX) || std::isnan(alongY)
                                          ? std::numeric_limits<double>::quiet_NaN()
                                          : std::max(alongX, alongY);

  ShiftModel model;
  if (tilts.empty() && !model.build(result.shifts))
    return diagnostics;
  const size_t frameCount = frames.size();
  std::vector<double> cosTheta(frameCount), sinTheta(frameCount);
  double sumSq = 0.0;
  long long terms = 0;
  std::vector<double> amplitudes;
  for (int y = 0; y < phase.height(); ++y) {
    for (int x = 0; x < phase.width(); ++x) {
      if (!usable[static_cast<size_t>(y) * phase.width() + x])
        continue;
      double a = 0.0, c = 0.0, s = 0.0;
      if (tilts.empty()) {
        model.fit(frames, x, y, a, c, s);
        cosTheta = model.cosShift;
        sinTheta = model.sinShift;
      } else {
        CosineFit fit;
        for (size_t n = 0; n < frameCount; ++n) {
          const double theta = result.shifts[n] + tilts.x[n] * centered(x, phase.width()) +
                               tilts.y[n] * centered(y, phase.height());
          cosTheta[n] = std::cos(theta);
          sinTheta[n] = std::sin(theta);
          fit.addCosSin(cosTheta[n], sinTheta[n], frames[n].pixel(x, y));
        }
        if (!fit.solve(a, c, s))
          continue;
      }
      amplitudes.push_back(std::sqrt(c * c + s * s));
      for (size_t n = 0; n < frameCount; ++n) {
        const double r = frames[n].pixel(x, y) - (a + c * cosTheta[n] - s * sinTheta[n]);
        sumSq += r * r;
        ++terms;
      }
    }
  }
  if (terms > 0)
    diagnostics.residual = std::sqrt(sumSq / static_cast<double>(terms));
  diagnostics.amplitude = median(amplitudes);
  return diagnostics;
}

}  // namespace

void CosineFit::add(double theta, double intensity) {
  addCosSin(std::cos(theta), std::sin(theta), intensity);
}

void CosineFit::addCosSin(double cosTheta, double sinTheta, double intensity) {
  ++m_count;
  if (!std::isfinite(cosTheta) || !std::isfinite(sinTheta) || !std::isfinite(intensity)) {
    m_valid = false;
    return;
  }
  // Append [1, cos(theta), -sin(theta)] using Givens rotations. Only R and
  // Q^T*I are retained: constant memory even for spatial fits with many pixels.
  double row[3] = {1.0, cosTheta, -sinTheta};
  for (int j = 0; j < 3; ++j) {
    const double diagonal = std::hypot(m_r[j][j], row[j]);
    if (diagonal == 0.0)
      continue;
    const double cosine = m_r[j][j] / diagonal, sine = row[j] / diagonal;
    m_r[j][j] = diagonal;
    for (int k = j + 1; k < 3; ++k) {
      const double old = m_r[j][k];
      m_r[j][k] = cosine * old + sine * row[k];
      row[k] = -sine * old + cosine * row[k];
    }
    const double old = m_rhs[j];
    m_rhs[j] = cosine * old + sine * intensity;
    intensity = -sine * old + cosine * intensity;
  }
}

bool CosineFit::solve(double &a, double &c, double &s) const {
  if (m_count < 3 || !m_valid)
    return false;
  double norm = 0.0;
  for (int i = 0; i < 3; ++i) {
    if (m_r[i][i] == 0.0)
      return false;
    double sum = 0.0;
    for (int j = i; j < 3; ++j)
      sum += std::fabs(m_r[i][j]);
    norm = std::max(norm, sum);
  }
  // Exact infinity-norm condition of the small triangular factor. These
  // solves estimate sensitivity only; coefficients use back substitution.
  double inverse[3][3] = {};
  for (int col = 0; col < 3; ++col)
    for (int i = 2; i >= 0; --i) {
      double rhs = i == col ? 1.0 : 0.0;
      for (int j = i + 1; j < 3; ++j)
        rhs -= m_r[i][j] * inverse[j][col];
      inverse[i][col] = rhs / m_r[i][i];
    }
  double inverseNorm = 0.0;
  for (const auto &row : inverse)
    inverseNorm = std::max(inverseNorm, std::fabs(row[0]) + std::fabs(row[1]) + std::fabs(row[2]));
  if (!std::isfinite(norm * inverseNorm) || norm * inverseNorm > kMaxFitCondition)
    return false;
  double solution[3];
  for (int i = 2; i >= 0; --i) {
    double rhs = m_rhs[i];
    for (int j = i + 1; j < 3; ++j)
      rhs -= m_r[i][j] * solution[j];
    solution[i] = rhs / m_r[i][i];
    if (!std::isfinite(solution[i]))
      return false;
  }
  a = solution[0];
  c = solution[1];
  s = solution[2];
  return true;
}

bool computePhase(const std::vector<Bitmap> &frames, const std::vector<double> &shifts,
                  const std::function<bool(int, int)> &isVisible, PsiResult &out,
                  std::string &error) {
  if (!checkFrames(frames, error))
    return false;
  if (shifts.size() != frames.size()) {
    error = "Number of phase shifts (" + std::to_string(shifts.size()) +
            ") does not match the number of frames (" + std::to_string(frames.size()) + ")";
    return false;
  }
  const int width = frames[0].width();
  const int height = frames[0].height();

  // The temporal design is shared by all pixels; factor it only once.
  ShiftModel model;
  if (!model.build(shifts)) {
    error =
        "Phase shifts are non-finite, degenerate or ill-conditioned "
        "(condition limit 10000). Use better-separated shifts.";
    return false;
  }

  PhaseMap phase(width, height);
  PhaseMap modulation(width, height);
  PhaseMap phaseStdError(width, height);
  int validPoints = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!isVisible(x, y))
        continue;
      double a = 0.0, c = 0.0, s = 0.0;
      model.fit(frames, x, y, a, c, s);
      const double amplitude = std::hypot(c, s);
      if (!std::isfinite(amplitude) || amplitude < kMinPhaseAmplitude)
        continue;
      ++validPoints;
      phase.setValue(x, y, std::atan2(s, c));                                  // (7)
      modulation.setValue(x, y, a > 0.0 ? amplitude / a : 0.0);                // (8)
      if (frames.size() > 3) {
        double sse = 0.0, sensitivity = 0.0;
        for (size_t n = 0; n < frames.size(); ++n) {
          const double r =
              frames[n].pixel(x, y) - (a + c * model.cosShift[n] - s * model.sinShift[n]);
          sse += r * r;
          const double derivative =
              (-s * model.pseudoColumns[n][1] + c * model.pseudoColumns[n][2]) /
              (amplitude * amplitude);
          sensitivity += derivative * derivative;
        }
        const double variance = std::max(1.0 / 12.0, sse / (frames.size() - 3));
        phaseStdError.setValue(x, y, std::sqrt(variance * sensitivity) / (2.0 * std::acos(-1.0)));
      }
    }
  }

  if (validPoints == 0) {
    error = "No usable phase points: empty aperture or fringe amplitude below 1 intensity level";
    return false;
  }
  out.wrappedPhase = std::move(phase);
  out.modulation = std::move(modulation);
  out.phaseStdError = std::move(phaseStdError);
  out.shifts = shifts;
  out.tiltX.clear();
  out.tiltY.clear();
  return true;
}

bool computePhaseTilted(const std::vector<Bitmap> &frames, const std::vector<double> &shifts,
                        const std::vector<double> &tiltX, const std::vector<double> &tiltY,
                        const std::function<bool(int, int)> &isVisible, PsiResult &out,
                        std::string &error) {
  if (tiltX.empty() && tiltY.empty())
    return computePhase(frames, shifts, isVisible, out, error);
  if (!checkFrames(frames, error))
    return false;
  const size_t frameCount = frames.size();
  if (shifts.size() != frameCount || tiltX.size() != frameCount || tiltY.size() != frameCount) {
    error = "Number of phase shifts or tilts does not match the number of frames";
    return false;
  }
  if (!allFinite(shifts) || !allFinite(tiltX) || !allFinite(tiltY)) {
    error = "Phase shifts and tilts must be finite";
    return false;
  }
  const int width = frames[0].width();
  const int height = frames[0].height();

  // Сдвиг в точке = (shifts[n] + tiltY[n]*v) + tiltX[n]*u: cos и sin
  // обеих частей считаются по строкам и столбцам, в точке -- только
  // формула сложения, без тригонометрии.
  std::vector<double> cosRow(frameCount * height), sinRow(frameCount * height);
  std::vector<double> cosColumn(frameCount * width), sinColumn(frameCount * width);
  for (size_t n = 0; n < frameCount; ++n) {
    for (int y = 0; y < height; ++y) {
      const double angle = shifts[n] + tiltY[n] * centered(y, height);
      cosRow[n * height + y] = std::cos(angle);
      sinRow[n * height + y] = std::sin(angle);
    }
    for (int x = 0; x < width; ++x) {
      const double angle = tiltX[n] * centered(x, width);
      cosColumn[n * width + x] = std::cos(angle);
      sinColumn[n * width + x] = std::sin(angle);
    }
  }

  PhaseMap phase(width, height);
  PhaseMap modulation(width, height);
  int validPoints = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!isVisible(x, y))
        continue;
      CosineFit fit;
      for (size_t n = 0; n < frameCount; ++n) {
        const double cr = cosRow[n * height + y], sr = sinRow[n * height + y];
        const double cc = cosColumn[n * width + x], sc = sinColumn[n * width + x];
        fit.addCosSin(cr * cc - sr * sc, sr * cc + cr * sc, frames[n].pixel(x, y));
      }
      double a = 0.0, c = 0.0, s = 0.0;
      if (!fit.solve(a, c, s))
        continue;  // в этой точке сдвиги кадров совпали -- фазы нет
      const double amplitude = std::hypot(c, s);
      if (amplitude < kMinPhaseAmplitude)
        continue;
      ++validPoints;
      phase.setValue(x, y, std::atan2(s, c));
      modulation.setValue(x, y, a > 0.0 ? amplitude / a : 0.0);
    }
  }

  if (validPoints == 0) {
    error = "No usable phase points: check aperture, fringe amplitude and phase-shift conditioning";
    return false;
  }
  out.wrappedPhase = std::move(phase);
  out.modulation = std::move(modulation);
  out.shifts = shifts;
  out.tiltX = tiltX;
  out.tiltY = tiltY;
  out.phaseStdError.clear();
  return true;
}

PointMask selectShiftEstimationPoints(const std::vector<Bitmap> &frames,
                                      const std::function<bool(int, int)> &isVisible,
                                      double minFringeSpan) {
  std::string ignored;
  if (!checkFrames(frames, ignored))
    return {};
  const int width = frames[0].width();
  const int height = frames[0].height();

  // Размах -- целое 0..255 (кадры 8-битные), так что процентиль -- по
  // гистограмме, без сортировки.
  std::vector<unsigned char> span(static_cast<size_t>(width) * height, 0);
  PointMask visible(span.size(), 0);
  long long histogram[256] = {};
  long long visibleCount = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!isVisible(x, y))
        continue;
      int low = 255, high = 0;
      for (const Bitmap &frame : frames) {
        const int value = frame.pixel(x, y);
        low = std::min(low, value);
        high = std::max(high, value);
      }
      const size_t i = static_cast<size_t>(y) * width + x;
      span[i] = static_cast<unsigned char>(high - low);
      visible[i] = 1;
      ++histogram[span[i]];
      ++visibleCount;
    }
  }

  // 99-й, а не максимум: одиночные горячие или битые пиксели не должны
  // задавать порог.
  int percentile99 = 0;
  long long below = 0;
  const double target = 0.99 * static_cast<double>(visibleCount);
  for (int value = 0; value < 256; ++value) {
    below += histogram[value];
    if (static_cast<double>(below) >= target) {
      percentile99 = value;
      break;
    }
  }

  const double threshold = minFringeSpan * percentile99;
  PointMask points(span.size(), 0);
  for (size_t i = 0; i < span.size(); ++i)
    points[i] = visible[i] && span[i] > 0 && span[i] >= threshold;
  return points;
}

bool estimateShifts(const std::vector<Bitmap> &frames, const PsiResult &current,
                    const PointMask &points, std::vector<double> &shifts, std::string &error) {
  if (!checkFrames(frames, error))
    return false;
  const PhaseMap &phase = current.wrappedPhase;
  if (phase.width() != frames[0].width() || phase.height() != frames[0].height() ||
      points.size() != static_cast<size_t>(phase.width()) * phase.height()) {
    error = "Phase map or point mask size does not match the frames";
    return false;
  }

  std::vector<double> found;
  int usedPoints = 0;
  const std::vector<double> noTilt;
  if (!fitFrameShifts(frames, phase, points, Tilts{noTilt, noTilt}, found, usedPoints, error))
    return false;

  // Сдвиги определены с точностью до общей константы (её же можно
  // перенести в фазу) -- фиксируем её так, чтобы первый кадр был нулём.
  const double twoPi = 2.0 * std::acos(-1.0);
  shifts.resize(found.size());
  for (size_t n = 0; n < found.size(); ++n) {
    double shift = std::fmod(found[n] - found[0], twoPi);
    if (shift < 0.0)
      shift += twoPi;
    shifts[n] = shift;
  }
  return true;
}

namespace {

/// Не больше maxPoints точек маски -- равномерной сеткой с шагом s по x и
/// по y (см. PsiOptions::maxShiftPoints). Сетка, а не случайная выборка:
/// у solveWithTiltShifts() в каждом оставшемся столбце и строке остаются
/// точки по всей их длине.
PointMask thinPoints(const PointMask &points, int width, int maxPoints) {
  long long count = 0;
  for (const unsigned char point : points)
    count += point;
  if (maxPoints <= 0 || count <= maxPoints)
    return points;
  const int stride = static_cast<int>(
      std::ceil(std::sqrt(static_cast<double>(count) / static_cast<double>(maxPoints))));
  PointMask thinned(points.size(), 0);
  for (size_t i = 0; i < points.size(); ++i) {
    const int x = static_cast<int>(i % static_cast<size_t>(width));
    const int y = static_cast<int>(i / static_cast<size_t>(width));
    thinned[i] = points[i] && x % stride == 0 && y % stride == 0;
  }
  return thinned;
}

/// Сдвиги, приведённые к виду, который возвращает estimateShifts(): первый
/// -- ноль, остальные в [0, 2*pi).
std::vector<double> normalizedShifts(std::vector<double> shifts) {
  if (shifts.empty())
    return shifts;
  const double twoPi = 2.0 * std::acos(-1.0);
  const double first = shifts[0];
  for (double &shift : shifts) {
    shift = std::fmod(shift - first, twoPi);
    if (shift < 0.0)
      shift += twoPi;
  }
  return shifts;
}

// Variable projection for I_np = a_p + c_p*cos(delta_n) - s_p*sin(delta_n).
// Eliminate the three pixel coefficients at every trial. Unlike the article's
// spatial cosine fit, this makes no uniform-illumination assumption. Fix delta_0
// to zero, leaving N-1 nonlinear unknowns. The projected Gauss-Newton matrix is
// H_jk = (I-AA^+)_jk * sum_p(d_pj*d_pk), d_pj = -c_p*sin(delta_j)-s_p*cos(delta_j).
// The residual is orthogonal to A, so g_j = sum_p(d_pj*r_pj).
struct ProjectedShiftSystem {
  double cost = 0.0;
  cv::Mat hessian, gradient;
};

bool projectedShiftSystem(const std::vector<Bitmap> &frames, const PointMask &points,
                          const std::vector<double> &shifts, bool linearize,
                          ProjectedShiftSystem &out) {
  ShiftModel model;
  if (!model.build(shifts))
    return false;
  const int n = static_cast<int>(frames.size());
  ProjectedShiftSystem system;
  if (linearize) {
    system.hessian = cv::Mat::zeros(n - 1, n - 1, CV_64F);
    system.gradient = cv::Mat::zeros(n - 1, 1, CV_64F);
  }
  double cc = 0.0, cs = 0.0, ss = 0.0;
  int used = 0;
  for (int y = 0; y < frames[0].height(); ++y) {
    for (int x = 0; x < frames[0].width(); ++x) {
      if (!points[static_cast<size_t>(y) * frames[0].width() + x])
        continue;
      double a = 0.0, c = 0.0, s = 0.0;
      model.fit(frames, x, y, a, c, s);
      ++used;
      cc += c * c;
      cs += c * s;
      ss += s * s;
      for (int j = 0; j < n; ++j) {
        const double r =
            frames[j].pixel(x, y) - (a + c * model.cosShift[j] - s * model.sinShift[j]);
        system.cost += r * r;
        if (linearize && j > 0)
          system.gradient.at<double>(j - 1) += (-c * model.sinShift[j] - s * model.cosShift[j]) * r;
      }
    }
  }
  if (used < 3 || !std::isfinite(system.cost))
    return false;
  if (linearize) {
    for (int j = 1; j < n; ++j) {
      const double aj[3] = {1.0, model.cosShift[j], -model.sinShift[j]};
      for (int k = 1; k < n; ++k) {
        double projection = j == k ? 1.0 : 0.0;
        for (int u = 0; u < 3; ++u)
          projection -= aj[u] * model.pseudoColumns[k][u];
        const double covariance =
            cc * model.sinShift[j] * model.sinShift[k] +
            cs * (model.sinShift[j] * model.cosShift[k] + model.cosShift[j] * model.sinShift[k]) +
            ss * model.cosShift[j] * model.cosShift[k];
        system.hessian.at<double>(j - 1, k - 1) = projection * covariance;
      }
    }
    // Roundoff in the inverse normal matrix must not break the SPD solve.
    system.hessian = (system.hessian + system.hessian.t()) * 0.5;
  }
  out = std::move(system);
  return true;
}

bool hasShiftInformation(const cv::Mat &hessian) {
  cv::Mat singular;
  cv::SVD::compute(hessian, singular);
  const double largest = singular.at<double>(0);
  const double smallest = singular.at<double>(static_cast<int>(singular.total()) - 1);
  // Engineering identifiability guard, not a calibrated uncertainty estimate.
  return std::isfinite(largest) && largest > 0.0 && std::isfinite(smallest) &&
         smallest > 1e-8 * largest;
}

bool refineSpatialIllumination(const std::vector<Bitmap> &frames, const PointMask &points,
                               const PsiOptions &options, std::vector<double> &shifts,
                               int &iterations, bool &converged) {
  // Three/four arbitrary frames do not in general determine this larger model.
  // Start with the article solution and retain it if the projected system lacks
  // information (also possible with repeated shifts or insufficient phase span).
  if (frames.size() < 5 || iterations >= options.maxIterations)
    return false;
  ProjectedShiftSystem current;
  if (!projectedShiftSystem(frames, points, shifts, true, current) ||
      !hasShiftInformation(current.hessian))
    return false;

  converged = false;
  double damping = 1e-3;
  while (iterations < options.maxIterations) {
    ++iterations;
    cv::Mat unconstrainedStep;
    if (!cv::solve(current.hessian, current.gradient, unconstrainedStep, cv::DECOMP_CHOLESKY))
      return true;
    if (cv::norm(unconstrainedStep, cv::NORM_INF) < options.tolerance) {
      converged = true;
      return true;
    }
    bool accepted = false;
    for (int attempt = 0; attempt < 10; ++attempt) {
      cv::Mat h = current.hessian.clone();
      for (int j = 0; j < h.rows; ++j)
        h.at<double>(j, j) *= 1.0 + damping;
      cv::Mat step;
      if (!cv::solve(h, current.gradient, step, cv::DECOMP_CHOLESKY))
        break;
      const double maximum = cv::norm(step, cv::NORM_INF);
      if (!std::isfinite(maximum))
        break;
      // A local model must not jump to another phase branch in a single trial.
      if (maximum > 0.25)
        step *= 0.25 / maximum;
      auto trial = shifts;
      for (size_t n = 1; n < trial.size(); ++n)
        trial[n] += step.at<double>(static_cast<int>(n) - 1);
      ProjectedShiftSystem candidate;
      if (projectedShiftSystem(frames, points, trial, false, candidate) &&
          candidate.cost < current.cost) {
        shifts = normalizedShifts(std::move(trial));
        accepted = true;
        damping = std::max(1e-12, damping * 0.1);
        break;
      }
      damping *= 10.0;
    }
    if (!accepted)
      return true;  // Keep the best accepted parameters, but not "converged".
    if (!projectedShiftSystem(frames, points, shifts, true, current) ||
        !hasShiftInformation(current.hessian))
      return true;
  }
  return true;
}

/// Уравнения не различают решение (phi, delta) и зеркальное (-phi,
/// -delta): cos(phi + delta) == cos(-phi - delta) (с наклонами --
/// зеркальны и они). Какое из двух найдут итерации, зависит от пути, а от
/// выбора зависит знак всей карты (бугор или яма). Берём то, чьи сдвиги
/// ближе к ожидаемым -- единственное, что знает о реальном направлении
/// фазосдвигателя.
bool preferExpectedDirection(const std::vector<Bitmap> &frames,
                             const std::vector<double> &expected,
                             const std::function<bool(int, int)> &isVisible, PsiResult &result,
                             std::string &error) {
  const double twoPi = 2.0 * std::acos(-1.0);
  std::vector<double> mirrored(result.shifts.size());
  double directDistance = 0.0, mirroredDistance = 0.0;
  for (size_t n = 0; n < result.shifts.size(); ++n) {
    mirrored[n] = n == 0 ? 0.0 : twoPi - result.shifts[n];
    directDistance += std::fabs(std::remainder(result.shifts[n] - expected[n], twoPi));
    mirroredDistance += std::fabs(std::remainder(mirrored[n] - expected[n], twoPi));
  }
  if (mirroredDistance >= directDistance)
    return true;

  std::vector<double> tiltX = result.tiltX, tiltY = result.tiltY;
  for (double &tilt : tiltX)
    tilt = -tilt;
  for (double &tilt : tiltY)
    tilt = -tilt;
  const int iterations = result.iterations;
  const bool converged = result.converged;
  if (!computePhaseTilted(frames, mirrored, tiltX, tiltY, isVisible, result, error))
    return false;
  result.iterations = iterations;
  result.converged = converged;
  result.mirrored = true;
  return true;
}

}  // namespace

bool solveWithUnknownShifts(const std::vector<Bitmap> &frames,
                            const std::vector<double> &expectedShifts,
                            const std::function<bool(int, int)> &isVisible,
                            const PsiOptions &options, PsiResult &out, std::string &error) {
  // Начальное приближение -- ожидаемые сдвиги, приведённые к тому же виду,
  // что возвращает estimateShifts(), чтобы первая разность в (16) не
  // учитывала просто другую точку отсчёта.
  const std::vector<double> expected = normalizedShifts(expectedShifts);
  std::vector<double> shifts = expected;

  if (!checkFrames(frames, error))
    return false;
  const int width = frames[0].width();
  // Один раз на весь расчёт -- см. selectShiftEstimationPoints().
  const PointMask points = selectShiftEstimationPoints(frames, isVisible, options.minFringeSpan);
  // Итерациям фаза нужна только там, где по ней оцениваются сдвиги.
  const PointMask shiftPoints = thinPoints(points, width, options.maxShiftPoints);
  const auto isShiftPoint = [&shiftPoints, width](int x, int y) {
    return shiftPoints[static_cast<size_t>(y) * width + x] != 0;
  };

  PsiResult result;
  if (!computePhase(frames, shifts, isShiftPoint, result, error))
    return false;

  for (int iteration = 1; iteration <= options.maxIterations; ++iteration) {
    std::vector<double> next;
    if (!estimateShifts(frames, result, shiftPoints, next, error))
      return false;

    // (16): насколько сдвинулись оценки за этот шаг, СКО по кадрам.
    double sumSq = 0.0;
    for (size_t n = 0; n < next.size(); ++n) {
      const double change = std::remainder(next[n] - shifts[n], 2.0 * std::acos(-1.0));
      sumSq += change * change;
    }
    const double change = std::sqrt(sumSq / next.size());

    shifts = std::move(next);
    if (!computePhase(frames, shifts, isShiftPoint, result, error))
      return false;
    result.iterations = iteration;
    result.converged = change < options.tolerance;
    // The uniform-illumination initializer need not converge on nonuniform
    // data. Switch once it has supplied a usable local starting point; leave
    // the remaining iteration budget for the full photometric model.
    if ((result.converged || iteration >= 5) &&
        refineSpatialIllumination(frames, shiftPoints, options, shifts, result.iterations,
                                  result.converged)) {
      result.illuminationRefined = true;
      break;
    }
    if (result.converged)
      break;
  }

  // Итоговая фаза -- во всей апертуре (iterations и converged не трогает).
  if (!computePhase(frames, shifts, isVisible, result, error))
    return false;
  if (!preferExpectedDirection(frames, expected, isVisible, result, error))
    return false;
  result.diagnostics = diagnose(frames, result, points);
  out = std::move(result);
  return true;
}

bool solveWithKnownShifts(const std::vector<Bitmap> &frames, const std::vector<double> &shifts,
                          const std::function<bool(int, int)> &isVisible,
                          const PsiOptions &options, PsiResult &out, std::string &error) {
  PsiResult result;
  if (!computePhase(frames, normalizedShifts(shifts), isVisible, result, error))
    return false;
  result.converged = true;
  const PointMask points = selectShiftEstimationPoints(frames, isVisible, options.minFringeSpan);
  result.diagnostics = diagnose(frames, result, points);
  out = std::move(result);
  return true;
}

namespace {

/// Состояние итераций метода Liu: плоскость сдвига каждого кадра,
/// разложенная на x- и y-части. Сдвиг кадра n в точке (u, v) от центра --
/// shiftX[n] + tiltX[n]*u + shiftY[n] + tiltY[n]*v. Кадр 0 -- отсчёт, его
/// плоскость нулевая.
struct TiltPlanes {
  std::vector<double> shiftX, shiftY, tiltX, tiltY;

  explicit TiltPlanes(size_t frameCount)
      : shiftX(frameCount, 0.0), shiftY(frameCount, 0.0), tiltX(frameCount, 0.0),
        tiltY(frameCount, 0.0) {}

  /// Сдвиги в центре кадра, приведённые к [0, 2*pi).
  std::vector<double> centerShifts() const {
    const double twoPi = 2.0 * std::acos(-1.0);
    std::vector<double> total(shiftX.size());
    for (size_t n = 0; n < total.size(); ++n) {
      total[n] = std::fmod(shiftX[n] + shiftY[n], twoPi);
      if (total[n] < 0.0)
        total[n] += twoPi;
    }
    return total;
  }
};

// Full photometric model for tilted shifts. Pixel backgrounds/amplitudes are
// eliminated with SVD; only frame planes are optimized. Frame 0 fixes the gauge.
// Coordinates span one frame so all three plane parameters have radian units.
bool projectedTiltSystem(const std::vector<Bitmap> &frames, const PointMask &points,
                         const cv::Mat &parameters, bool linearize, ProjectedShiftSystem &out) {
  const int n = static_cast<int>(frames.size()), dimension = 3 * (n - 1);
  const int width = frames[0].width(), height = frames[0].height();
  ProjectedShiftSystem system;
  if (linearize) {
    system.hessian = cv::Mat::zeros(dimension, dimension, CV_64F);
    system.gradient = cv::Mat::zeros(dimension, 1, CV_64F);
  }
  std::vector<double> shifts(n, 0.0), derivatives(n), residuals(n);
  ShiftModel model;
  int used = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!points[static_cast<size_t>(y) * width + x])
        continue;
      const double basis[3] = {1.0, centered(x, width) / std::max(1, width - 1),
                               centered(y, height) / std::max(1, height - 1)};
      for (int j = 1; j < n; ++j) {
        shifts[j] = 0.0;
        for (int p = 0; p < 3; ++p)
          shifts[j] += parameters.at<double>(3 * (j - 1) + p) * basis[p];
      }
      // Never improve the objective by silently dropping difficult pixels.
      if (!model.build(shifts))
        return false;
      double a = 0.0, c = 0.0, s = 0.0;
      model.fit(frames, x, y, a, c, s);
      ++used;
      for (int j = 0; j < n; ++j) {
        residuals[j] = frames[j].pixel(x, y) - (a + c * model.cosShift[j] - s * model.sinShift[j]);
        derivatives[j] = -c * model.sinShift[j] - s * model.cosShift[j];
        system.cost += residuals[j] * residuals[j];
      }
      if (!linearize)
        continue;
      for (int j = 1; j < n; ++j) {
        for (int p = 0; p < 3; ++p)
          system.gradient.at<double>(3 * (j - 1) + p) += derivatives[j] * basis[p] * residuals[j];
        const double row[3] = {1.0, model.cosShift[j], -model.sinShift[j]};
        for (int k = 1; k < n; ++k) {
          double projection = j == k ? 1.0 : 0.0;
          for (int p = 0; p < 3; ++p)
            projection -= row[p] * model.pseudoColumns[k][p];
          const double weight = projection * derivatives[j] * derivatives[k];
          for (int p = 0; p < 3; ++p)
            for (int q = 0; q < 3; ++q)
              system.hessian.at<double>(3 * (j - 1) + p, 3 * (k - 1) + q) +=
                  weight * basis[p] * basis[q];
        }
      }
    }
  }
  if (used < dimension || !std::isfinite(system.cost))
    return false;
  if (linearize)
    system.hessian = (system.hessian + system.hessian.t()) * 0.5;
  out = std::move(system);
  return true;
}

bool refineTiltIllumination(const std::vector<Bitmap> &frames, const PointMask &points,
                            const PsiOptions &options, TiltPlanes &planes, int &iterations,
                            bool &converged) {
  if (frames.size() < 5 || iterations >= options.maxIterations)
    return false;
  const int width = frames[0].width(), height = frames[0].height();
  cv::Mat parameters = cv::Mat::zeros(3 * (static_cast<int>(frames.size()) - 1), 1, CV_64F);
  for (size_t j = 1; j < frames.size(); ++j) {
    const int index = 3 * (static_cast<int>(j) - 1);
    parameters.at<double>(index) = planes.shiftX[j] + planes.shiftY[j];
    parameters.at<double>(index + 1) = planes.tiltX[j] * std::max(1, width - 1);
    parameters.at<double>(index + 2) = planes.tiltY[j] * std::max(1, height - 1);
  }
  ProjectedShiftSystem current;
  if (!projectedTiltSystem(frames, points, parameters, true, current) ||
      !hasShiftInformation(current.hessian))
    return false;
  converged = false;
  double damping = 1e-3;
  while (iterations < options.maxIterations) {
    ++iterations;
    cv::Mat rawStep;
    if (!cv::solve(current.hessian, current.gradient, rawStep, cv::DECOMP_CHOLESKY))
      break;
    if (cv::norm(rawStep, cv::NORM_INF) < options.tolerance) {
      converged = true;
      break;
    }
    bool accepted = false;
    for (int attempt = 0; attempt < 10; ++attempt) {
      cv::Mat h = current.hessian.clone(), step;
      for (int p = 0; p < h.rows; ++p)
        h.at<double>(p, p) *= 1.0 + damping;
      if (!cv::solve(h, current.gradient, step, cv::DECOMP_CHOLESKY))
        break;
      const double maximum = cv::norm(step, cv::NORM_INF);
      if (!std::isfinite(maximum))
        break;
      if (maximum > 0.25)
        step *= 0.25 / maximum;
      cv::Mat trial = parameters + step;
      ProjectedShiftSystem candidate;
      if (projectedTiltSystem(frames, points, trial, false, candidate) &&
          candidate.cost < current.cost) {
        parameters = std::move(trial);
        accepted = true;
        damping = std::max(1e-12, damping * 0.1);
        break;
      }
      damping *= 10.0;
    }
    if (!accepted)
      break;
    if (!projectedTiltSystem(frames, points, parameters, true, current) ||
        !hasShiftInformation(current.hessian))
      break;
  }
  for (size_t j = 1; j < frames.size(); ++j) {
    const int index = 3 * (static_cast<int>(j) - 1);
    planes.shiftX[j] = parameters.at<double>(index);
    planes.shiftY[j] = 0.0;
    planes.tiltX[j] = parameters.at<double>(index + 1) / std::max(1, width - 1);
    planes.tiltY[j] = parameters.at<double>(index + 2) / std::max(1, height - 1);
  }
  return true;
}

/// Развёртка сдвигов линий (столбцов или строк) по порядку и взвешенная
/// прямая value = slope*coordinate + intercept. Линии с weight == 0
/// пропускаются. false, если годных линий меньше двух.
bool fitUnwrappedLine(const std::vector<double> &values, const std::vector<double> &weights,
                      int size, double &slope, double &intercept) {
  const double twoPi = 2.0 * std::acos(-1.0);
  double sw = 0.0, swt = 0.0, swtt = 0.0, swu = 0.0, swtu = 0.0;
  bool started = false;
  double previousRaw = 0.0, previousUnwrapped = 0.0;
  int lines = 0;
  for (int i = 0; i < size; ++i) {
    if (weights[i] <= 0.0)
      continue;
    const double unwrapped =
        started ? previousUnwrapped + std::remainder(values[i] - previousRaw, twoPi) : values[i];
    started = true;
    previousRaw = values[i];
    previousUnwrapped = unwrapped;
    const double t = centered(i, size), w = weights[i];
    sw += w;
    swt += w * t;
    swtt += w * t * t;
    swu += w * unwrapped;
    swtu += w * t * unwrapped;
    ++lines;
  }
  const double denominator = sw * swtt - swt * swt;
  if (lines < 2 || denominator <= 0.0)
    return false;
  slope = (sw * swtu - swt * swu) / denominator;
  intercept = std::remainder((swu - slope * swt) / sw, twoPi);
  return true;
}

/// Итерации метода Liu на точках маски. Фаза хранится как cos и sin --
/// так шаги 2 и 3 обходятся без тригонометрии в каждой точке.
class TiltIterations {
public:
  TiltIterations(const std::vector<Bitmap> &frames, const PointMask &points)
      : m_frames(frames), m_points(points), m_width(frames[0].width()),
        m_height(frames[0].height()), m_cosPhi(points.size(), 0.0), m_sinPhi(points.size(), 0.0),
        m_valid(points.size(), 0), m_cosRow(frames.size() * m_height),
        m_sinRow(frames.size() * m_height), m_cosColumn(frames.size() * m_width),
        m_sinColumn(frames.size() * m_width) {}

  /// Шаг 1 на точках маски; заодно -- невязка модели.
  void updatePhase(const TiltPlanes &planes) {
    const size_t frameCount = m_frames.size();
    // Как в computePhaseTilted(): cos и sin y-части (со сдвигом) -- по
    // строкам, x-части -- по столбцам, в точке -- формула сложения.
    for (size_t n = 0; n < frameCount; ++n) {
      for (int y = 0; y < m_height; ++y) {
        const double angle = planes.shiftX[n] + planes.shiftY[n] +
                             planes.tiltY[n] * centered(y, m_height);
        m_cosRow[n * m_height + y] = std::cos(angle);
        m_sinRow[n * m_height + y] = std::sin(angle);
      }
      for (int x = 0; x < m_width; ++x) {
        const double angle = planes.tiltX[n] * centered(x, m_width);
        m_cosColumn[n * m_width + x] = std::cos(angle);
        m_sinColumn[n * m_width + x] = std::sin(angle);
      }
    }
    std::vector<double> cosTheta(frameCount), sinTheta(frameCount);
    double sumSq = 0.0;
    long long terms = 0;
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const size_t i = static_cast<size_t>(y) * m_width + x;
        m_valid[i] = 0;
        if (!m_points[i])
          continue;
        CosineFit fit;
        for (size_t n = 0; n < frameCount; ++n) {
          const double cr = m_cosRow[n * m_height + y], sr = m_sinRow[n * m_height + y];
          const double cc = m_cosColumn[n * m_width + x], sc = m_sinColumn[n * m_width + x];
          cosTheta[n] = cr * cc - sr * sc;
          sinTheta[n] = sr * cc + cr * sc;
          fit.addCosSin(cosTheta[n], sinTheta[n], m_frames[n].pixel(x, y));
        }
        double a = 0.0, c = 0.0, s = 0.0;
        if (!fit.solve(a, c, s))
          continue;
        const double b = std::sqrt(c * c + s * s);
        if (b <= 0.0)
          continue;
        m_cosPhi[i] = c / b;
        m_sinPhi[i] = s / b;
        m_valid[i] = 1;
        for (size_t n = 0; n < frameCount; ++n) {
          const double r = m_frames[n].pixel(x, y) - (a + c * cosTheta[n] - s * sinTheta[n]);
          sumSq += r * r;
          ++terms;
        }
      }
    }
    m_residual = terms > 0 ? std::sqrt(sumSq / static_cast<double>(terms))
                           : std::numeric_limits<double>::infinity();
  }

  /// Шаг 2 (alongColumns) или 3: сдвиг каждой линии каждого кадра при
  /// известной другой части плоскости -> прямая -> наклон и сдвиг.
  void fitLines(TiltPlanes &planes, bool alongColumns) const {
    const int lineCount = alongColumns ? m_width : m_height;
    const int otherSize = alongColumns ? m_height : m_width;
    std::vector<double> cosOther(otherSize), sinOther(otherSize);
    for (size_t n = 1; n < m_frames.size(); ++n) {
      // Известная часть плоскости: для столбцов -- y-часть, для строк -- x-часть.
      for (int j = 0; j < otherSize; ++j) {
        const double angle = alongColumns
                                 ? planes.shiftY[n] + planes.tiltY[n] * centered(j, m_height)
                                 : planes.shiftX[n] + planes.tiltX[n] * centered(j, m_width);
        cosOther[j] = std::cos(angle);
        sinOther[j] = std::sin(angle);
      }
      std::vector<CosineFit> fits(lineCount);
      for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
          const size_t i = static_cast<size_t>(y) * m_width + x;
          if (!m_valid[i])
            continue;
          const int j = alongColumns ? y : x;
          // theta = phi + известная часть; cos и sin -- по формуле сложения.
          const double cosTheta = m_cosPhi[i] * cosOther[j] - m_sinPhi[i] * sinOther[j];
          const double sinTheta = m_sinPhi[i] * cosOther[j] + m_cosPhi[i] * sinOther[j];
          fits[alongColumns ? x : y].addCosSin(cosTheta, sinTheta, m_frames[n].pixel(x, y));
        }
      }
      std::vector<double> lineShifts(lineCount, 0.0), weights(lineCount, 0.0);
      for (int line = 0; line < lineCount; ++line) {
        double a = 0.0, c = 0.0, s = 0.0;
        if (fits[line].solve(a, c, s)) {
          lineShifts[line] = std::atan2(s, c);  // (10), (12)
          weights[line] = fits[line].count();
        }
      }
      double slope = 0.0, intercept = 0.0;
      if (!fitUnwrappedLine(lineShifts, weights, lineCount, slope, intercept))
        continue;  // мало годных линий -- прежняя оценка остаётся
      (alongColumns ? planes.tiltX : planes.tiltY)[n] = slope;
      (alongColumns ? planes.shiftX : planes.shiftY)[n] = intercept;
    }
  }

  double residual() const { return m_residual; }

private:
  const std::vector<Bitmap> &m_frames;
  const PointMask &m_points;
  int m_width, m_height;
  std::vector<double> m_cosPhi, m_sinPhi;
  PointMask m_valid;
  std::vector<double> m_cosRow, m_sinRow, m_cosColumn, m_sinColumn;
  double m_residual = std::numeric_limits<double>::infinity();
};

/// Преобладающая частота полос кадра по пику спектра, периодов на пиксель.
/// Знак не определён: у спектра вещественного кадра пики +f и -f равны.
///
/// Спектр -- по прямоугольнику вокруг точек апертуры, а не по всему кадру,
/// и пик уточняется параболой между ячейками: иначе оценка зависела от
/// размера холста (в GUI кадры дополнены полем, см. padImageBackground) --
/// у изогнутых полос пик широкий, его максимум перескакивал в соседнюю
/// ячейку, и стартовый наклон всех кадров (он отсчитывается от кадра 0)
/// сдвигался на ~1.5 волны -- итерации застревали в ложном минимуме.
bool fringeFrequency(const Bitmap &frame, const PointMask &points, double &fx, double &fy) {
  const int frameWidth = frame.width(), frameHeight = frame.height();
  int minX = frameWidth, maxX = -1, minY = frameHeight, maxY = -1;
  double sum = 0.0;
  long long count = 0;
  for (int y = 0; y < frameHeight; ++y)
    for (int x = 0; x < frameWidth; ++x)
      if (points[static_cast<size_t>(y) * frameWidth + x]) {
        sum += frame.pixel(x, y);
        ++count;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
      }
  if (count == 0)
    return false;
  const double mean = sum / static_cast<double>(count);
  const int width = maxX - minX + 1, height = maxY - minY + 1;
  cv::Mat image(height, width, CV_32F, cv::Scalar(0));
  for (int y = minY; y <= maxY; ++y)
    for (int x = minX; x <= maxX; ++x)
      if (points[static_cast<size_t>(y) * frameWidth + x])
        image.at<float>(y - minY, x - minX) = static_cast<float>(frame.pixel(x, y) - mean);
  cv::Mat spectrum;
  cv::dft(image, spectrum, cv::DFT_COMPLEX_OUTPUT);
  auto power = [&](int row, int column) {
    const cv::Vec2f value =
        spectrum.at<cv::Vec2f>((row + height) % height, (column + width) % width);
    return static_cast<double>(value[0]) * value[0] + static_cast<double>(value[1]) * value[1];
  };

  // Окрестность нулевой частоты -- фон и огибающая апертуры, не полосы.
  constexpr int kLowFrequency = 2;
  double best = -1.0;
  int bestRow = 0, bestColumn = 0, bestKx = 0, bestKy = 0;
  for (int row = 0; row < height; ++row) {
    const int ky = row < (height + 1) / 2 ? row : row - height;
    for (int column = 0; column < width; ++column) {
      const int kx = column < (width + 1) / 2 ? column : column - width;
      if (std::abs(kx) <= kLowFrequency && std::abs(ky) <= kLowFrequency)
        continue;
      const double p = power(row, column);
      if (p > best) {
        best = p;
        bestRow = row;
        bestColumn = column;
        bestKx = kx;
        bestKy = ky;
      }
    }
  }
  if (best <= 0.0)
    return false;
  // Вершина параболы по трём ячейкам вдоль каждой оси.
  auto vertex = [](double left, double center, double right) {
    const double denominator = left - 2.0 * center + right;
    return std::fabs(denominator) > 1e-30 ? 0.5 * (left - right) / denominator : 0.0;
  };
  const double dx = vertex(power(bestRow, bestColumn - 1), best, power(bestRow, bestColumn + 1));
  const double dy = vertex(power(bestRow - 1, bestColumn), best, power(bestRow + 1, bestColumn));
  fx = (bestKx + std::clamp(dx, -0.5, 0.5)) / width;
  fy = (bestKy + std::clamp(dy, -0.5, 0.5)) / height;
  return true;
}

}  // namespace

bool solveWithTiltShifts(const std::vector<Bitmap> &frames,
                         const std::vector<double> &expectedShifts,
                         const std::function<bool(int, int)> &isVisible,
                         const PsiOptions &options, PsiResult &out, std::string &error) {
  if (!checkFrames(frames, error))
    return false;
  if (expectedShifts.size() != frames.size()) {
    error = "Number of phase shifts (" + std::to_string(expectedShifts.size()) +
            ") does not match the number of frames (" + std::to_string(frames.size()) + ")";
    return false;
  }
  const size_t frameCount = frames.size();
  const std::vector<double> expected = normalizedShifts(expectedShifts);
  const PointMask points = selectShiftEstimationPoints(frames, isVisible, options.minFringeSpan);
  const int width = frames[0].width(), height = frames[0].height();

  // Старты: без наклона (как в статье) и с наклоном по сдвигу пика
  // спектра относительно первого кадра -- обоими знаками.
  std::vector<TiltPlanes> starts(1, TiltPlanes(frameCount));
  {
    std::vector<double> fx(frameCount), fy(frameCount);
    bool spectrumOk = true;
    for (size_t n = 0; n < frameCount && spectrumOk; ++n)
      spectrumOk = fringeFrequency(frames[n], points, fx[n], fy[n]);
    if (spectrumOk) {
      const double twoPi = 2.0 * std::acos(-1.0);
      TiltPlanes plus(frameCount), minus(frameCount);
      for (size_t n = 1; n < frameCount; ++n) {
        // Из двух равных пиков кадра n берём тот, что ближе к пику кадра 0.
        const double sign = fx[n] * fx[0] + fy[n] * fy[0] < 0.0 ? -1.0 : 1.0;
        plus.tiltX[n] = twoPi * (sign * fx[n] - fx[0]);
        plus.tiltY[n] = twoPi * (sign * fy[n] - fy[0]);
        minus.tiltX[n] = -plus.tiltX[n];
        minus.tiltY[n] = -plus.tiltY[n];
      }
      starts.push_back(plus);
      starts.push_back(minus);
    }
  }

  struct Run {
    TiltPlanes planes;
    int iterations = 0;
    bool converged = false;
    double residual = std::numeric_limits<double>::infinity();
  };
  // Итерации -- на прореженных точках (PsiOptions::maxShiftPoints), выбор
  // старта -- на ещё более редких; спектр выше и диагностика ниже -- по всем.
  const PointMask shiftPoints = thinPoints(points, width, options.maxShiftPoints);
  const PointMask screeningPoints = thinPoints(points, width, options.maxScreeningPoints);
  TiltIterations iterations(frames, shiftPoints);
  TiltIterations screening(frames, screeningPoints);
  const double halfWidth = (width - 1) / 2.0, halfHeight = (height - 1) / 2.0;
  // Цикл шагов 1-3 до сходимости или до limit итераций всего.
  auto iterate = [&](TiltIterations &iterations, Run &run, int limit) {
    iterations.updatePhase(run.planes);
    while (run.iterations < limit && !run.converged) {
      ++run.iterations;
      const TiltPlanes previous = run.planes;
      iterations.fitLines(run.planes, true);
      iterations.updatePhase(run.planes);
      iterations.fitLines(run.planes, false);
      iterations.updatePhase(run.planes);

      // (13): насколько за цикл сдвинулись сдвиг в центре и плоскость --
      // наклон переведён в радианы на краю кадра.
      double change = 0.0;
      for (size_t n = 1; n < frameCount; ++n) {
        const double center = std::remainder(run.planes.shiftX[n] + run.planes.shiftY[n] -
                                                 previous.shiftX[n] - previous.shiftY[n],
                                             2.0 * std::acos(-1.0));
        const double edge = std::fabs(run.planes.tiltX[n] - previous.tiltX[n]) * halfWidth +
                            std::fabs(run.planes.tiltY[n] - previous.tiltY[n]) * halfHeight;
        change = std::max({change, std::fabs(center), edge});
      }
      run.converged = change < options.tolerance;
    }
    run.residual = iterations.residual();
  };

  // Все старты -- немного итераций, до конца доводится только лучший по
  // невязке: неудачный старт за это время уже явно хуже.
  constexpr int kScreeningIterations = 15;
  Run best{TiltPlanes(frameCount)};
  for (const TiltPlanes &start : starts) {
    Run run{start};
    run.planes.shiftX = expected;
    iterate(screening, run, std::min(kScreeningIterations, options.maxIterations));
    if (run.residual < best.residual)
      best = run;
  }
  if (!std::isfinite(best.residual)) {
    error = "Cannot estimate tilted phase shifts: too few points with fringes";
    return false;
  }
  // На редкой сетке сходимость могла наступить раньше -- проверяется заново.
  best.converged = false;
  iterate(iterations, best, options.maxIterations);

  PsiResult result;
  result.illuminationRefined = refineTiltIllumination(frames, shiftPoints, options, best.planes,
                                                      best.iterations, best.converged);
  if (!computePhaseTilted(frames, best.planes.centerShifts(), best.planes.tiltX,
                          best.planes.tiltY, isVisible, result, error))
    return false;
  result.iterations = best.iterations;
  result.converged = best.converged;
  if (!preferExpectedDirection(frames, expected, isVisible, result, error))
    return false;
  result.diagnostics = diagnose(frames, result, points);
  out = std::move(result);
  return true;
}

}  // namespace digitqt::core::psi
