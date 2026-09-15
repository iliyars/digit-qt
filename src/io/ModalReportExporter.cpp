#include "ModalReportExporter.h"

#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/ModalFitMethod.h"
#include "core/PolynomialBasis.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace digitqt::core::io {

namespace {

QString basisName(digitqt::core::PolynomialBasis basis) {
  return basis == digitqt::core::PolynomialBasis::Zernike
             ? QStringLiteral("Zernike (classical textbook polynomials)")
             : QStringLiteral("Seregin (DAPPSIM/WinFringe-compatible)");
}

QString fitMethodName(digitqt::core::ModalFitMethod method) {
  switch (method) {
    case digitqt::core::ModalFitMethod::GramSchmidtOnAperture:
      return QStringLiteral("Gram-Schmidt on aperture");
    case digitqt::core::ModalFitMethod::SequentialSeregin:
      return QStringLiteral("Sequential Seregin (exact DAPPSIM Fitting.cpp algorithm)");
    case digitqt::core::ModalFitMethod::JointLeastSquares:
    default:
      return QStringLiteral("Joint least squares (no orthogonalization)");
  }
}

QString notSubtracted() {
  return QStringLiteral("not subtracted -- still in residual");
}

}  // namespace

bool writeModalReport(const QString &path, const digitqt::core::Measurement &measurement,
                      QString &errorMessage) {
  const auto data = digitqt::core::buildModalAnalysisReportData(measurement);
  if (data.isEmpty) {
    errorMessage = QStringLiteral("No S5 (Polynomial / Modal Analysis) result yet");
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    errorMessage = QStringLiteral("Could not open %1 for writing").arg(path);
    return false;
  }

  const auto &c = data.coefficients;
  const auto &sel = data.selection;

  QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  out.setEncoding(QStringConverter::Utf8);
#else
  out.setCodec("UTF-8");
#endif

  const QChar degree(0x00B0);
  const QChar lambda(0x03BB);

  out << "DigitQt -- Polynomial / Modal Analysis (S5) report\n";
  out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  out << "Wavelength: " << data.wavelengthNm << " nm\n";
  out << "Polynomial basis: " << basisName(data.basis) << "\n";
  out << "Fit method: " << fitMethodName(measurement.modalFitMethod()) << "\n";
  out << "\n";

  out << "Setup geometry (not a surface property):\n";
  out << "  Piston: " << QString::number(c.piston, 'f', 2) << " nm\n";
  if (sel.tilt)
    out << "  Tilt X/Y: " << QString::number(c.tiltX, 'f', 2) << " / "
        << QString::number(c.tiltY, 'f', 2) << " nm\n";
  else
    out << "  Tilt X/Y: " << notSubtracted() << "\n";
  if (sel.defocus)
    out << "  Defocus: " << QString::number(c.defocus, 'f', 2) << " nm\n";
  else
    out << "  Defocus: " << notSubtracted() << "\n";
  out << "\n";

  out << "Surface aberrations:\n";
  if (sel.astigmatism)
    out << "  Astigmatism: " << QString::number(data.astigMagnitude, 'f', 2) << " nm at "
        << QString::number(data.astigAngleDeg, 'f', 1) << degree << "  (X: "
        << QString::number(c.astigX, 'f', 2) << " nm, Y: " << QString::number(c.astigY, 'f', 2)
        << " nm)\n";
  else
    out << "  Astigmatism: " << notSubtracted() << "\n";
  if (sel.coma)
    out << "  Coma: " << QString::number(data.comaMagnitude, 'f', 2) << " nm at "
        << QString::number(data.comaAngleDeg, 'f', 1) << degree << "  (X: "
        << QString::number(c.comaX, 'f', 2) << " nm, Y: " << QString::number(c.comaY, 'f', 2)
        << " nm)\n";
  else
    out << "  Coma: " << notSubtracted() << "\n";
  if (sel.trefoil)
    out << "  Trefoil: " << QString::number(data.trefoilMagnitude, 'f', 2) << " nm at "
        << QString::number(data.trefoilAngleDeg, 'f', 1) << degree << "  (X: "
        << QString::number(c.trefoilX, 'f', 2) << " nm, Y: " << QString::number(c.trefoilY, 'f', 2)
        << " nm)\n";
  else
    out << "  Trefoil: " << notSubtracted() << "\n";
  if (sel.spherical)
    out << "  Spherical (3rd order): " << QString::number(c.spherical, 'f', 2) << " nm\n";
  else
    out << "  Spherical (3rd order): " << notSubtracted() << "\n";
  out << "\n";

  const double rmsBeforeWaves = data.wavelengthNm > 0.0 ? data.rmsBeforeNm / data.wavelengthNm : 0.0;
  const double rmsAfterWaves = data.wavelengthNm > 0.0 ? data.rmsAfterNm / data.wavelengthNm : 0.0;
  const double pvBeforeWaves = data.wavelengthNm > 0.0 ? data.pvBeforeNm / data.wavelengthNm : 0.0;
  const double pvAfterWaves = data.wavelengthNm > 0.0 ? data.pvAfterNm / data.wavelengthNm : 0.0;

  out << "RMS before: " << QString::number(data.rmsBeforeNm, 'f', 2) << " nm ("
      << QString::number(rmsBeforeWaves, 'f', 3) << " " << lambda << ")\n";
  out << "RMS after: " << QString::number(data.rmsAfterNm, 'f', 2) << " nm ("
      << QString::number(rmsAfterWaves, 'f', 3) << " " << lambda << ")\n";
  out << "PV before: " << QString::number(data.pvBeforeNm, 'f', 2) << " nm ("
      << QString::number(pvBeforeWaves, 'f', 3) << " " << lambda << ")\n";
  out << "PV after: " << QString::number(data.pvAfterNm, 'f', 2) << " nm ("
      << QString::number(pvAfterWaves, 'f', 3) << " " << lambda << ")\n";
  out << "Strehl (Marechal est.): " << QString::number(data.strehl, 'f', 3) << "\n";

  if (data.fitMethod == digitqt::core::ModalFitMethod::SequentialSeregin) {
    const auto &sq = data.sequential;
    const double lam = data.wavelengthNm;
    auto waves = [&](double nm) { return lam > 0.0 ? nm / lam : 0.0; };
    auto line = [&](const QString &label, double nm) {
      return QStringLiteral("  %1: %2 nm (%3 %4)\n")
          .arg(label, QString::number(nm, 'f', 2), QString::number(waves(nm), 'f', 3), lambda);
    };

    out << "\n";
    out << "--- Raw sequential stages (exact DAPPSIM algorithm; byte-verified only for\n";
    out << "    piston/tiltX/tiltY/defocus against test_data/report.txt's D/Lx/Ly/C via\n";
    out << "    test_data/Terminal.pol -- everything below that is a faithful\n";
    out << "    transcription of Includes/Fitting.cpp, NOT independently verified\n";
    out << "    against report.txt's B0-B8/C(coma)/FIC labels: those are formatted by\n";
    out << "    WinFringe, whose source is unavailable) ---\n";
    out << line("Piston (stage 1)", sq.piston);
    out << line("Tilt X (stage 1)", sq.tiltX);
    out << line("Tilt Y (stage 1)", sq.tiltY);
    out << line("Defocus / \"C\" (stage 2, ρ²)", sq.defocus);
    out << line("Astig (3X²-1)/2 (stage 3)", sq.astig4);
    out << line("Astig XY (stage 3)", sq.astig5);
    out << line("Astig (3Y²-1)/2 (stage 3)", sq.astig6);
    out << line("Residual tilt X (stage 4)", sq.comaResidualX);
    out << line("Residual tilt Y (stage 4)", sq.comaResidualY);
    out << line("Coma X (stage 4)", sq.comaX);
    out << line("Coma Y (stage 4)", sq.comaY);
    out << line("Trefoil X (stage 4)", sq.trefoilX);
    out << line("Trefoil Y (stage 4)", sq.trefoilY);
    out << line("ρ² (stage 5, S3)", sq.s3rho2);
    out << line("ρ⁴ (stage 5, S3)", sq.s3rho4);
    out << line("ρ² (stage 6, S5)", sq.s5rho2);
    out << line("ρ⁴ (stage 6, S5)", sq.s5rho4);
    out << line("ρ⁶ (stage 6, S5)", sq.s5rho6);
    out << line("ρ² (stage 7, S7)", sq.s7rho2);
    out << line("ρ⁴ (stage 7, S7)", sq.s7rho4);
    out << line("ρ⁶ (stage 7, S7)", sq.s7rho6);
    out << line("ρ⁸ (stage 7, S7)", sq.s7rho8);
    out << line("Discarded stage intercepts (2-7, kept in residual calc only)",
                sq.discardedIntercepts);
    out << "\n";
    out << "RMS(W) initial: " << QString::number(waves(sq.rmsInitial), 'f', 3) << " " << lambda
        << "\n";
    out << "RMS after tilt+defocus: " << QString::number(waves(sq.rmsAfterTiltDefocus), 'f', 3)
        << " " << lambda << "\n";
    out << "RMS after astig: " << QString::number(waves(sq.rmsAfterAstig), 'f', 3) << " " << lambda
        << "\n";
    out << "RMS after coma+trefoil: " << QString::number(waves(sq.rmsAfterComa), 'f', 3) << " "
        << lambda << "\n";
    out << "RMS after S3: " << QString::number(waves(sq.rmsAfterS3), 'f', 3) << " " << lambda
        << "\n";
    out << "RMS after S5: " << QString::number(waves(sq.rmsAfterS5), 'f', 3) << " " << lambda
        << "\n";
    out << "RMS after S7: " << QString::number(waves(sq.rmsAfterS7), 'f', 3) << " " << lambda
        << "\n";
  }

  out.flush();
  if (file.error() != QFile::NoError) {
    errorMessage = file.errorString();
    return false;
  }
  return true;
}

}  // namespace digitqt::core::io
