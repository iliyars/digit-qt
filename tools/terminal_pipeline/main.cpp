// terminal_pipeline -- прогоняет реальную .bmp-интерферограмму через
// НАСТОЯЩИЙ пайплайн приложения (Setup/S1 трассировка -> S2 восстановление
// фазы -> опционально экспорт) без GUI, чтобы получить .mtr для сравнения
// с эталоном (например Terminal_digit.mtr от самого Digit) БЕЗ ручной
// перерисовки границы апертуры мышью каждый раз, когда меняется код S1/S2.
//
// Апертура здесь -- один круг (aperture::Ellipse с равными полуосями),
// т.к. Terminal.bmp -- обычная круглая апертура на весь кадр. Если объект
// другой формы, --center-x/-y и --radius достаточно, чтобы это подстроить.
//
// Usage:
//   terminal_pipeline <input.bmp> <output.mtr>
//       [--center-x=X] [--center-y=Y] [--radius=R]
//       [--wavelength=NM] [--extend] [--minmax]
//       [--out-map=FILE] [--out-report=FILE]
// --out-map: full-resolution phase cycles, same text format as psi_check.
// --out-report: S4/S5 report, default reflection conversion (lambda/2).
//
// По умолчанию центр/радиус берутся из размера изображения (вписанный
// круг с отступом 1px), wavelength=632.8, без продления полос до края
// (--extend включает те же extrapolateFringesHorizontally/Vertically,
// что и кнопки "Extend" в GUI, вызванные ровно так же, как их вызывает
// FringeTracingController).

#include "core/FringeEdgeExtrapolation.h"
#include "core/AutoSeedPlacement.h"
#include "core/Measurement.h"
#include "core/PhaseReconstructionAlgorithm.h"
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/PipelineStageId.h"
#include "io/ImageLoader.h"
#include "io/MtrExporter.h"
#include "io/ModalReportExporter.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/VisibilityChecker.h>

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <memory>

namespace {

size_t totalPointCount(const std::vector<digitqt::core::NumberedFringeLine> &lines) {
  size_t total = 0;
  for (const auto &line : lines)
    total += line.points.size();
  return total;
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const QStringList args = QCoreApplication::arguments();

  QStringList positional;
  double wavelengthNm = 632.8;
  double centerX = -1, centerY = -1, radius = -1;
  bool extend = false;
  bool minmax = false;
  bool sequential = false;
  QString mapPath, reportPath;
  for (int i = 1; i < args.size(); ++i) {
    const QString &a = args[i];
    if (a.startsWith(QStringLiteral("--out-map=")))
      mapPath = a.mid(10);
    else if (a.startsWith(QStringLiteral("--out-report=")))
      reportPath = a.mid(13);
    else if (a == QStringLiteral("--sequential"))
      sequential = true;
    else if (a == QStringLiteral("--minmax"))
      minmax = true;
    else if (a.startsWith(QStringLiteral("--wavelength=")))
      wavelengthNm = a.mid(QStringLiteral("--wavelength=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--center-x=")))
      centerX = a.mid(QStringLiteral("--center-x=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--center-y=")))
      centerY = a.mid(QStringLiteral("--center-y=").size()).toDouble();
    else if (a.startsWith(QStringLiteral("--radius=")))
      radius = a.mid(QStringLiteral("--radius=").size()).toDouble();
    else if (a == QStringLiteral("--extend"))
      extend = true;
    else
      positional << a;
  }

  QTextStream out(stdout);
  QTextStream err(stderr);

  if (positional.size() != 2) {
    err << "Usage: terminal_pipeline <input.bmp> <output.mtr> [--center-x=X] "
          "[--center-y=Y] [--radius=R] [--wavelength=NM] [--extend] [--minmax] "
          "[--out-map=FILE] [--out-report=FILE]\n";
    return 1;
  }

  const QString inputPath = positional[0];
  const QString outputPath = positional[1];

  auto loadResult = digitqt::io::loadImage(inputPath);
  if (!loadResult.ok()) {
    err << "Failed to load " << inputPath << ": " << loadResult.errorMessage << "\n";
    return 1;
  }

  digitqt::core::Measurement measurement;
  measurement.setImage(loadResult.image, inputPath.toStdString());
  measurement.setWavelengthNm(wavelengthNm);

  if (centerX < 0)
    centerX = loadResult.image.width() / 2.0 - 0.5;
  if (centerY < 0)
    centerY = loadResult.image.height() / 2.0 - 0.5;
  if (radius < 0)
    radius = std::min(loadResult.image.width(), loadResult.image.height()) / 2.0 - 1.0;

  measurement.boundaries().addExternal(
      std::make_unique<aperture::Ellipse>(radius, radius, centerX, centerY));

  measurement.fringeTracing().setAlgorithm(sequential ? digitqt::core::TracerAlgorithm::SequentialTracking
                                                     : digitqt::core::TracerAlgorithm::ScanlineExtremum);
  if (sequential) {
    aperture::VisibilityChecker checker(measurement.boundaries());
    auto visible = [&checker](int x, int y) {
      return checker.isVisible(aperture::Point{double(x), double(y)});
    };
    measurement.fringeTracing().seeds() = digitqt::core::findRowSeeds(measurement.image(), visible);
    out << "Seeds: " << measurement.fringeTracing().seeds().size() << "\n";
  }
  // Adjacent extrema in MinMax are half a phase cycle apart.
  measurement.fringeTracing().setFringeCenterMode(
      minmax ? digitqt::core::FringeCenterMode::MinMax : digitqt::core::FringeCenterMode::Max);

  out << "Image: " << loadResult.image.width() << "x" << loadResult.image.height()
      << ", aperture center=(" << centerX << ", " << centerY << ") radius=" << radius << "\n";

  digitqt::core::pipeline::Pipeline pipeline;
  using digitqt::core::pipeline::StageId;

  if (!pipeline.stage(StageId::Setup).compute(measurement)) {
    err << "Setup (tracing) failed: "
        << QString::fromStdString(pipeline.stage(StageId::Setup).errorMessage()) << "\n";
    return 1;
  }
  out << "Traced lines: " << measurement.fringeTracing().tracedLines().size()
      << ", points: " << totalPointCount(measurement.fringeTracing().tracedLines()) << "\n";

  if (extend) {
    aperture::VisibilityChecker checker(measurement.boundaries());
    auto isVisible = [&checker](double x, double y) {
      return checker.isVisible(aperture::Point{x, y});
    };

    // Horizontal: одна линия за вызов на каждую сторону (см.
    // extrapolateFringesHorizontally()) -- повторяем, пока растёт, той же
    // проверкой остановки, что использует FringeTracingController.
    for (int i = 0; i < 64; ++i) {
      auto &lines = measurement.fringeTracing().tracedLines();
      const size_t before = totalPointCount(lines);
      auto after = digitqt::core::extrapolateFringesHorizontally(lines, isVisible);
      if (totalPointCount(after) == before)
        break;
      lines = std::move(after);
    }

    // Vertical: один вызов дотягивает оба конца каждой линии до края
    // (с запасом overshootMargin=10), как и кнопка "Extend Vertically".
    {
      auto &lines = measurement.fringeTracing().tracedLines();
      const auto roi = checker.getVisibleRegion();
      const int cap = static_cast<int>(std::max(roi.width(), roi.height())) + 16;
      lines = digitqt::core::extrapolateFringesVertically(lines, isVisible, cap, 10);
    }

    out << "After extension: lines=" << measurement.fringeTracing().tracedLines().size()
        << ", points=" << totalPointCount(measurement.fringeTracing().tracedLines()) << "\n";
  }

  if (!pipeline.stage(StageId::S2).compute(measurement)) {
    err << "Phase reconstruction (S2) failed: "
        << QString::fromStdString(pipeline.stage(StageId::S2).errorMessage()) << "\n";
    return 1;
  }

  // WinFringe .mtr хранит волны (Units=WAV) -- измерение фазы уже в этих
  // единицах (см. WavefrontReconstructionStage.cpp: wavefrontMap() = phaseMap()
  // * heightPerOrder, т.е. это phaseMap(), а не wavefrontMap(), нужно
  // экспортировать -- ровно то же самое, что делает MainWindow::exportMtrAction).
  QString writeErr;
  if (!digitqt::core::io::writeMtrFile(outputPath, measurement.phaseMap(), writeErr)) {
    err << "Export failed: " << writeErr << "\n";
    return 1;
  }

  out << "Wrote " << outputPath << "\n";
  if (!mapPath.isEmpty()) {
    QFile file(mapPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream stream(&file);
    const auto &map = measurement.phaseMap();
    stream << map.width() << " " << map.height() << "\n";
    for (int y = 0; y < map.height(); ++y) {
      for (int x = 0; x < map.width(); ++x) {
        if (x) stream << " ";
        stream << (map.hasValue(x, y) ? QString::number(map.value(x, y), 'g', 9)
                                     : QStringLiteral("nan"));
      }
      stream << "\n";
    }
  }
  if (!reportPath.isEmpty()) {
    for (auto id : {StageId::S4, StageId::S5})
      if (!pipeline.stage(id).compute(measurement)) {
        err << QString::fromStdString(pipeline.stage(id).errorMessage()) << "\n";
        return 1;
      }
    if (!digitqt::core::io::writeFullReport(reportPath, measurement, writeErr)) {
      err << writeErr << "\n";
      return 1;
    }
  }
  return 0;
}
