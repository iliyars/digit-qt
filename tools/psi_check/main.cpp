// psi_check -- прогоняет серию кадров фазового сдвига (.idf) через
// НАСТОЯЩИЙ пайплайн приложения (S2 методом Phase Shifting -> S4 -> S5) без
// GUI и печатает то же, что программа показывает в строке состояния, плюс
// RMS волнового фронта до и после вычитания полиномов S5.
//
// Для синтетики из test_data/psi_synthetic/ (там в truth.txt -- что
// заложено и что должно получиться) и для серий со стенда: сравнить
// режимы (--no-refine, --tilt) на одних и тех же кадрах.
//
// Usage:
//   psi_check <series.idf> [--step=DEG] [--no-refine] [--tilt] [--pad]
//       [--center-x=X] [--center-y=Y] [--radius=R]
//       [--wavelength=NM] [--double-pass] [--zernike[=unit|iso|rms]] [--out-map=FILE]
//       [--out-report=FILE] [--qm=FILE]
//
// --zernike -- базис Цернике (по умолчанию Seregin); после "=" -- нормировка
// коэффициентов в отчёте (unit по умолчанию, iso -- ISO 14999, rms -- ANSI).
// Строка "S5 coefficients" всегда в единичной нормировке.
//
// --qm загружает перевод (например, build/src/translations/DigitQt_ru.qm) --
// тогда --out-report пишется на этом языке.
//
// --out-map пишет карту S2 попиксельно (текст) -- для сверки с заложенной
// фазой в test_data/psi_synthetic/psi_demo.py. --out-report -- тот же
// полный отчёт, что File -> Export Report в программе (io::writeFullReport).
//
// В отличие от GUI кадры не дополняются полем (padImageBackground), если
// не задан --pad (тогда -- ровно как в GUI); координаты апертуры в обоих
// случаях в пикселях исходных кадров. По умолчанию
// апертура -- вписанный круг с отступом 1px; для синтетики
// (апертура 0.45 от кадра) -- --radius=230. Без двойного хода (как в
// CLAUDE.md при сверке со статьёй): RMS в волнах = RMS фронта / λ.

#include "core/ImagePadding.h"
#include "core/Measurement.h"
#include "core/PhaseReconstructionAlgorithm.h"
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/PipelineStageId.h"
#include "formats/IdfFile.h"
#include "io/ImageLoader.h"
#include "io/ModalReportExporter.h"

#include <aperture/include/geometry/Ellipse.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTranslator>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

QString joined(const std::vector<double> &values, int decimals) {
  QStringList parts;
  for (const double value : values)
    parts << QString::number(value, 'f', decimals);
  return parts.join(QStringLiteral(" / "));
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const QStringList args = QCoreApplication::arguments();

  QStringList positional;
  QString mapPath, reportPath, qmPath;
  double stepDeg = 90.0;
  bool refine = true, tilt = false, doublePass = false, zernike = false, pad = false;
  auto normalization = digitqt::core::ZernikeNormalization::Unit;
  double wavelengthNm = 632.8;
  double centerX = -1, centerY = -1, radius = -1;
  for (int i = 1; i < args.size(); ++i) {
    const QString &a = args[i];
    if (a.startsWith(QStringLiteral("--step=")))
      stepDeg = a.mid(QStringLiteral("--step=").size()).toDouble();
    else if (a == QStringLiteral("--no-refine"))
      refine = false;
    else if (a == QStringLiteral("--tilt"))
      tilt = true;
    else if (a == QStringLiteral("--pad"))
      pad = true;
    else if (a == QStringLiteral("--double-pass"))
      doublePass = true;
    else if (a == QStringLiteral("--zernike"))
      zernike = true;
    else if (a.startsWith(QStringLiteral("--zernike="))) {
      zernike = true;
      const QString name = a.mid(QStringLiteral("--zernike=").size());
      if (name == QStringLiteral("iso"))
        normalization = digitqt::core::ZernikeNormalization::Iso14999;
      else if (name == QStringLiteral("rms"))
        normalization = digitqt::core::ZernikeNormalization::Rms;
      else if (name != QStringLiteral("unit"))
        positional << a;  // неизвестная нормировка -- покажем usage
    }
    else if (a.startsWith(QStringLiteral("--wavelength=")))
      wavelengthNm = a.mid(QStringLiteral("--wavelength=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--center-x=")))
      centerX = a.mid(QStringLiteral("--center-x=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--center-y=")))
      centerY = a.mid(QStringLiteral("--center-y=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--radius=")))
      radius = a.mid(QStringLiteral("--radius=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--out-map=")))
      mapPath = a.mid(QStringLiteral("--out-map=").size());
    else if (a.startsWith(QStringLiteral("--out-report=")))
      reportPath = a.mid(QStringLiteral("--out-report=").size());
    else if (a.startsWith(QStringLiteral("--qm=")))
      qmPath = a.mid(QStringLiteral("--qm=").size());
    else
      positional << a;
  }

  QTextStream out(stdout);
  QTextStream err(stderr);

  // Перевод -- только для --out-report (консольный вывод остаётся английским).
  QTranslator translator;
  if (!qmPath.isEmpty()) {
    if (!translator.load(qmPath)) {
      err << "Cannot load translation " << qmPath << "\n";
      return 1;
    }
    QCoreApplication::installTranslator(&translator);
  }

  if (positional.size() != 1) {
    err << "Usage: psi_check <series.idf> [--step=DEG] [--no-refine] [--tilt] [--pad] "
           "[--center-x=X] [--center-y=Y] [--radius=R] [--wavelength=NM] [--double-pass] "
           "[--zernike[=unit|iso|rms]] [--out-map=FILE] [--out-report=FILE] [--qm=FILE]\n";
    return 1;
  }

  const QString idfPath = positional[0];
  QFile file(idfPath);
  if (!file.open(QIODevice::ReadOnly)) {
    err << "Cannot open " << idfPath << "\n";
    return 1;
  }
  const QByteArray bytes = file.readAll();
  digitqt::formats::IdfDescription idf;
  std::string error;
  if (!digitqt::formats::parseIdf(std::string_view(bytes.constData(), bytes.size()), idf,
                                  error)) {
    err << idfPath << ": " << QString::fromStdString(error) << "\n";
    return 1;
  }

  const QDir dir = QFileInfo(idfPath).absoluteDir();
  std::vector<digitqt::core::Bitmap> frames;
  for (const std::string &name : idf.frameFiles) {
    const QString path = dir.filePath(QString::fromLocal8Bit(name.c_str()));
    const auto result = digitqt::io::loadImage(path);
    if (!result.ok()) {
      err << "Failed to load " << path << ": " << result.errorMessage << "\n";
      return 1;
    }
    frames.push_back(result.image);
  }
  const int width = frames.front().width(), height = frames.front().height();
  // Поле -- как в MainWindow::openPhaseShiftingSeries(); апертура сдвигается
  // на его ширину.
  const int margin = pad ? static_cast<int>(std::lround(0.15 * std::max(width, height))) : 0;
  if (pad)
    for (auto &frame : frames)
      frame = digitqt::core::padImageBackground(frame);

  digitqt::core::Measurement measurement;
  measurement.setPhaseShiftingFrames(std::move(frames), idfPath.toStdString(), width, height);
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);
  measurement.setWavelengthNm(wavelengthNm);
  measurement.setDoublePass(doublePass);
  if (zernike)
    measurement.polynomialBasis() = digitqt::core::PolynomialBasis::Zernike;
  measurement.zernikeNormalization() = normalization;
  auto &psi = measurement.phaseShifting();
  psi.setNominalStepDeg(stepDeg);
  psi.setRefineShifts(refine);
  psi.setCompensateTilt(tilt);

  if (centerX < 0)
    centerX = width / 2.0 - 0.5;
  if (centerY < 0)
    centerY = height / 2.0 - 0.5;
  if (radius < 0)
    radius = std::min(width, height) / 2.0 - 1.0;
  measurement.boundaries().addExternal(
      std::make_unique<aperture::Ellipse>(radius, radius, centerX + margin, centerY + margin));

  out << "Series: " << idf.frameFiles.size() << " frames " << width << "x" << height
      << ", aperture center=(" << centerX << ", " << centerY << ") radius=" << radius << "\n"
      << "Mode: nominal step " << stepDeg << " deg, "
      << (!refine ? "shifts NOT refined" : tilt ? "refined, tilt compensated" : "refined")
      << "\n";

  digitqt::core::pipeline::Pipeline pipeline;
  using digitqt::core::pipeline::StageId;
  for (const StageId stage : {StageId::Setup, StageId::S2, StageId::S4, StageId::S5}) {
    if (!pipeline.stage(stage).compute(measurement)) {
      err << "Stage failed: " << QString::fromStdString(pipeline.stage(stage).errorMessage())
          << "\n";
      return 1;
    }
  }

  const auto &report = psi.report();
  out << "Shifts, deg:        " << joined(report.shiftsDeg, 2) << "\n";
  if (report.refined)
    out << "Iterations:         " << report.iterations
        << (report.converged ? " (converged)" : " (NOT converged)") << "\n";
  if (!report.tiltXWaves.empty()) {
    out << "Tilt x, waves:      " << joined(report.tiltXWaves, 3) << "\n";
    out << "Tilt y, waves:      " << joined(report.tiltYWaves, 3) << "\n";
  }
  out << "Mirrored:           " << (report.mirrored ? "YES -- check the sign of the map" : "no")
      << "\n";
  out << "Halves disagree:    " << report.halvesDisagreementDeg << " deg\n";
  out << "Fit residual:       " << report.residual << " of amplitude " << report.amplitude
      << " grey levels (" << report.shiftPoints << " points)\n";
  out << "Unwrap regions:     " << report.regionCount << ", masked " << report.maskedLowModulation
      << " + islands " << report.droppedIslandPoints << " points\n";

  const auto &modal = measurement.modalAnalysis();
  out << "Wavefront RMS:      " << modal.rmsBefore / wavelengthNm << " waves\n";
  out << "S5 residual RMS:    " << modal.rmsAfter / wavelengthNm << " waves\n";
  // Коэффициенты S5 в волнах (базис -- Measurement::polynomialBasis(), по
  // умолчанию Seregin; формулы -- buildSereginHierarchy() в
  // ModalAnalysisStage.cpp). Одной строкой name=value -- для psi_demo.py.
  const auto &c = modal.coefficients;
  const std::pair<const char *, double> terms[] = {
      {"tiltX", c.tiltX},       {"tiltY", c.tiltY},         {"defocus", c.defocus},
      {"astigX", c.astigX},     {"astigY", c.astigY},       {"comaX", c.comaX},
      {"comaY", c.comaY},       {"trefoilX", c.trefoilX},   {"trefoilY", c.trefoilY},
      {"spherical", c.spherical}};
  out << "S5 coefficients:   ";
  for (const auto &[name, value] : terms)
    out << " " << name << "=" << QString::number(value / wavelengthNm, 'f', 5);
  out << " waves\n";

  // Карта S2 (порядки полосы) попиксельно: "ширина высота", затем строки
  // значений, nan -- нет данных. Для сравнения с заложенной фазой, см.
  // test_data/psi_synthetic/psi_demo.py.
  if (!mapPath.isEmpty()) {
    QFile mapFile(mapPath);
    if (!mapFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      err << "Cannot write " << mapPath << "\n";
      return 1;
    }
    QTextStream map(&mapFile);
    const auto &orders = measurement.phaseMap();
    map << orders.width() << " " << orders.height() << "\n";
    for (int y = 0; y < orders.height(); ++y) {
      for (int x = 0; x < orders.width(); ++x) {
        if (x > 0)
          map << " ";
        if (orders.hasValue(x, y))
          map << QString::number(orders.value(x, y), 'g', 9);
        else
          map << "nan";
      }
      map << "\n";
    }
  }

  if (!reportPath.isEmpty()) {
    QString reportError;
    if (!digitqt::core::io::writeFullReport(reportPath, measurement, reportError)) {
      err << "Cannot write report: " << reportError << "\n";
      return 1;
    }
  }
  return 0;
}
