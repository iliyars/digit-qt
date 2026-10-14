#include "PhaseShifting.h"

#include <cmath>

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

}  // namespace

void CosineFit::add(double theta, double intensity) {
  // Строка системы: [1, cos(theta), -sin(theta)] * [a, c, s]^T = I.
  const double row[3] = {1.0, std::cos(theta), -std::sin(theta)};
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
  const int frameCount = static_cast<int>(frames.size());
  const int width = frames[0].width();
  const int height = frames[0].height();

  // Матрица системы зависит только от сдвигов, а они одни на все пиксели:
  // нормальная матрица A^T*A собирается и обращается ОДИН раз. От пикселя
  // к пикселю меняется только правая часть A^T*I.
  std::vector<double> cosShift(frameCount), sinShift(frameCount);
  double matrix[3][3] = {};
  for (int n = 0; n < frameCount; ++n) {
    cosShift[n] = std::cos(shifts[n]);
    sinShift[n] = std::sin(shifts[n]);
    const double row[3] = {1.0, cosShift[n], -sinShift[n]};
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        matrix[i][j] += row[i] * row[j];
  }
  double inv[3][3];
  if (!invert3x3(matrix, inv)) {
    error = "Phase shifts are degenerate: at least 3 distinct shifts are needed";
    return false;
  }

  PhaseMap phase(width, height);
  PhaseMap modulation(width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!isVisible(x, y))
        continue;
      double rhs[3] = {};
      for (int n = 0; n < frameCount; ++n) {
        const double intensity = frames[n].pixel(x, y);
        rhs[0] += intensity;
        rhs[1] += cosShift[n] * intensity;
        rhs[2] -= sinShift[n] * intensity;
      }
      const double a = inv[0][0] * rhs[0] + inv[0][1] * rhs[1] + inv[0][2] * rhs[2];
      const double c = inv[1][0] * rhs[0] + inv[1][1] * rhs[1] + inv[1][2] * rhs[2];
      const double s = inv[2][0] * rhs[0] + inv[2][1] * rhs[1] + inv[2][2] * rhs[2];

      phase.setValue(x, y, std::atan2(s, c));                                  // (7)
      modulation.setValue(x, y, a > 0.0 ? std::sqrt(c * c + s * s) / a : 0.0);  // (8)
    }
  }

  out.wrappedPhase = std::move(phase);
  out.modulation = std::move(modulation);
  out.shifts = shifts;
  return true;
}

bool estimateShifts(const std::vector<Bitmap> &frames, const PsiResult &current,
                    double minModulation, std::vector<double> &shifts, std::string &error) {
  if (!checkFrames(frames, error))
    return false;
  const PhaseMap &phase = current.wrappedPhase;
  if (phase.width() != frames[0].width() || phase.height() != frames[0].height()) {
    error = "Phase map size does not match the frames";
    return false;
  }

  std::vector<double> found;
  found.reserve(frames.size());
  for (size_t n = 0; n < frames.size(); ++n) {
    // Та же подгонка, что в computePhase(), только уравнения -- по
    // пикселям, а theta -- фаза в пикселе: I = a + (b*cos d)*cos(phi) - (b*sin d)*sin(phi).
    CosineFit fit;
    for (int y = 0; y < phase.height(); ++y) {
      for (int x = 0; x < phase.width(); ++x) {
        if (!phase.hasValue(x, y) || current.modulation.value(x, y) < minModulation)
          continue;
        fit.add(phase.value(x, y), frames[n].pixel(x, y));
      }
    }
    double a = 0.0, c = 0.0, s = 0.0;
    if (!fit.solve(a, c, s)) {
      error = "Cannot estimate the phase shift of frame " + std::to_string(n + 1) + ": " +
              std::to_string(fit.count()) + " usable point(s) with modulation >= " +
              std::to_string(minModulation);
      return false;
    }
    found.push_back(std::atan2(s, c));  // (15)
  }

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

bool solveWithUnknownShifts(const std::vector<Bitmap> &frames,
                            const std::vector<double> &expectedShifts,
                            const std::function<bool(int, int)> &isVisible,
                            const PsiOptions &options, PsiResult &out, std::string &error) {
  // Начальное приближение -- ожидаемые сдвиги, приведённые к тому же виду,
  // что возвращает estimateShifts(), чтобы первая разность в (16) не
  // учитывала просто другую точку отсчёта.
  std::vector<double> expected = expectedShifts;
  if (!expected.empty()) {
    const double twoPi = 2.0 * std::acos(-1.0);
    const double first = expected[0];
    for (double &shift : expected) {
      shift = std::fmod(shift - first, twoPi);
      if (shift < 0.0)
        shift += twoPi;
    }
  }
  std::vector<double> shifts = expected;

  PsiResult result;
  if (!computePhase(frames, shifts, isVisible, result, error))
    return false;

  for (int iteration = 1; iteration <= options.maxIterations; ++iteration) {
    std::vector<double> next;
    if (!estimateShifts(frames, result, options.minModulation, next, error))
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

  // Уравнения не различают решение (phi, delta) и зеркальное (-phi,
  // -delta): cos(phi + delta) == cos(-phi - delta). Какое из двух найдут
  // итерации, зависит от пути, а от выбора зависит знак всей карты (бугор
  // или яма). Берём то, чьи сдвиги ближе к ожидаемым -- единственное, что
  // знает о реальном направлении фазосдвигателя.
  const double twoPi = 2.0 * std::acos(-1.0);
  std::vector<double> mirrored(result.shifts.size());
  double directDistance = 0.0, mirroredDistance = 0.0;
  for (size_t n = 0; n < result.shifts.size(); ++n) {
    mirrored[n] = n == 0 ? 0.0 : twoPi - result.shifts[n];
    directDistance += std::fabs(std::remainder(result.shifts[n] - expected[n], twoPi));
    mirroredDistance += std::fabs(std::remainder(mirrored[n] - expected[n], twoPi));
  }
  if (mirroredDistance < directDistance) {
    const int iterations = result.iterations;
    const bool converged = result.converged;
    if (!computePhase(frames, mirrored, isVisible, result, error))
      return false;
    result.iterations = iterations;
    result.converged = converged;
    result.mirrored = true;
  }

  out = std::move(result);
  return true;
}

}  // namespace digitqt::core::psi
