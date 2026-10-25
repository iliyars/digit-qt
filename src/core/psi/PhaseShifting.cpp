#include "PhaseShifting.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace digitqt::core::psi {

namespace {

/// Обращение матрицы 3x3 через алгебраические дополнения. false, если
/// определитель слишком мал по сравнению с самими элементами.
bool invert3x3(const double m[3][3], double inv[3][3]) {
  const double c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
  const double c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
  const double c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
  const double det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;

  double scale = 0.0;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      scale = std::max(scale, std::fabs(m[i][j]));
  if (std::fabs(det) <= 1e-12 * scale * scale * scale)
    return false;

  inv[0][0] = c00 / det;
  inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / det;
  inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / det;
  inv[1][0] = c01 / det;
  inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / det;
  inv[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / det;
  inv[2][0] = c02 / det;
  inv[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / det;
  inv[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / det;
  return true;
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
/// -sin(delta_n)] и обращённая нормальная матрица A^T*A. Она одна на все
/// пиксели -- меняется только правая часть A^T*I.
struct ShiftModel {
  std::vector<double> cosShift, sinShift;
  double inv[3][3];

  bool build(const std::vector<double> &shifts) {
    const size_t frameCount = shifts.size();
    cosShift.resize(frameCount);
    sinShift.resize(frameCount);
    double matrix[3][3] = {};
    for (size_t n = 0; n < frameCount; ++n) {
      cosShift[n] = std::cos(shifts[n]);
      sinShift[n] = std::sin(shifts[n]);
      const double row[3] = {1.0, cosShift[n], -sinShift[n]};
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          matrix[i][j] += row[i] * row[j];
    }
    return invert3x3(matrix, inv);
  }

  /// Подгонка a + c*cos(delta) - s*sin(delta) к кадрам в точке (x, y).
  void fit(const std::vector<Bitmap> &frames, int x, int y, double &a, double &c,
           double &s) const {
    double rhs[3] = {};
    for (size_t n = 0; n < frames.size(); ++n) {
      const double intensity = frames[n].pixel(x, y);
      rhs[0] += intensity;
      rhs[1] += cosShift[n] * intensity;
      rhs[2] -= sinShift[n] * intensity;
    }
    a = inv[0][0] * rhs[0] + inv[0][1] * rhs[1] + inv[0][2] * rhs[2];
    c = inv[1][0] * rhs[0] + inv[1][1] * rhs[1] + inv[1][2] * rhs[2];
    s = inv[2][0] * rhs[0] + inv[2][1] * rhs[1] + inv[2][2] * rhs[2];
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
double halvesDisagreementDeg(const std::vector<Bitmap> &frames, const PhaseMap &phase,
                             const PointMask &usable, const Tilts &tilts, bool alongX) {
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
  if (!fitFrameShifts(frames, phase, low, tilts, lowShifts, used, ignored) ||
      !fitFrameShifts(frames, phase, high, tilts, highShifts, used, ignored))
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
  for (int y = 0; y < phase.height(); ++y) {
    for (int x = 0; x < phase.width(); ++x) {
      const size_t i = static_cast<size_t>(y) * phase.width() + x;
      if (points[i] && phase.hasValue(x, y)) {
        usable[i] = 1;
        ++diagnostics.shiftPoints;
      }
    }
  }

  // С наклонами половины сравниваются по сдвигу в центре при уже
  // найденных наклонах: расхождение тогда -- то, что плоскость не
  // описала (или нехватка полос).
  const Tilts tilts{result.tiltX, result.tiltY};
  const double alongX = halvesDisagreementDeg(frames, phase, usable, tilts, true);
  const double alongY = halvesDisagreementDeg(frames, phase, usable, tilts, false);
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
  // Строка системы: [1, cos(theta), -sin(theta)] * [a, c, s]^T = I.
  const double row[3] = {1.0, cosTheta, -sinTheta};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      m_matrix[i][j] += row[i] * row[j];
    m_rhs[i] += row[i] * intensity;
  }
  ++m_count;
}

bool CosineFit::solve(double &a, double &c, double &s) const {
  double inv[3][3];
  if (m_count < 3 || !invert3x3(m_matrix, inv))
    return false;
  a = inv[0][0] * m_rhs[0] + inv[0][1] * m_rhs[1] + inv[0][2] * m_rhs[2];
  c = inv[1][0] * m_rhs[0] + inv[1][1] * m_rhs[1] + inv[1][2] * m_rhs[2];
  s = inv[2][0] * m_rhs[0] + inv[2][1] * m_rhs[1] + inv[2][2] * m_rhs[2];
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

  // Матрица системы зависит только от сдвигов, а они одни на все пиксели:
  // обращается один раз (см. ShiftModel).
  ShiftModel model;
  if (!model.build(shifts)) {
    error = "Phase shifts are degenerate: at least 3 distinct shifts are needed";
    return false;
  }

  PhaseMap phase(width, height);
  PhaseMap modulation(width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!isVisible(x, y))
        continue;
      double a = 0.0, c = 0.0, s = 0.0;
      model.fit(frames, x, y, a, c, s);

      phase.setValue(x, y, std::atan2(s, c));                                  // (7)
      modulation.setValue(x, y, a > 0.0 ? std::sqrt(c * c + s * s) / a : 0.0);  // (8)
    }
  }

  out.wrappedPhase = std::move(phase);
  out.modulation = std::move(modulation);
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
      phase.setValue(x, y, std::atan2(s, c));
      modulation.setValue(x, y, a > 0.0 ? std::sqrt(c * c + s * s) / a : 0.0);
    }
  }

  out.wrappedPhase = std::move(phase);
  out.modulation = std::move(modulation);
  out.shifts = shifts;
  out.tiltX = tiltX;
  out.tiltY = tiltY;
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

  PsiResult result;
  if (!computePhase(frames, shifts, isVisible, result, error))
    return false;
  // Один раз на весь расчёт -- см. selectShiftEstimationPoints().
  const PointMask points = selectShiftEstimationPoints(frames, isVisible, options.minFringeSpan);

  for (int iteration = 1; iteration <= options.maxIterations; ++iteration) {
    std::vector<double> next;
    if (!estimateShifts(frames, result, points, next, error))
      return false;

    // (16): насколько сдвинулись оценки за этот шаг, СКО по кадрам.
    double sumSq = 0.0;
    for (size_t n = 0; n < next.size(); ++n) {
      const double change = std::remainder(next[n] - shifts[n], 2.0 * std::acos(-1.0));
      sumSq += change * change;
    }
    const double change = std::sqrt(sumSq / next.size());

    shifts = std::move(next);
    if (!computePhase(frames, shifts, isVisible, result, error))
      return false;
    result.iterations = iteration;
    if (change < options.tolerance) {
      result.converged = true;
      break;
    }
  }

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
bool fringeFrequency(const Bitmap &frame, const PointMask &points, double &fx, double &fy) {
  const int width = frame.width(), height = frame.height();
  double sum = 0.0;
  long long count = 0;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (points[static_cast<size_t>(y) * width + x]) {
        sum += frame.pixel(x, y);
        ++count;
      }
  if (count == 0)
    return false;
  const double mean = sum / static_cast<double>(count);
  cv::Mat image(height, width, CV_32F, cv::Scalar(0));
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (points[static_cast<size_t>(y) * width + x])
        image.at<float>(y, x) = static_cast<float>(frame.pixel(x, y) - mean);
  cv::Mat spectrum;
  cv::dft(image, spectrum, cv::DFT_COMPLEX_OUTPUT);

  // Окрестность нулевой частоты -- фон и огибающая апертуры, не полосы.
  constexpr int kLowFrequency = 2;
  double best = -1.0;
  for (int row = 0; row < height; ++row) {
    const int ky = row < (height + 1) / 2 ? row : row - height;
    for (int column = 0; column < width; ++column) {
      const int kx = column < (width + 1) / 2 ? column : column - width;
      if (std::abs(kx) <= kLowFrequency && std::abs(ky) <= kLowFrequency)
        continue;
      const cv::Vec2f value = spectrum.at<cv::Vec2f>(row, column);
      const double power = static_cast<double>(value[0]) * value[0] +
                           static_cast<double>(value[1]) * value[1];
      if (power > best) {
        best = power;
        fx = static_cast<double>(kx) / width;
        fy = static_cast<double>(ky) / height;
      }
    }
  }
  return best > 0.0;
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
  TiltIterations iterations(frames, points);
  const double halfWidth = (width - 1) / 2.0, halfHeight = (height - 1) / 2.0;
  // Цикл шагов 1-3 до сходимости или до limit итераций всего.
  auto iterate = [&](Run &run, int limit) {
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
    iterate(run, std::min(kScreeningIterations, options.maxIterations));
    if (run.residual < best.residual)
      best = run;
  }
  if (!std::isfinite(best.residual)) {
    error = "Cannot estimate tilted phase shifts: too few points with fringes";
    return false;
  }
  iterate(best, options.maxIterations);

  PsiResult result;
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
