#include "core/Measurement.h"
#include "core/PhaseReconstructionAlgorithm.h"
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/PipelineStageId.h"
#include "formats/FrnAdapter.h"

#include <QtTest/QtTest>

namespace {

using digitqt::core::Measurement;
using digitqt::core::PhaseReconstructionAlgorithm;
using digitqt::core::pipeline::Pipeline;
using digitqt::core::pipeline::StageId;

/// То же, что делает MainWindow::applyImportedFringes(), когда картинки
/// рядом с .frn нет: холст-заглушка размера из файла + апертура и полосы.
void loadFrnWithoutImage(Measurement &measurement) {
  QFile file(QStringLiteral(DIGITQT_TEST_DATA_DIR "/Digit_frn.frn"));
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray bytes = file.readAll();

  digitqt::formats::ImportedData data;
  std::string error;
  QVERIFY2(digitqt::formats::FrnAdapter().importData(
               std::string_view(bytes.constData(), bytes.size()), data, error),
           error.c_str());

  measurement.setPlaceholderImage(data.imageWidth, data.imageHeight);
  measurement.boundaries() = std::move(*data.boundaries);
  measurement.fringeTracing().tracedLines() = std::move(data.fringeLines);
}

class ImportedFringesPipelineTest : public QObject {
  Q_OBJECT

private slots:
  void tracingRefusesAndKeepsImportedLines();
  void imageBasedPhaseMethodRefuses();
  void horizontalSplineReconstructsPhase();
  void openingAnImageClearsThePlaceholder();
};

void ImportedFringesPipelineTest::tracingRefusesAndKeepsImportedLines() {
  // С обоими видами метода фазы: линейный запустил бы трассировку по
  // пустому холсту, image-based -- молча очистил бы линии.
  for (const auto algorithm :
       {PhaseReconstructionAlgorithm::HorizontalSpline, PhaseReconstructionAlgorithm::FourierTransform}) {
    Measurement measurement;
    loadFrnWithoutImage(measurement);
    measurement.setPhaseReconstructionAlgorithm(algorithm);

    Pipeline pipeline;
    QVERIFY(!pipeline.stage(StageId::Setup).compute(measurement));
    QVERIFY(pipeline.stage(StageId::Setup).errorMessage().find("imported") != std::string::npos);
    QCOMPARE(measurement.fringeTracing().tracedLines().size(), size_t(57));
  }
}

void ImportedFringesPipelineTest::imageBasedPhaseMethodRefuses() {
  Measurement measurement;
  loadFrnWithoutImage(measurement);
  measurement.setPhaseReconstructionAlgorithm(PhaseReconstructionAlgorithm::FourierTransform);

  Pipeline pipeline;
  QVERIFY(!pipeline.stage(StageId::S2).compute(measurement));
  QVERIFY(pipeline.stage(StageId::S2).errorMessage().find("Horizontal Spline") != std::string::npos);
  QVERIFY(measurement.phaseMap().isEmpty());
}

void ImportedFringesPipelineTest::horizontalSplineReconstructsPhase() {
  Measurement measurement;
  loadFrnWithoutImage(measurement);
  measurement.setPhaseReconstructionAlgorithm(PhaseReconstructionAlgorithm::HorizontalSpline);

  Pipeline pipeline;
  QVERIFY2(pipeline.stage(StageId::S2).compute(measurement),
           pipeline.stage(StageId::S2).errorMessage().c_str());

  // 57 полос с номерами 0..56 поперёк апертуры: фаза (в порядках полосы)
  // должна покрыть почти весь этот диапазон и не выходить далеко за него.
  const auto &phase = measurement.phaseMap();
  QVERIFY(!phase.isEmpty());
  int filled = 0;
  double minValue = 1e300, maxValue = -1e300;
  for (int y = 0; y < phase.height(); ++y) {
    for (int x = 0; x < phase.width(); ++x) {
      if (!phase.hasValue(x, y))
        continue;
      ++filled;
      minValue = std::min(minValue, phase.value(x, y));
      maxValue = std::max(maxValue, phase.value(x, y));
    }
  }
  qInfo("phase map %dx%d, %d filled cells, range %.3f..%.3f", phase.width(), phase.height(), filled,
        minValue, maxValue);
  QVERIFY(filled > phase.width() * phase.height() / 2);
  QVERIFY(maxValue - minValue > 50.0);
  QVERIFY(minValue > -5.0 && maxValue < 61.0);
}

void ImportedFringesPipelineTest::openingAnImageClearsThePlaceholder() {
  Measurement measurement;
  loadFrnWithoutImage(measurement);
  QVERIFY(measurement.imageIsPlaceholder());

  digitqt::core::Bitmap image(16, 16);
  image.fill(100);
  measurement.setImage(std::move(image), "real.bmp");
  QVERIFY(!measurement.imageIsPlaceholder());
}

}  // namespace

QTEST_APPLESS_MAIN(ImportedFringesPipelineTest)
#include "ImportedFringesPipelineTest.moc"
