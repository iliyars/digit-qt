#include "core/Measurement.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"
#include "gui/canvas/Surface3DView.h"

#include <QImage>
#include <QtConcurrent>
#include <QtTest/QtTest>
#include <cmath>
#include <limits>

namespace {
digitqt::core::PhaseMap surface(int size) {
  digitqt::core::PhaseMap map(size, size);
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x)
      map.setValue(x, y, 0.2 * x * x + x * y - 0.3 * y + std::sin(x * 0.4));
  return map;
}
QImage render(QWidget &view) {
  QImage image(view.size(), QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  view.render(&image);
  return image;
}
}  // namespace

class Surface3DViewTest : public QObject {
  Q_OBJECT
private slots:
  void paddingAndOffsetDoNotChangePupilCoordinates() {
    digitqt::core::Measurement cropped;
    cropped.modalAnalysis().residual = surface(25);
    digitqt::core::Measurement padded;
    padded.modalAnalysis().residual = digitqt::core::PhaseMap(81, 73);
    for (int y = 0; y < 25; ++y)
      for (int x = 0; x < 25; ++x)
        padded.modalAnalysis().residual.setValue(
            x + 33, y + 17, cropped.modalAnalysis().residual.value(x, y));
    digitqt::gui::canvas::Surface3DView view;
    view.resize(640, 480);
    view.setMeasurement(&cropped);
    const QImage expected = render(view);
    view.setMeasurement(&padded);
    QCOMPARE(render(view), expected);
  }

  void erosionKeepsOriginalPupilScale() {
    digitqt::core::Measurement original;
    original.wavefrontMap() = surface(25);
    original.modalAnalysis().residual = original.wavefrontMap();
    // Two views of the same eroded residual must use the original pupil even
    // when the residual's empty padding and offset are different.
    for (int y = 0; y < 25; ++y)
      for (int x = 0; x < 25; ++x)
        if (x < 3 || x > 21 || y < 3 || y > 21)
          original.modalAnalysis().residual.setValue(x, y, std::numeric_limits<double>::quiet_NaN());
    digitqt::core::Measurement padded;
    padded.wavefrontMap() = digitqt::core::PhaseMap(81, 73);
    padded.modalAnalysis().residual = digitqt::core::PhaseMap(81, 73);
    for (int y = 0; y < 25; ++y)
      for (int x = 0; x < 25; ++x) {
        padded.wavefrontMap().setValue(x + 33, y + 17, original.wavefrontMap().value(x, y));
        padded.modalAnalysis().residual.setValue(
            x + 33, y + 17, original.modalAnalysis().residual.value(x, y));
      }
    digitqt::gui::canvas::Surface3DView view;
    view.resize(640, 480);
    view.setMeasurement(&original);
    const QImage expected = render(view);
    view.setMeasurement(&padded);
    QCOMPARE(render(view), expected);
  }

  void displayedSurfaceChangesOnlyOnRefresh() {
    digitqt::core::Measurement measurement;
    measurement.modalAnalysis().residual = surface(25);
    digitqt::gui::canvas::Surface3DView view;
    view.resize(640, 480);
    view.setMeasurement(&measurement);
    const QImage before = render(view);
    measurement.modalAnalysis().clear();
    QCOMPARE(render(view), before);
    view.refresh();
    QVERIFY(render(view) != before);
    view.setMeasurement(nullptr);
    render(view);
  }

  void repaintDuringRepeatedBackgroundFits() {
    digitqt::core::Measurement measurement;
    measurement.wavefrontMap() = surface(25);
    measurement.modalAnalysis().residual = surface(25);
    digitqt::gui::canvas::Surface3DView view;
    view.resize(640, 480);
    view.setMeasurement(&measurement);
    const QImage before = render(view);
    auto future = QtConcurrent::run([&measurement] {
      digitqt::core::pipeline::ModalAnalysisStage stage;
      for (const auto basis : {digitqt::core::PolynomialBasis::Seregin,
                               digitqt::core::PolynomialBasis::Zernike}) {
        measurement.polynomialBasis() = basis;
        for (const auto method : {digitqt::core::ModalFitMethod::JointLeastSquares,
                                  digitqt::core::ModalFitMethod::GramSchmidtOnAperture,
                                  digitqt::core::ModalFitMethod::SequentialSeregin}) {
          measurement.modalFitMethod() = method;
          for (int mask = 0; mask < 64; ++mask) {
            measurement.modalTermSelection() = {
                bool(mask & 1), bool(mask & 2), bool(mask & 4),
                bool(mask & 8), bool(mask & 16), bool(mask & 32)};
            if (!stage.compute(measurement))
              return false;
          }
        }
      }
      return true;
    });
    bool unchanged = true;
    do {
      unchanged = (render(view) == before) && unchanged;
      QCoreApplication::processEvents();
    } while (!future.isFinished());
    QVERIFY(future.result());
    QVERIFY(unchanged);
    QCOMPARE(render(view), before);
    view.refresh();
    QVERIFY(render(view) != before);
  }
};

QTEST_MAIN(Surface3DViewTest)
#include "Surface3DViewTest.moc"
