#include "ModalReportExporter.h"

#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/ModalFitMethod.h"
#include "core/PolynomialBasis.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <utility>

namespace digitqt::core::io {

namespace {

/// Контекст перевода текста отчёта (translations/DigitQt_ru.ts). Язык --
/// тот же, что у интерфейса: переводчик ставит main.cpp; в консольных
/// утилитах (mtr_diff, psi_check) его нет, и отчёт остаётся английским.
class Report {
  Q_DECLARE_TR_FUNCTIONS(Report)
};

QString basisName(digitqt::core::PolynomialBasis basis) {
  return basis == digitqt::core::PolynomialBasis::Zernike
             ? Report::tr("Zernike (classical textbook polynomials)")
             : Report::tr("Seregin (DAPPSIM/WinFringe-compatible)");
}

QString fitMethodName(digitqt::core::ModalFitMethod method) {
  switch (method) {
    case digitqt::core::ModalFitMethod::GramSchmidtOnAperture:
      return Report::tr("Gram-Schmidt on aperture");
    case digitqt::core::ModalFitMethod::SequentialSeregin:
      return Report::tr("Sequential Seregin (exact DAPPSIM Fitting.cpp algorithm)");
    case digitqt::core::ModalFitMethod::JointLeastSquares:
    default:
      return Report::tr("Joint least squares (no orthogonalization)");
  }
}

QString notSubtracted() {
  return Report::tr("not subtracted -- still in residual");
}

QString phaseAlgorithmName(const digitqt::core::Measurement &measurement) {
  if (!measurement.customPhaseReconstructorPluginPath().empty())
    return Report::tr("plugin %1")
        .arg(QString::fromStdString(measurement.customPhaseReconstructorPluginPath()));
  switch (measurement.phaseReconstructionAlgorithm()) {
    case digitqt::core::PhaseReconstructionAlgorithm::HorizontalSpline:
      return Report::tr("Horizontal Spline (traced lines)");
    case digitqt::core::PhaseReconstructionAlgorithm::FourierTransform:
      return Report::tr("Fourier Transform Method (Takeda)");
    case digitqt::core::PhaseReconstructionAlgorithm::WaveletTransform:
      return Report::tr("Wavelet Transform Method (Zhong-Weng)");
    case digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting:
    default:
      return Report::tr("Phase Shifting (PSI)");
  }
}

/// Shape::typeName() -- английский идентификатор из aperture/, для отчёта
/// переводится здесь.
QString shapeName(const char *typeName) {
  const QString name = QString::fromLatin1(typeName);
  if (name == QLatin1String("Ellipse"))
    return Report::tr("ellipse");
  if (name == QLatin1String("Rectangle"))
    return Report::tr("rectangle");
  if (name == QLatin1String("Polygon"))
    return Report::tr("polygon");
  return name;
}

QString yesNo(bool value) { return value ? Report::tr("yes") : Report::tr("no"); }
QString onOff(bool value) { return value ? Report::tr("on") : Report::tr("off"); }

/// RMS (относительно среднего) и PV по определённым точкам карты.
void mapStats(const digitqt::core::PhaseMap &map, double &rms, double &pv, long long &points) {
  double sum = 0.0, sumSq = 0.0, low = 0.0, high = 0.0;
  points = 0;
  for (int y = 0; y < map.height(); ++y) {
    for (int x = 0; x < map.width(); ++x) {
      if (!map.hasValue(x, y))
        continue;
      const double v = map.value(x, y);
      if (points == 0)
        low = high = v;
      low = std::min(low, v);
      high = std::max(high, v);
      sum += v;
      sumSq += v * v;
      ++points;
    }
  }
  const double mean = points > 0 ? sum / static_cast<double>(points) : 0.0;
  rms = points > 0 ? std::sqrt(std::max(0.0, sumSq / static_cast<double>(points) - mean * mean))
                   : 0.0;
  pv = high - low;
}

QString fixed(double value, int decimals) { return QString::number(value, 'f', decimals); }

/// Секция S5 -- общая для writeModalReport() и writeFullReport().
void writeModalSection(QTextStream &out, const digitqt::core::ModalAnalysisReportData &data,
                       const digitqt::core::Measurement &measurement) {
  const auto &c = data.coefficients;
  const auto &sel = data.selection;
  const QChar lambda(0x03BB);

  out << Report::tr("Wavelength: %1 nm").arg(data.wavelengthNm) << "\n";
  out << Report::tr("Polynomial basis: %1").arg(basisName(data.basis)) << "\n";
  out << Report::tr("Fit method: %1").arg(fitMethodName(measurement.modalFitMethod())) << "\n";
  out << "\n";

  // Модуль, угол и составляющие X/Y одного терма.
  auto polar = [](double magnitude, double angleDeg, double x, double y) {
    return Report::tr("%1 nm at %2°  (X: %3 nm, Y: %4 nm)")
        .arg(fixed(magnitude, 2), fixed(angleDeg, 1), fixed(x, 2), fixed(y, 2));
  };
  auto nm = [](double value) { return Report::tr("%1 nm").arg(fixed(value, 2)); };

  out << Report::tr("Setup geometry (not a surface property):") << "\n";
  out << "  " << Report::tr("Piston: %1").arg(nm(c.piston)) << "\n";
  out << "  "
      << Report::tr("Tilt X/Y: %1")
             .arg(sel.tilt ? Report::tr("%1 / %2 nm").arg(fixed(c.tiltX, 2), fixed(c.tiltY, 2))
                           : notSubtracted())
      << "\n";
  out << "  " << Report::tr("Defocus: %1").arg(sel.defocus ? nm(c.defocus) : notSubtracted())
      << "\n\n";

  out << Report::tr("Surface aberrations:") << "\n";
  out << "  "
      << Report::tr("Astigmatism: %1")
             .arg(sel.astigmatism ? polar(data.astigMagnitude, data.astigAngleDeg, c.astigX, c.astigY)
                                  : notSubtracted())
      << "\n";
  out << "  "
      << Report::tr("Coma: %1")
             .arg(sel.coma ? polar(data.comaMagnitude, data.comaAngleDeg, c.comaX, c.comaY)
                           : notSubtracted())
      << "\n";
  out << "  "
      << Report::tr("Trefoil: %1")
             .arg(sel.trefoil
                      ? polar(data.trefoilMagnitude, data.trefoilAngleDeg, c.trefoilX, c.trefoilY)
                      : notSubtracted())
      << "\n";
  out << "  "
      << Report::tr("Spherical (3rd order): %1").arg(sel.spherical ? nm(c.spherical) : notSubtracted())
      << "\n\n";

  const double lam = data.wavelengthNm;
  auto waves = [lam](double valueNm) { return lam > 0.0 ? valueNm / lam : 0.0; };
  auto nmAndWaves = [&](double valueNm) {
    return Report::tr("%1 nm (%2 %3)").arg(fixed(valueNm, 2), fixed(waves(valueNm), 3), lambda);
  };
  out << Report::tr("RMS before: %1").arg(nmAndWaves(data.rmsBeforeNm)) << "\n";
  out << Report::tr("RMS after: %1").arg(nmAndWaves(data.rmsAfterNm)) << "\n";
  out << Report::tr("PV before: %1").arg(nmAndWaves(data.pvBeforeNm)) << "\n";
  out << Report::tr("PV after: %1").arg(nmAndWaves(data.pvAfterNm)) << "\n";
  out << Report::tr("Strehl (Marechal est.): %1").arg(fixed(data.strehl, 3)) << "\n";

  if (data.fitMethod == digitqt::core::ModalFitMethod::SequentialSeregin) {
    const auto &sq = data.sequential;
    auto line = [&](const QString &label, double valueNm) {
      return QStringLiteral("  %1: %2\n").arg(label, nmAndWaves(valueNm));
    };
    auto rmsLine = [&](const QString &label, double valueNm) {
      return QStringLiteral("%1: %2 %3\n").arg(label, fixed(waves(valueNm), 3), lambda);
    };

    out << "\n";
    out << Report::tr(
               "--- Raw sequential stages (exact DAPPSIM algorithm; byte-verified only for\n"
               "    piston/tiltX/tiltY/defocus against test_data/report.txt's D/Lx/Ly/C via\n"
               "    test_data/Terminal.pol -- everything below that is a faithful\n"
               "    transcription of Includes/Fitting.cpp, NOT independently verified\n"
               "    against report.txt's B0-B8/C(coma)/FIC labels: those are formatted by\n"
               "    WinFringe, whose source is unavailable) ---")
        << "\n";
    out << line(Report::tr("Piston (stage 1)"), sq.piston);
    out << line(Report::tr("Tilt X (stage 1)"), sq.tiltX);
    out << line(Report::tr("Tilt Y (stage 1)"), sq.tiltY);
    out << line(Report::tr("Defocus / \"C\" (stage 2, ρ²)"), sq.defocus);
    out << line(Report::tr("Astig (3X²-1)/2 (stage 3)"), sq.astig4);
    out << line(Report::tr("Astig XY (stage 3)"), sq.astig5);
    out << line(Report::tr("Astig (3Y²-1)/2 (stage 3)"), sq.astig6);
    out << line(Report::tr("Residual tilt X (stage 4)"), sq.comaResidualX);
    out << line(Report::tr("Residual tilt Y (stage 4)"), sq.comaResidualY);
    out << line(Report::tr("Coma X (stage 4)"), sq.comaX);
    out << line(Report::tr("Coma Y (stage 4)"), sq.comaY);
    out << line(Report::tr("Trefoil X (stage 4)"), sq.trefoilX);
    out << line(Report::tr("Trefoil Y (stage 4)"), sq.trefoilY);
    out << line(Report::tr("ρ² (stage 5, S3)"), sq.s3rho2);
    out << line(Report::tr("ρ⁴ (stage 5, S3)"), sq.s3rho4);
    out << line(Report::tr("ρ² (stage 6, S5)"), sq.s5rho2);
    out << line(Report::tr("ρ⁴ (stage 6, S5)"), sq.s5rho4);
    out << line(Report::tr("ρ⁶ (stage 6, S5)"), sq.s5rho6);
    out << line(Report::tr("ρ² (stage 7, S7)"), sq.s7rho2);
    out << line(Report::tr("ρ⁴ (stage 7, S7)"), sq.s7rho4);
    out << line(Report::tr("ρ⁶ (stage 7, S7)"), sq.s7rho6);
    out << line(Report::tr("ρ⁸ (stage 7, S7)"), sq.s7rho8);
    out << line(Report::tr("Discarded stage intercepts (2-7, kept in residual calc only)"),
                sq.discardedIntercepts);
    out << "\n";
    out << rmsLine(Report::tr("RMS(W) initial"), sq.rmsInitial);
    out << rmsLine(Report::tr("RMS after tilt+defocus"), sq.rmsAfterTiltDefocus);
    out << rmsLine(Report::tr("RMS after astig"), sq.rmsAfterAstig);
    out << rmsLine(Report::tr("RMS after coma+trefoil"), sq.rmsAfterComa);
    out << rmsLine(Report::tr("RMS after S3"), sq.rmsAfterS3);
    out << rmsLine(Report::tr("RMS after S5"), sq.rmsAfterS5);
    out << rmsLine(Report::tr("RMS after S7"), sq.rmsAfterS7);
  }
}

bool openReport(QFile &file, QTextStream &out, const QString &path, QString &errorMessage) {
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    errorMessage = Report::tr("Could not open %1 for writing").arg(path);
    return false;
  }
  out.setDevice(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  out.setEncoding(QStringConverter::Utf8);
#else
  out.setCodec("UTF-8");
#endif
  return true;
}

bool closeReport(QFile &file, QTextStream &out, QString &errorMessage) {
  out.flush();
  if (file.error() != QFile::NoError) {
    errorMessage = file.errorString();
    return false;
  }
  return true;
}

QString generatedLine() {
  return Report::tr("Generated: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}

void writeInputSection(QTextStream &out, const digitqt::core::Measurement &measurement) {
  const auto &psi = measurement.phaseShifting();
  out << Report::tr("=== Input ===") << "\n";
  out << Report::tr("File: %1").arg(QString::fromStdString(measurement.imagePath())) << "\n";
  if (measurement.hasImage())
    out << Report::tr("Canvas: %1 x %2 px (image with background margin, see padImageBackground)")
               .arg(measurement.image().width())
               .arg(measurement.image().height())
        << "\n";
  if (psi.hasSeries())
    out << Report::tr("Phase-shifting series: %1 frames, source frame %2 x %3 px")
               .arg(psi.frames().size())
               .arg(psi.sourceWidth())
               .arg(psi.sourceHeight())
        << "\n";

  // Координаты холста -- с полем; для серии PSI заодно в координатах
  // исходного кадра (поле поровну с каждой стороны).
  const double offsetX =
      psi.hasSeries() ? (measurement.image().width() - psi.sourceWidth()) / 2.0 : 0.0;
  const double offsetY =
      psi.hasSeries() ? (measurement.image().height() - psi.sourceHeight()) / 2.0 : 0.0;
  auto writeShapes = [&](const QString &kind, const auto &shapes) {
    for (const auto &shape : shapes) {
      const auto bounds = shape->getBounds();
      const double cx = (bounds.minX() + bounds.maxX()) / 2.0;
      const double cy = (bounds.minY() + bounds.maxY()) / 2.0;
      out << "  "
          << Report::tr("%1 %2: center (%3, %4), size %5 x %6 px")
                 .arg(kind, shapeName(shape->typeName()), fixed(cx, 1), fixed(cy, 1),
                      fixed(bounds.maxX() - bounds.minX(), 1),
                      fixed(bounds.maxY() - bounds.minY(), 1));
      if (psi.hasSeries())
        out << Report::tr("; in source frame: center (%1, %2)")
                   .arg(fixed(cx - offsetX, 1), fixed(cy - offsetY, 1));
      out << "\n";
    }
  };
  const auto &boundaries = measurement.boundaries();
  out << Report::tr("Aperture: %1 external, %2 internal shape(s)")
             .arg(boundaries.getExternal().size())
             .arg(boundaries.getInternal().size())
      << "\n";
  writeShapes(Report::tr("external"), boundaries.getExternal());
  writeShapes(Report::tr("internal"), boundaries.getInternal());
  out << "\n";
}

void writePhaseSection(QTextStream &out, const digitqt::core::Measurement &measurement) {
  out << Report::tr("=== S2: phase ===") << "\n";
  out << Report::tr("Method: %1").arg(phaseAlgorithmName(measurement)) << "\n";
  const auto &phase = measurement.phaseMap();
  if (phase.isEmpty()) {
    out << Report::tr("Not computed") << "\n\n";
    return;
  }
  double rms = 0.0, pv = 0.0;
  long long points = 0;
  mapStats(phase, rms, pv, points);
  out << Report::tr("Grid: %1 x %2, %3 valid points")
             .arg(phase.width())
             .arg(phase.height())
             .arg(points)
      << "\n";
  out << Report::tr("Phase map, fringe orders: RMS %1, PV %2").arg(fixed(rms, 4), fixed(pv, 4))
      << "\n";

  const auto &psi = measurement.phaseShifting();
  const auto &report = psi.report();
  const bool isPsi = measurement.customPhaseReconstructorPluginPath().empty() &&
                     measurement.phaseReconstructionAlgorithm() ==
                         digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting;
  if (isPsi && !report.shiftsDeg.empty()) {
    out << "\n"
        << Report::tr("PSI settings: nominal step %1 deg, refine shifts %2, compensate tilt %3")
               .arg(psi.nominalStepDeg())
               .arg(onOff(psi.refineShifts()), onOff(psi.compensateTilt()))
        << "\n";
    if (report.refined)
      out << Report::tr("Iterations: %1 (%2)")
                 .arg(report.iterations)
                 .arg(report.converged ? Report::tr("converged") : Report::tr("did NOT converge"))
          << "\n";
    out << Report::tr("Mirrored to the nominal direction: %1").arg(yesNo(report.mirrored)) << "\n";

    // Наклон -- в двух единицах: как в окне (волны на ширину и высоту
    // исходного кадра, y вниз по строкам) и как Lx/Ly отчётов DAPPSIM и
    // tiltX/tiltY на S5 (коэффициент при u, v от -1 до 1 по полукадру, y
    // вверх) -- второе совпадает с параметрами генераторов синтетики.
    // Столбцы -- по ширине заголовков, поэтому заголовки и числа
    // выравниваются по одним и тем же ширинам.
    const bool hasTilt = !report.tiltXWaves.empty();
    QStringList headers = {Report::tr("Frame"), Report::tr("Shift,deg"),
                           Report::tr("Shift,periods")};
    if (hasTilt)
      headers << Report::tr("TiltX,waves/frame") << Report::tr("TiltY,waves/frame(down)")
              << Report::tr("dLx,coef") << Report::tr("dLy,coef(up)");
    std::vector<int> widths;
    for (const QString &header : headers)
      widths.push_back(std::max(9, static_cast<int>(header.size())));
    out << "\n";
    for (int i = 0; i < headers.size(); ++i)
      out << (i > 0 ? "  " : "") << headers[i].rightJustified(widths[i]);
    out << "\n";
    for (size_t n = 0; n < report.shiftsDeg.size(); ++n) {
      QStringList cells = {QString::number(n + 1), fixed(report.shiftsDeg[n], 3),
                           fixed(report.shiftsDeg[n] / 360.0, 4)};
      if (hasTilt && n < report.tiltXWaves.size())
        cells << fixed(report.tiltXWaves[n], 4) << fixed(report.tiltYWaves[n], 4)
              << fixed(report.tiltXWaves[n] / 2.0, 4) << fixed(-report.tiltYWaves[n] / 2.0, 4);
      for (int i = 0; i < cells.size(); ++i)
        out << (i > 0 ? "  " : "") << cells[i].rightJustified(widths[i]);
      out << "\n";
    }
    if (hasTilt)
      out << Report::tr("Tilts are relative to frame 1.") << "\n";

    const QString halves = std::isnan(report.halvesDisagreementDeg)
                               ? Report::tr("n/a")
                               : fixed(report.halvesDisagreementDeg, 4);
    const double relative =
        report.amplitude > 0.0 ? 100.0 * report.residual / report.amplitude : 0.0;
    out << "\n" << Report::tr("Diagnostics:") << "\n";
    out << "  " << Report::tr("Points used for the shifts: %1").arg(report.shiftPoints) << "\n";
    out << "  " << Report::tr("Halves of the field disagree by: %1 deg").arg(halves) << "\n";
    out << "  "
        << Report::tr("Fit residual: %1 grey levels of fringe amplitude %2 (%3%)")
               .arg(fixed(report.residual, 3), fixed(report.amplitude, 2), fixed(relative, 2))
        << "\n";
    out << "  "
        << Report::tr("Unwrap regions: %1, masked low modulation %2, dropped islands %3 points")
               .arg(report.regionCount)
               .arg(report.maskedLowModulation)
               .arg(report.droppedIslandPoints)
        << "\n";
  }
  out << "\n";
}

/// Коэффициенты S5 в обозначениях отчёта DAPPSIM: полиномы со старшим
/// членом 1 (B2*(rho^2 - 1/2) + B4*(rho^4 - rho^2 + 1/6), кома
/// C*(rho^3 - 2rho/3)*cos(theta - FIC), астигматизм A*rho^2*cos 2(theta -
/// FIA)), Y вверх. Пересчёт из обоих базисов -- по формулам
/// buildSereginHierarchy()/buildZernikeHierarchy() (ModalAnalysisStage.cpp).
/// В порядках полос (волнах фронта), как в отчётах DAPPSIM и генераторах.
void writeDappsimSection(QTextStream &out, const digitqt::core::Measurement &measurement,
                         double perOrderNm) {
  const auto &c = measurement.modalAnalysis().coefficients;
  const bool zernike = measurement.modalAnalysis().basis == digitqt::core::PolynomialBasis::Zernike;
  auto orders = [perOrderNm](double nm) { return perOrderNm > 0.0 ? nm / perOrderNm : 0.0; };

  // Y = -y: в базисе Цернике y растёт вниз по строкам, у Серегина знак
  // уже перевёрнут в самих формулах.
  const double lx = orders(c.tiltX);
  const double ly = orders(zernike ? -c.tiltY : c.tiltY);
  const double b4 = orders(zernike ? 6.0 * c.spherical : c.spherical);
  const double b2 = orders(zernike ? 2.0 * c.defocus : c.defocus + c.spherical);
  const double astigCos = orders(zernike ? c.astigX : 1.5 * c.astigX);  // при X^2 - Y^2
  const double astigSin = orders(zernike ? -c.astigY : 0.5 * c.astigY);  // при 2XY
  const double comaX = orders(zernike ? 3.0 * c.comaX : c.comaX);  // при X(rho^2 - 2/3)
  const double comaY = orders(zernike ? -3.0 * c.comaY : c.comaY);  // при Y(rho^2 - 2/3)
  const double trefoilCos = orders(zernike ? c.trefoilX : -c.trefoilX);  // при X^3 - 3XY^2
  const double trefoilSin = orders(-c.trefoilY);                         // при 3X^2Y - Y^3

  const double degPerRad = 180.0 / std::acos(-1.0);
  auto angle = [](double deg, double period) {  // в [0, period)
    deg = std::fmod(deg, period);
    return deg < 0.0 ? deg + period : deg;
  };
  // Углы -- как в DAPPSIM, от оси +Y (против часовой стрелки): на
  // синтетике с FIA = FIC = 0 астигматизм и кома DigitQt вышли на 90° от +X.
  // Направление отсчёта (по или против часовой) этим примером не проверено.
  const double astig = std::hypot(astigCos, astigSin);
  const double astigDeg =
      angle(0.5 * std::atan2(astigSin, astigCos) * degPerRad - 90.0, 180.0);
  const double coma = std::hypot(comaX, comaY);
  const double comaDeg = angle(std::atan2(comaY, comaX) * degPerRad - 90.0, 360.0);
  const double trefoil = std::hypot(trefoilCos, trefoilSin);
  const double trefoilDeg =
      angle(std::atan2(trefoilSin, trefoilCos) / 3.0 * degPerRad - 90.0, 120.0);

  out << "\n" << Report::tr("DAPPSIM notation (fringe orders = waves of wavefront, Y up):") << "\n";
  out << "  " << Report::tr("Lx = %1   Ly = %2").arg(fixed(lx, 3), fixed(ly, 3)) << "\n";
  out << "  "
      << Report::tr("B2 = %1   B4 = %2   (B2*(rho^2-1/2) + B4*(rho^4-rho^2+1/6); textbook "
                    "Zernike Z20 = B2/2, Z40 = B4/6)")
             .arg(fixed(b2, 3), fixed(b4, 3))
      << "\n";
  out << "  " << Report::tr("A  = %1   FIA = %2 deg").arg(fixed(astig, 3), fixed(astigDeg, 1))
      << "\n";
  out << "  "
      << Report::tr("C  = %1   FIC = %2 deg   (C*(rho^3-2rho/3); textbook Zernike Z31 = C/3)")
             .arg(fixed(coma, 3), fixed(comaDeg, 1))
      << "\n";
  out << "  "
      << Report::tr("T  = %1   FIT = %2 deg   (T*rho^3*cos 3(theta-FIT))")
             .arg(fixed(trefoil, 3), fixed(trefoilDeg, 1))
      << "\n";
  out << "  "
      << Report::tr("Angles from +Y, counterclockwise, as in DAPPSIM (checked on one synthetic\n"
                    "  series with FIA = FIC = 0; the direction of rotation is not checked yet).")
      << "\n";
}

void writeWavefrontSection(QTextStream &out, const digitqt::core::Measurement &measurement) {
  out << Report::tr("=== S4: wavefront ===") << "\n";
  const auto &wavefront = measurement.wavefrontMap();
  if (wavefront.isEmpty()) {
    out << Report::tr("Not computed") << "\n\n";
    return;
  }
  const double lambdaNm = measurement.wavelengthNm();
  double rms = 0.0, pv = 0.0;
  long long points = 0;
  mapStats(wavefront, rms, pv, points);
  out << Report::tr("Wavelength: %1 nm, %2")
             .arg(lambdaNm)
             .arg(measurement.isDoublePass()
                      ? Report::tr("double pass (height = order * wavelength / 2)")
                      : Report::tr("single pass (height = order * wavelength)"))
      << "\n";
  out << Report::tr("RMS: %1 nm (%2 waves)")
             .arg(fixed(rms, 2), fixed(lambdaNm > 0.0 ? rms / lambdaNm : 0.0, 4))
      << "\n";
  out << Report::tr("PV: %1 nm (%2 waves)")
             .arg(fixed(pv, 2), fixed(lambdaNm > 0.0 ? pv / lambdaNm : 0.0, 4))
      << "\n\n";
}

}  // namespace

bool writeModalReport(const QString &path, const digitqt::core::Measurement &measurement,
                      QString &errorMessage) {
  const auto data = digitqt::core::buildModalAnalysisReportData(measurement);
  if (data.isEmpty) {
    errorMessage = Report::tr("No S5 (Polynomial / Modal Analysis) result yet");
    return false;
  }

  QFile file(path);
  QTextStream out;
  if (!openReport(file, out, path, errorMessage))
    return false;
  out << Report::tr("DigitQt -- Polynomial / Modal Analysis (S5) report") << "\n";
  out << generatedLine() << "\n";
  writeModalSection(out, data, measurement);
  return closeReport(file, out, errorMessage);
}

bool writeFullReport(const QString &path, const digitqt::core::Measurement &measurement,
                     QString &errorMessage) {
  QFile file(path);
  QTextStream out;
  if (!openReport(file, out, path, errorMessage))
    return false;

  out << Report::tr("DigitQt -- measurement report") << "\n";
  out << generatedLine() << "\n\n";
  writeInputSection(out, measurement);
  writePhaseSection(out, measurement);
  writeWavefrontSection(out, measurement);

  out << Report::tr("=== S5: polynomial / modal analysis ===") << "\n";
  const auto data = digitqt::core::buildModalAnalysisReportData(measurement);
  if (data.isEmpty) {
    out << Report::tr("Not computed") << "\n";
  } else {
    writeModalSection(out, data, measurement);
    // Коэффициенты в волнах и в порядках полос -- для сравнения с
    // параметрами генераторов синтетики и с отчётами DAPPSIM (Lx, Ly...).
    // Сырые коэффициенты МНК, без WinFringe-множителя x2 для комы,
    // трилистника и сферической, который применяет
    // buildModalAnalysisReportData() (см. там) к секции выше. Имена термов
    // -- поля ModalCoefficients, не переводятся.
    const double lambdaNm = data.wavelengthNm;
    const double perOrder = lambdaNm * (measurement.isDoublePass() ? 0.5 : 1.0);
    const auto &c = measurement.modalAnalysis().coefficients;
    const std::pair<const char *, double> terms[] = {
        {"piston", c.piston},     {"tiltX", c.tiltX},       {"tiltY", c.tiltY},
        {"defocus", c.defocus},   {"astigX", c.astigX},     {"astigY", c.astigY},
        {"comaX", c.comaX},       {"comaY", c.comaY},       {"trefoilX", c.trefoilX},
        {"trefoilY", c.trefoilY}, {"spherical", c.spherical}};
    out << "\n"
        << Report::tr("Raw fit coefficients (%1; no WinFringe x2 for coma/trefoil/spherical):")
               .arg(basisName(data.basis))
        << "\n";
    const QString headers[] = {Report::tr("Term"), Report::tr("nm"), Report::tr("waves"),
                               Report::tr("fringe orders")};
    const int widths[] = {10, 12, 13, 15};
    out << headers[0].leftJustified(widths[0]);
    for (int i = 1; i < 4; ++i)
      out << headers[i].rightJustified(widths[i]);
    out << "\n";
    for (const auto &[name, valueNm] : terms)
      out << QString::fromLatin1(name).leftJustified(widths[0])
          << fixed(valueNm, 3).rightJustified(widths[1])
          << fixed(lambdaNm > 0.0 ? valueNm / lambdaNm : 0.0, 5).rightJustified(widths[2])
          << fixed(perOrder > 0.0 ? valueNm / perOrder : 0.0, 5).rightJustified(widths[3])
          << "\n";
    writeDappsimSection(out, measurement, perOrder);
  }
  return closeReport(file, out, errorMessage);
}

}  // namespace digitqt::core::io
