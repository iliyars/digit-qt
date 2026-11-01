#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/commands/AddSeedCommand.h"
#include "core/commands/ReplaceShapeCommand.h"
#include "core/commands/SetFringeOrderCommand.h"
#include "core/pipeline/Pipeline.h"
#include "io/ModalReportExporter.h"

#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest/QtTest>
#include <aperture/include/geometry/Ellipse.h>
#include <cmath>

using namespace digitqt::core;
using namespace digitqt::core::pipeline;

namespace {
void markComputed(Pipeline &pipeline) {
  for (const auto id : kCanonicalOrder)
    pipeline.stage(id).markComputed();
}

void importPhase(Measurement &m, Pipeline &p) {
  PhaseMap phase(17, 17);
  for (int y = 0; y < 17; ++y)
    for (int x = 0; x < 17; ++x)
      phase.setValue(x, y, (x * x + y * y) / 1000.0);
  m.setImportedPhaseMap(std::move(phase));
  p.synchronizeInputs(m);
  p.stage(StageId::S2).markComputed();
}
}  // namespace

class PipelineValidityTest : public QObject {
  Q_OBJECT
private slots:
  void parameterEdits_data();
  void parameterEdits();
  void displayChangesKeepResultsCurrent();
  void undoRedoInvalidatesResults();
  void replacingSourceResetsStatuses();
  void recomputeAndFailureInvalidateDownstream();
  void reportUsesComputedParameters();
  void phaseReportUsesComputedParameters();
};

void PipelineValidityTest::parameterEdits_data() {
  QTest::addColumn<QString>("edit");
  QTest::addColumn<int>("firstStale");
  for (const auto *name : {"aperture", "seed", "tracer", "center"})
    QTest::newRow(name) << QString::fromLatin1(name) << 0;
  for (const auto *name :
       {"lineOrder", "linePoint", "algorithm", "plugin", "step", "refine", "tilt"})
    QTest::newRow(name) << QString::fromLatin1(name) << 1;
  for (const auto *name : {"wavelength", "doublePass"})
    QTest::newRow(name) << QString::fromLatin1(name) << 2;
  for (const auto *name : {"fitMethod", "basis", "terms", "erosion"})
    QTest::newRow(name) << QString::fromLatin1(name) << 4;
}

void PipelineValidityTest::parameterEdits() {
  QFETCH(QString, edit);
  QFETCH(int, firstStale);
  Measurement m;
  NumberedFringeLine line;
  line.points.push_back({1, 2, 0, 0});
  m.fringeTracing().tracedLines().push_back(line);
  Pipeline p;
  p.synchronizeInputs(m);
  markComputed(p);
  if (edit == "aperture")
    m.boundaries().addExternal(std::make_unique<aperture::Ellipse>(5, 5, 8, 8));
  else if (edit == "seed")
    m.fringeTracing().seeds().push_back({1, 2});
  else if (edit == "tracer")
    m.fringeTracing().setAlgorithm(TracerAlgorithm::BinaryThinning);
  else if (edit == "center")
    m.fringeTracing().setFringeCenterMode(FringeCenterMode::Max);
  else if (edit == "lineOrder")
    m.fringeTracing().tracedLines()[0].order = 3;
  else if (edit == "linePoint")
    m.fringeTracing().tracedLines()[0].points[0].x = 3;
  else if (edit == "algorithm")
    m.setPhaseReconstructionAlgorithm(PhaseReconstructionAlgorithm::PhaseShifting);
  else if (edit == "plugin")
    m.setCustomPhaseReconstructorPluginPath("other.dll");
  else if (edit == "step")
    m.phaseShifting().setNominalStepDeg(75);
  else if (edit == "refine")
    m.phaseShifting().setRefineShifts(!m.phaseShifting().refineShifts());
  else if (edit == "tilt")
    m.phaseShifting().setCompensateTilt(!m.phaseShifting().compensateTilt());
  else if (edit == "wavelength")
    m.setWavelengthNm(10600);
  else if (edit == "doublePass")
    m.setDoublePass(!m.isDoublePass());
  else if (edit == "fitMethod")
    m.modalFitMethod() = ModalFitMethod::SequentialSeregin;
  else if (edit == "basis")
    m.polynomialBasis() = PolynomialBasis::Zernike;
  else if (edit == "terms")
    m.modalTermSelection().coma = !m.modalTermSelection().coma;
  else if (edit == "erosion")
    m.setEdgeErosionPixels(5);
  p.synchronizeInputs(m);
  for (size_t i = 0; i < kCanonicalOrder.size(); ++i)
    QCOMPARE(p.stage(kCanonicalOrder[i]).status(),
             static_cast<int>(i) >= firstStale ? StageStatus::Stale : StageStatus::Computed);
  QVERIFY(p.hasOutdatedResults());
}

void PipelineValidityTest::displayChangesKeepResultsCurrent() {
  Measurement m;
  Pipeline p;
  p.synchronizeInputs(m);
  markComputed(p);
  m.zernikeNormalization() = ZernikeNormalization::Rms;
  m.setWavelengthNm(m.wavelengthNm());
  p.synchronizeInputs(m);
  QVERIFY(!p.hasOutdatedResults());
}

void PipelineValidityTest::undoRedoInvalidatesResults() {
  Measurement m;
  m.fringeTracing().tracedLines().push_back(NumberedFringeLine{});
  Pipeline p;
  p.synchronizeInputs(m);
  QUndoStack undo;
  undo.push(new digitqt::commands::SetFringeOrderCommand(m, 0, 4));
  p.synchronizeInputs(m);
  markComputed(p);
  undo.undo();
  p.synchronizeInputs(m);
  QCOMPARE(p.stage(StageId::Setup).status(), StageStatus::Computed);
  QCOMPARE(p.stage(StageId::S2).status(), StageStatus::Stale);
  markComputed(p);
  undo.redo();
  p.synchronizeInputs(m);
  QCOMPARE(p.stage(StageId::S2).status(), StageStatus::Stale);
  undo.push(new digitqt::commands::AddSeedCommand(m, {2, 3}));
  p.synchronizeInputs(m);
  markComputed(p);
  undo.undo();
  p.synchronizeInputs(m);
  QCOMPARE(p.stage(StageId::Setup).status(), StageStatus::Stale);

  m.boundaries().addExternal(std::make_unique<aperture::Ellipse>(5, 5, 8, 8));
  auto before = m.boundaries().getExternal()[0]->clone();
  auto after = before->clone();
  after->shiftX(1);
  undo.push(new digitqt::commands::ReplaceShapeCommand(
      m.boundaries(), aperture::TypeLimits::EXTERNAL, 0, std::move(before), std::move(after)));
  p.synchronizeInputs(m);
  markComputed(p);
  undo.undo();
  p.synchronizeInputs(m);
  QCOMPARE(p.stage(StageId::Setup).status(), StageStatus::Stale);
}

void PipelineValidityTest::replacingSourceResetsStatuses() {
  Measurement m;
  Pipeline p;
  m.setImage(Bitmap(5, 5), "same.bmp");
  p.synchronizeInputs(m);
  markComputed(p);
  m.setImage(Bitmap(5, 5), "same.bmp");
  p.synchronizeInputs(m);
  for (auto id : kCanonicalOrder)
    QCOMPARE(p.stage(id).status(), StageStatus::NotComputed);
  markComputed(p);
  importPhase(m, p);
  QCOMPARE(p.stage(StageId::Setup).status(), StageStatus::NotComputed);
  QCOMPARE(p.stage(StageId::S2).status(), StageStatus::Computed);
  QCOMPARE(p.stage(StageId::S5).status(), StageStatus::NotComputed);
}

void PipelineValidityTest::recomputeAndFailureInvalidateDownstream() {
  Measurement m;
  Pipeline p;
  importPhase(m, p);
  QVERIFY(p.compute(StageId::S4, m));
  QVERIFY(p.compute(StageId::S5, m));
  QVERIFY(p.compute(StageId::S4, m));
  QCOMPARE(p.stage(StageId::S5).status(), StageStatus::Stale);
  QVERIFY(p.compute(StageId::S5, m));
  p.invalidateFrom(StageId::S2);
  QVERIFY(!p.compute(StageId::S4, m));
  QCOMPARE(p.stage(StageId::S4).status(), StageStatus::Error);
  QVERIFY(!p.compute(StageId::S5, m));
  p.stage(StageId::S2).markComputed();
  QVERIFY(p.compute(StageId::S4, m));
  m.setWavelengthNm(0);
  QVERIFY(!p.compute(StageId::S4, m));
  QVERIFY(!p.compute(StageId::S5, m));
}

void PipelineValidityTest::reportUsesComputedParameters() {
  Measurement m;
  Pipeline p;
  importPhase(m, p);
  m.setWavelengthNm(1000);
  m.setDoublePass(false);
  QVERIFY(p.compute(StageId::S4, m));
  QVERIFY(p.compute(StageId::S5, m));
  const auto before = buildModalAnalysisReportData(m);
  m.setWavelengthNm(2000);
  m.setDoublePass(true);
  m.modalFitMethod() = ModalFitMethod::SequentialSeregin;
  p.synchronizeInputs(m);
  const auto after = buildModalAnalysisReportData(m);
  QCOMPARE(after.wavelengthNm, 1000.0);
  QCOMPARE(after.doublePass, false);
  QCOMPARE(after.fitMethod, ModalFitMethod::JointLeastSquares);
  QCOMPARE(after.rmsAfterNm, before.rmsAfterNm);
  // Even recomputing S4 cannot change the stored S5 report's source PV/units.
  QVERIFY(p.compute(StageId::S4, m));
  QCOMPARE(buildModalAnalysisReportData(m).pvBeforeNm, before.pvBeforeNm);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const QString path = dir.filePath("report.txt");
  QVERIFY2(digitqt::core::io::writeFullReport(path, m, error), qPrintable(error));
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray text = file.readAll();
  QVERIFY(text.contains("Wavelength: 1000 nm"));
  QVERIFY(text.contains("Fit method: Joint least squares"));
  QVERIFY(!text.contains("Fit method: Sequential"));
}

void PipelineValidityTest::phaseReportUsesComputedParameters() {
  Measurement m;
  std::vector<Bitmap> frames;
  const double pi = std::acos(-1.0);
  for (int n = 0; n < 4; ++n) {
    Bitmap frame(17, 17);
    for (int y = 0; y < 17; ++y)
      for (int x = 0; x < 17; ++x)
        frame.setPixel(x, y,
                       static_cast<uint8_t>(128 + 80 * std::cos(2 * pi * x / 8 + n * pi / 3)));
    frames.push_back(std::move(frame));
  }
  m.setPhaseShiftingFrames(std::move(frames), "series.idf");
  m.setPhaseReconstructionAlgorithm(PhaseReconstructionAlgorithm::PhaseShifting);
  m.phaseShifting().setNominalStepDeg(60);
  m.phaseShifting().setRefineShifts(false);
  Pipeline p;
  p.synchronizeInputs(m);
  // Image-based phase methods need no separate tracing after an aperture edit.
  p.stage(StageId::Setup).markComputed();
  m.boundaries().addExternal(std::make_unique<aperture::Ellipse>(8, 8, 8, 8));
  QVERIFY2(p.compute(StageId::S2, m), p.stage(StageId::S2).errorMessage().c_str());
  QVERIFY(!p.hasOutdatedResults());
  QVERIFY(m.phaseParameters().has_value());
  QCOMPARE(m.phaseParameters()->nominalStepDeg, 60.0);
  m.phaseShifting().setNominalStepDeg(90);
  m.phaseShifting().setRefineShifts(true);
  m.setPhaseReconstructionAlgorithm(PhaseReconstructionAlgorithm::FourierTransform);
  p.synchronizeInputs(m);
  QCOMPARE(p.stage(StageId::S2).status(), StageStatus::Stale);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const auto path = dir.filePath("phase-report.txt");
  QVERIFY2(digitqt::core::io::writeFullReport(path, m, error), qPrintable(error));
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const auto text = file.readAll();
  QVERIFY(text.contains("PSI settings: nominal step 60 deg"));
  QVERIFY(!text.contains("Fourier Transform Method"));
}

QTEST_APPLESS_MAIN(PipelineValidityTest)
#include "PipelineValidityTest.moc"
