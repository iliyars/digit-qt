#pragma once

#include "core/ModalAnalysisResult.h"
#include "core/ModalFitMethod.h"
#include "core/PolynomialBasis.h"
#include "core/SequentialSereginResult.h"

namespace digitqt::core {

class Measurement;

/**
 * @brief Every value shown on the S5 page of ParametersDock (and now also
 * written to the exported text report) -- computed once here, so the
 * on-screen panel and the exported file can never drift apart on the
 * astigmatism/coma/trefoil magnitude+angle conversion or the RMS/PV/Strehl
 * summary math.
 *
 * `isEmpty` mirrors ModalAnalysisResult::isEmpty() (no S5 computed yet).
 * astig/coma/trefoil magnitude+angle are the same conversion as before
 * (see ParametersDock's old inline computation this replaces): magnitude
 * = hypot(X, Y); angle = atan2(Y, X), halved for astigmatism (180°
 * period) and divided by 3 for trefoil (120° period, 3-fold symmetry).
 */
struct ModalAnalysisReportData {
  bool isEmpty = true;

  ModalCoefficients coefficients;
  ModalTermSelection selection;
  PolynomialBasis basis = PolynomialBasis::Seregin;
  ModalFitMethod fitMethod = ModalFitMethod::JointLeastSquares;

  /// Заполнено только если fitMethod == SequentialSeregin -- полный
  /// дамп всех 22 коэффициентов точной последовательной схемы DAPPSIM
  /// (см. SequentialSereginResult.h про то, что из них подтверждено
  /// против report.txt, а что нет).
  SequentialSereginResult sequential;

  double astigMagnitude = 0.0;
  double astigAngleDeg = 0.0;
  double comaMagnitude = 0.0;
  double comaAngleDeg = 0.0;
  double trefoilMagnitude = 0.0;
  double trefoilAngleDeg = 0.0;

  double wavelengthNm = 0.0;
  double rmsBeforeNm = 0.0;
  double rmsAfterNm = 0.0;
  double pvBeforeNm = 0.0;
  double pvAfterNm = 0.0;

  /// Maréchal approximation: Strehl ≈ exp(-(2π·RMS_wavefront/λ)²) --
  /// accurate for well-corrected systems (RMS ≲ 0.15λ); see
  /// ParametersDock's original comment on why rmsAfterNm (surface
  /// height) is doubled to get the wavefront (OPD) RMS this needs.
  double strehl = 0.0;
};

/// Computes ModalAnalysisReportData from measurement.modalAnalysis() +
/// measurement.wavefrontMap()/wavelengthNm(). Returns a mostly-default
/// (isEmpty=true) result if S5 hasn't been computed yet.
ModalAnalysisReportData buildModalAnalysisReportData(const Measurement &measurement);

}  // namespace digitqt::core
