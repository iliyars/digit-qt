#include "core/ImagePadding.h"

#include <QtTest/QtTest>

namespace {

class ImagePaddingTest : public QObject {
  Q_OBJECT

private slots:
  void nullImagePassesThrough();
  void padsWithMajorityBorderColor();
  void marginIsFractionOfLargerSide();
};

void ImagePaddingTest::nullImagePassesThrough() {
  QImage empty;
  const QImage result = digitqt::core::padImageBackground(empty);
  QVERIFY(result.isNull());
}

void ImagePaddingTest::padsWithMajorityBorderColor() {
  QImage image(20, 20, QImage::Format_Grayscale8);
  image.fill(10);  // background
  for (int y = 5; y < 15; ++y)
    for (int x = 5; x < 15; ++x)
      image.setPixel(x, y, qRgb(200, 200, 200));  // "fringe" content, away from the border

  const QImage padded = digitqt::core::padImageBackground(image, 0.1);

  QCOMPARE(padded.width(), 20 + 2 * 2);
  QCOMPARE(padded.height(), 20 + 2 * 2);

  // Original content should reappear unchanged at the margin offset.
  QCOMPARE(qGray(padded.pixel(2 + 7, 2 + 7)), 200);
  QCOMPARE(qGray(padded.pixel(2 + 0, 2 + 0)), 10);

  // The new border ring is filled with the detected background color.
  QCOMPARE(qGray(padded.pixel(0, 0)), 10);
  QCOMPARE(qGray(padded.pixel(padded.width() - 1, padded.height() - 1)), 10);
}

void ImagePaddingTest::marginIsFractionOfLargerSide() {
  QImage image(1024, 1024, QImage::Format_Grayscale8);
  image.fill(0);

  const QImage padded = digitqt::core::padImageBackground(image, 0.05);

  QCOMPARE(padded.width(), 1024 + 2 * 51);
  QCOMPARE(padded.height(), 1024 + 2 * 51);
}

}  // namespace

QTEST_MAIN(ImagePaddingTest)
#include "ImagePaddingTest.moc"
