#include "core/Measurement.h"
#include "core/pipeline/Pipeline.h"
#include "gui/PipelineTreeDock.h"
#include "gui/PsiWorkflowPanel.h"

#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QtTest/QtTest>
#include <aperture/include/geometry/Ellipse.h>

using digitqt::core::pipeline::StageId;
namespace {
void load(digitqt::core::Measurement &measurement) {
  std::vector<digitqt::core::Bitmap> frames(5, digitqt::core::Bitmap(32, 32));
  for (size_t i = 0; i < frames.size(); ++i)
    frames[i].fill(static_cast<uint8_t>(30 + i));
  measurement.setPhaseShiftingFrames(std::move(frames), "source.bmp");
  measurement.setPhaseReconstructionAlgorithm(
      digitqt::core::PhaseReconstructionAlgorithm::PhaseShifting);
}
}  // namespace
class PsiWorkflowPanelTest : public QObject {
  Q_OBJECT
private slots:
  void previewDoesNotMutateInputs() {
    digitqt::core::Measurement m;
    load(m);
    digitqt::core::pipeline::Pipeline pipeline;
    pipeline.synchronizeInputs(m);
    digitqt::gui::PsiWorkflowPanel panel;
    panel.setMeasurement(&m, &pipeline);
    auto *list = panel.findChild<QListWidget *>("psiFrames");
    QCOMPARE(list->count(), 5);
    QVERIFY(!list->item(0)->icon().isNull());
    QSignalSpy selected(&panel, &digitqt::gui::PsiWorkflowPanel::frameSelected);
    list->setCurrentRow(3);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected[0][0].toInt(), 3);
    QCOMPARE(m.image().pixel(0, 0), uint8_t(30));
    QCOMPARE(m.phaseShifting().frames()[3].pixel(0, 0), uint8_t(33));
    QCOMPARE(m.phaseShifting().nominalStepDeg(), 90.0);
    QVERIFY(!pipeline.hasOutdatedResults());
  }
  void calculationNeedsApertureOrExplicitFullFrame() {
    digitqt::core::Measurement m;
    load(m);
    digitqt::core::pipeline::Pipeline pipeline;
    digitqt::gui::PsiWorkflowPanel panel;
    panel.setMeasurement(&m, &pipeline);
    auto *calculate = panel.findChild<QPushButton *>("psiCalculate");
    auto *full = panel.findChild<QCheckBox *>("psiFullFrame");
    QVERIFY(!calculate->isEnabled());
    QSignalSpy requests(&panel, &digitqt::gui::PsiWorkflowPanel::calculateRequested);
    full->setChecked(true);
    QVERIFY(calculate->isEnabled());
    calculate->click();
    QCOMPARE(requests.count(), 1);
    QVERIFY(!m.boundaries().hasAnyExternal());
    m.boundaries().addExternal(std::make_unique<aperture::Ellipse>(10, 10, 16, 16));
    panel.refresh();
    QVERIFY(full->isHidden());
    QVERIFY(!full->isChecked());
    QVERIFY(calculate->isEnabled());
  }
  void staleResultsCannotBeExportedAndRemarksRemain() {
    digitqt::core::Measurement m;
    load(m);
    digitqt::core::pipeline::Pipeline pipeline;
    pipeline.synchronizeInputs(m);
    for (auto id : {StageId::S2, StageId::S4, StageId::S5})
      pipeline.stage(id).markComputed();
    m.phaseMap() = digitqt::core::PhaseMap(32, 32);
    digitqt::gui::PsiWorkflowPanel panel;
    panel.setMeasurement(&m, &pipeline);
    panel.setWarnings({"Keep this remark"});
    auto *report = panel.findChild<QPushButton *>("psiExportReport");
    auto *phase = panel.findChild<QPushButton *>("psiPhase");
    QVERIFY(report->isEnabled());
    QVERIFY(phase->isEnabled());
    m.phaseShifting().setNominalStepDeg(70);
    pipeline.synchronizeInputs(m);
    panel.refresh();
    QVERIFY(!report->isEnabled());
    QVERIFY(!phase->isEnabled());
    QVERIFY(panel.findChild<QLabel *>("psiQuality")->text().contains("out of date"));
    QCOMPARE(panel.findChild<QPlainTextEdit *>("psiWarnings")->toPlainText(),
             QString("Keep this remark"));
    panel.setMeasurement(&m, &pipeline);
    QVERIFY(panel.findChild<QPlainTextEdit *>("psiWarnings")->toPlainText().isEmpty());
  }
  void resultButtonsWaitForPhaseAndRespectLaterStages() {
    digitqt::core::Measurement m;
    load(m);
    digitqt::core::pipeline::Pipeline pipeline;
    digitqt::gui::PsiWorkflowPanel panel;
    panel.setMeasurement(&m, &pipeline);
    const auto enabled = [&](const char *name) {
      return panel.findChild<QPushButton *>(name)->isEnabled();
    };
    for (const auto *name : {"psiSource", "psiPhase", "psiWavefront", "psiAnalysis",
                             "psiExportMap", "psiExportReport"})
      QVERIFY(!enabled(name));
    QSignalSpy source(&panel, &digitqt::gui::PsiWorkflowPanel::sourceRequested);
    panel.findChild<QPushButton *>("psiSource")->click();
    QCOMPARE(source.count(), 0);
    pipeline.stage(StageId::S2).markComputed();
    panel.refresh();
    QVERIFY(enabled("psiSource"));
    QVERIFY(enabled("psiPhase"));
    QVERIFY(!enabled("psiWavefront"));
    QVERIFY(!enabled("psiAnalysis"));
    QVERIFY(!enabled("psiExportMap"));
    QVERIFY(!enabled("psiExportReport"));
    pipeline.stage(StageId::S4).markComputed();
    pipeline.stage(StageId::S5).markComputed();
    panel.refresh();
    for (const auto *name : {"psiSource", "psiPhase", "psiWavefront", "psiAnalysis",
                             "psiExportMap", "psiExportReport"})
      QVERIFY(enabled(name));
    pipeline.invalidateFrom(StageId::S2);
    panel.refresh();
    for (const auto *name : {"psiSource", "psiPhase", "psiWavefront", "psiAnalysis",
                             "psiExportMap", "psiExportReport"})
      QVERIFY(!enabled(name));
  }
  void errorRemainsVisible() {
    digitqt::core::Measurement m;
    load(m);
    digitqt::core::pipeline::Pipeline pipeline;
    pipeline.stage(StageId::S4).setError("Missing wavelength");
    digitqt::gui::PsiWorkflowPanel panel;
    panel.setMeasurement(&m, &pipeline);
    QVERIFY(panel.findChild<QLabel *>("psiNextStep")->text().contains("Missing wavelength"));
    QVERIFY(!panel.findChild<QPushButton *>("psiExportMap")->isEnabled());
  }
  void psiNavigationKeepsOtherMethodsAvailable() {
    digitqt::gui::PipelineTreeDock dock;
    auto *tree = dock.findChild<QTreeWidget *>();
    const int total = tree->topLevelItemCount();
    QVERIFY(total > 4);
    dock.setPsiMode(true);
    int visible = 0;
    for (int i = 0; i < total; ++i)
      visible += !tree->topLevelItem(i)->isHidden();
    QCOMPARE(visible, 4);
    dock.setPsiMode(false);
    for (int i = 0; i < total; ++i)
      QVERIFY(!tree->topLevelItem(i)->isHidden());
  }
};
QTEST_MAIN(PsiWorkflowPanelTest)
#include "PsiWorkflowPanelTest.moc"
