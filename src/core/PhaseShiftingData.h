#pragma once

#include "core/Bitmap.h"

#include <utility>
#include <vector>

namespace digitqt::core {

/**
 * @brief Данные фазового сдвига (PSI) в Measurement: серия кадров одной
 * интерферограммы с разным сдвигом опорной волны и итог последнего расчёта.
 *
 * Measurement::image() при этом -- первый кадр серии: на нём рисуются
 * апертура и реперы, и они относятся ко всем кадрам сразу (кадры одного
 * размера, сняты одной камерой).
 */
class PhaseShiftingData {
public:
  const std::vector<Bitmap> &frames() const { return m_frames; }
  bool hasSeries() const { return m_frames.size() >= 3; }

  /// Ожидаемый ("паспортный") шаг сдвига между соседними кадрами, в
  /// градусах -- только начальное приближение: реальные сдвиги
  /// уточняются по самим кадрам (см. psi::solveWithUnknownShifts()).
  double nominalStepDeg() const { return m_nominalStepDeg; }
  void setNominalStepDeg(double step) { m_nominalStepDeg = step; }

  /// Итог последнего расчёта S2 методом PSI -- для показа пользователю.
  struct Report {
    std::vector<double> shiftsDeg;  ///< найденный сдвиг каждого кадра, первый = 0
    int iterations = 0;
    bool converged = false;
    bool mirrored = false;  ///< см. psi::PsiResult::mirrored
    int regionCount = 0;    ///< см. UnwrapResult::regionCount
    int maskedLowModulation = 0;  ///< точек апертуры, выброшенных из-за слабых полос
  };
  const Report &report() const { return m_report; }
  void setReport(Report report) { m_report = std::move(report); }

private:
  friend class Measurement;
  std::vector<Bitmap> m_frames;
  double m_nominalStepDeg = 90.0;
  Report m_report;
};

}  // namespace digitqt::core
