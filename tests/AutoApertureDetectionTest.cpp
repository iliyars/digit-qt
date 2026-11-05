#include "core/AutoApertureDetection.h"
#include "core/ImagePadding.h"
#include "core/Measurement.h"
#include "gui/canvas/BoundaryEditController.h"
#include "gui/canvas/BoundaryItem.h"

#include <QGraphicsScene>
#include <QPainter>
#include <QtTest/QtTest>
#include <aperture/include/geometry/Ellipse.h>
#include <cmath>

namespace {
using digitqt::core::Bitmap;
// First exposure has no spatial contrast at all. Only a series-based
// detector can locate the off-centre elliptical pupil in this case.
std::vector<Bitmap> pupilSeries(bool padded = false) {
  std::vector<Bitmap> frames;
  const double pi = std::acos(-1.0);
  for (const double shift : {pi / 2, 0.0, pi, 3 * pi / 2}) {
    Bitmap frame(180, 140);
    frame.fill(120);
    for (int y = 0; y < 140; ++y)
      for (int x = 0; x < 180; ++x)
        if (std::pow((x - 103.0) / 51, 2) + std::pow((y - 61.0) / 37, 2) <= 1)
          frame.setPixel(x, y, static_cast<uint8_t>(std::lround(120 + 80 * std::cos(shift))));
    frames.push_back(padded ? digitqt::core::padImageBackground(frame) : frame);
  }
  return frames;
}
void checkPupil(const aperture::Ellipse &ellipse, double margin) {
  const auto bounds = ellipse.getBounds();
  QVERIFY(std::abs((bounds.minX() + bounds.maxX()) / 2 - (103 + margin)) < 1);
  QVERIFY(std::abs((bounds.minY() + bounds.maxY()) / 2 - (61 + margin)) < 1);
  QVERIFY(std::abs((bounds.maxX() - bounds.minX()) / 2 - 51) < 1.5);
  QVERIFY(std::abs((bounds.maxY() - bounds.minY()) / 2 - 37) < 1.5);
}
}  // namespace

class AutoApertureDetectionTest : public QObject {
  Q_OBJECT
private slots:
  void singleImageCurvedFringesOnBlackBackground() {
    for (int size : {256, 825}) {
      Bitmap image(size, size);
      image.fill(0);
      const double cx = .50 * size, cy = .50 * size, radius = .385 * size;
      for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
          const double u = (x - cx) / radius, v = (y - cy) / radius;
          if (u * u + v * v <= 1)
            image.setPixel(x, y, static_cast<uint8_t>(std::lround(
                127.5 * (1 + std::cos(32 * v + 18 * u + 12 * u * u + 20 * v * v)))));
        }
      const auto result = digitqt::core::detectApertureBoundary(image);
      QVERIFY2(result.ok(), result.errorMessage.c_str());
      const auto bounds = result.ellipse->getBounds();
      QVERIFY(std::abs((bounds.minX() + bounds.maxX()) / 2 - cx) < .02 * size);
      QVERIFY(std::abs((bounds.minY() + bounds.maxY()) / 2 - cy) < .02 * size);
      QVERIFY(std::abs((bounds.maxX() - bounds.minX()) / 2 - radius) < 3);
      QVERIFY(std::abs((bounds.maxY() - bounds.minY()) / 2 - radius) < 3);
    }
  }
  void psiUsesAllFramesAndPreservesPadding() {
    for (bool padded : {false, true}) {
      const auto frames = pupilSeries(padded);
      QVERIFY(!digitqt::core::detectApertureBoundary(frames[0]).ok());
      const auto result = digitqt::core::detectApertureBoundary(frames);
      QVERIFY2(result.ok(), result.errorMessage.c_str());
      checkPupil(*result.ellipse, padded ? 27 : 0);
    }
  }
  void blankNoiseAndInvalidSeriesFail() {
    using digitqt::core::detectApertureBoundary;
    QVERIFY(!detectApertureBoundary(std::vector<Bitmap>{}).ok());
    auto frames = pupilSeries();
    frames.back() = Bitmap(4, 4);
    QVERIFY(!detectApertureBoundary(frames).ok());
    frames.assign(4, Bitmap(180, 140));
    for (auto &frame : frames)
      frame.fill(120);
    QVERIFY(!detectApertureBoundary(frames).ok());
    frames.back().setPixel(90, 70, 255);
    QVERIFY(!detectApertureBoundary(frames).ok());
  }
  void dimPupilEdgeIsNotMistakenForItsBoundary() {
    auto frames = pupilSeries();
    for (auto &frame : frames)
      for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x) {
          const double r2 = std::pow((x - 103.0) / 51, 2) + std::pow((y - 61.0) / 37, 2);
          const double value = 120 + (frame.pixel(x, y) - 120) * std::exp(-2 * r2);
          frame.setPixel(x, y, static_cast<uint8_t>(std::lround(value)));
        }
    const auto result = digitqt::core::detectApertureBoundary(frames);
    QVERIFY2(result.ok(), result.errorMessage.c_str());
    checkPupil(*result.ellipse, 0);
  }
  void ordinaryImageStillFindsSpatialFringes() {
    Bitmap image(240, 200);
    image.fill(120);
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 240; ++x)
        if (std::pow((x - 120.0) / 80, 2) + std::pow((y - 100.0) / 60, 2) <= 1)
          image.setPixel(x, y,
                         static_cast<uint8_t>(std::lround(120 + 90 * std::cos(.6 * x + .3 * y))));
    const auto result = digitqt::core::detectApertureBoundary(image);
    QVERIFY2(result.ok(), result.errorMessage.c_str());
    const auto bounds = result.ellipse->getBounds();
    QVERIFY(std::abs((bounds.minX() + bounds.maxX()) / 2 - 120) < 3);
    QVERIFY(std::abs((bounds.minY() + bounds.maxY()) / 2 - 100) < 3);
    QVERIFY(std::abs((bounds.maxX() - bounds.minX()) / 2 - 80) < 12);
    QVERIFY(std::abs((bounds.maxY() - bounds.minY()) / 2 - 60) < 12);
  }
  void controllerPublishesVisibleGeometryAndSupportsUndo() {
    digitqt::core::Measurement measurement;
    measurement.setPhaseShiftingFrames(pupilSeries(true), "first.bmp", 180, 140);
    measurement.boundaries().addExternal(std::make_unique<aperture::Ellipse>(5, 5, 10, 10));
    QUndoStack undo;
    digitqt::gui::canvas::BoundaryEditController controller(&undo);
    controller.setMeasurement(&measurement);
    QSignalSpy changed(&controller,
                       &digitqt::gui::canvas::BoundaryEditController::boundariesChanged);
    controller.autoDetectAperture();
    QVERIFY(controller.lastError().isEmpty());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(measurement.boundaries().getExternal().size(), size_t(1));
    const auto *ellipse =
        dynamic_cast<const aperture::Ellipse *>(measurement.boundaries().getExternal()[0].get());
    QVERIFY(ellipse);
    checkPupil(*ellipse, 27);
    // Exercise the actual canvas item and rasterization, not just the model
    // counter: a successfully added boundary must have visible green pixels.
    {
      QGraphicsScene scene;
      scene.setSceneRect(0, 0, measurement.image().width(), measurement.image().height());
      auto *item = new digitqt::gui::canvas::BoundaryItem(ellipse, 0);
      scene.addItem(item);
      QVERIFY(!item->path().isEmpty());
      QImage image(measurement.image().width(), measurement.image().height(), QImage::Format_RGB32);
      image.fill(Qt::black);
      QPainter painter(&image);
      scene.render(&painter);
      painter.end();
      int green = 0;
      for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
          const auto pixel = image.pixelColor(x, y);
          if (pixel.green() > 100 && pixel.green() > pixel.red() + 50)
            ++green;
        }
      QVERIFY(green > 100);
    }
    const auto contour = ellipse->getContour(2);
    QVERIFY(contour.size() > 50);
    for (const auto &p : contour) {
      QVERIFY(std::isfinite(p.x) && std::isfinite(p.y));
      QVERIFY(p.x >= 0 && p.x < measurement.image().width());
      QVERIFY(p.y >= 0 && p.y < measurement.image().height());
    }
    undo.undo();
    QCOMPARE(measurement.boundaries().getExternal()[0]->getBounds().minX(), 5.0);
    undo.redo();
    checkPupil(dynamic_cast<const aperture::Ellipse &>(*measurement.boundaries().getExternal()[0]),
               27);
  }
};
QTEST_MAIN(AutoApertureDetectionTest)
#include "AutoApertureDetectionTest.moc"

