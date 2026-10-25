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

  /// Учитывать наклон между кадрами (сдвиг кадра -- плоскость, см.
  /// psi::solveWithTiltShifts()). Медленнее и требует полос в каждом
  /// столбце и строке, поэтому по умолчанию выключено.
  bool compensateTilt() const { return m_compensateTilt; }
  void setCompensateTilt(bool on) { m_compensateTilt = on; }

  /// Уточнять сдвиги по кадрам. Выключено -- сдвиги считаются точно
  /// равными n * nominalStepDeg() (psi::solveWithKnownShifts()):
  /// для откалиброванного фазосдвигателя и для сравнения с уточнением.
  /// compensateTilt() тогда не действует.
  bool refineShifts() const { return m_refineShifts; }
  void setRefineShifts(bool on) { m_refineShifts = on; }

  /// Итог последнего расчёта S2 методом PSI -- для показа пользователю.
  struct Report {
    std::vector<double> shiftsDeg;  ///< найденный сдвиг каждого кадра, первый = 0
    bool refined = true;            ///< false -- shiftsDeg номинальные, см. refineShifts()
    int iterations = 0;
    bool converged = false;
    bool mirrored = false;  ///< см. psi::PsiResult::mirrored
    int regionCount = 0;    ///< см. UnwrapResult::regionCount
    int maskedLowModulation = 0;  ///< точек апертуры, выброшенных из-за слабых полос
    int droppedIslandPoints = 0;  ///< точек в мелких оторванных островках, тоже выброшенных
    // Надёжность сдвигов -- см. psi::PsiResult::Diagnostics.
    int shiftPoints = 0;
    double halvesDisagreementDeg = 0.0;  ///< NaN -- не посчитано
    double residual = 0.0;               ///< уровни яркости
    double amplitude = 0.0;              ///< уровни яркости
    /// Наклон сдвига каждого кадра относительно первого, в полосах
    /// (волнах) на ширину и высоту кадра. Пусто -- наклон не учитывался.
    std::vector<double> tiltXWaves, tiltYWaves;
  };
  const Report &report() const { return m_report; }
  void setReport(Report report) { m_report = std::move(report); }

private:
  friend class Measurement;
  std::vector<Bitmap> m_frames;
  double m_nominalStepDeg = 90.0;
  bool m_compensateTilt = false;
  bool m_refineShifts = true;
  Report m_report;
};

}  // namespace digitqt::core
