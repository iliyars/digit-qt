#include "formats/FrnAdapter.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/geometry/Polygon.h>
#include <aperture/include/geometry/Rectangle.h>

#include <QtTest/QtTest>

namespace {

using digitqt::formats::Fidelity;
using digitqt::formats::FrnAdapter;
using digitqt::formats::ImportedData;

// Два файла, записанные оригинальным Digit для одной и той же
// интерферограммы (Generated.bmp, 1024x1024) в двух диалектах.
const char kDigitFile[] = DIGITQT_TEST_DATA_DIR "/Digit_frn.frn";
const char kWinFringeFile[] = DIGITQT_TEST_DATA_DIR "/FinFringe_FRN.frn";

QByteArray readAll(const char *path) {
  QFile file(QString::fromUtf8(path));
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool import(const QByteArray &bytes, ImportedData &data, std::string &error) {
  return FrnAdapter().importData(std::string_view(bytes.constData(), bytes.size()), data, error);
}

bool hasNote(const ImportedData &data, Fidelity kind, const char *fragment) {
  for (const auto &note : data.report)
    if (note.kind == kind && note.what.find(fragment) != std::string::npos)
      return true;
  return false;
}

class FrnAdapterTest : public QObject {
  Q_OBJECT

private slots:
  void probeRecognizesBothDialects();
  void importsImageReference();
  void importsApertureFromBothDialects();
  void denormalizesRotatedAndInternalShapes();
  void rejectsFileWithoutImageSection();
  void importsFringes();
};

void FrnAdapterTest::probeRecognizesBothDialects() {
  const FrnAdapter adapter;
  for (const char *path : {kDigitFile, kWinFringeFile}) {
    const QByteArray bytes = readAll(path);
    QVERIFY(!bytes.isEmpty());
    QVERIFY(adapter.probe(std::string_view(
        bytes.constData(), std::min<size_t>(bytes.size(), digitqt::formats::kProbeBytes))));
  }
  QVERIFY(!adapter.probe("Size=3\n[MATRIX]\n"));
}

void FrnAdapterTest::importsImageReference() {
  for (const char *path : {kDigitFile, kWinFringeFile}) {
    ImportedData data;
    std::string error;
    QVERIFY2(import(readAll(path), data, error), error.c_str());
    QCOMPARE(data.imageFileName, std::string("Generated.bmp"));
    QCOMPARE(data.imageWidth, 1024);
    QCOMPARE(data.imageHeight, 1024);
    QVERIFY(!data.phaseMap.has_value());
  }
}

void FrnAdapterTest::importsApertureFromBothDialects() {
  ImportedData digit, winFringe;
  std::string error;
  QVERIFY2(import(readAll(kDigitFile), digit, error), error.c_str());
  QVERIFY2(import(readAll(kWinFringeFile), winFringe, error), error.c_str());

  QVERIFY(hasNote(digit, Fidelity::Lossless, "Digit dialect"));
  QVERIFY(hasNote(winFringe, Fidelity::Converted, "Y axis flipped"));
  QVERIFY(hasNote(winFringe, Fidelity::Converted, "1 normalized shape(s)"));

  // Один и тот же эллипс: в Digit-файле записан в пикселях, в
  // WinFringe-файле -- как "1 1 0 0", и восстанавливается из [BOUNDS].
  // Оба файла хранят 3 знака после запятой.
  const aperture::Ellipse *ellipses[2] = {};
  int i = 0;
  for (const ImportedData *data : {&digit, &winFringe}) {
    QVERIFY(data->boundaries.has_value());
    QCOMPARE(data->boundaries->getExternal().size(), size_t(1));
    QCOMPARE(data->boundaries->totalCount(), size_t(1));
    ellipses[i] = dynamic_cast<const aperture::Ellipse *>(data->boundaries->getExternal()[0].get());
    QVERIFY(ellipses[i++]);
  }
  QCOMPARE(ellipses[0]->semiMajor(), 513.758);
  QCOMPARE(ellipses[0]->center().x, 514.203);
  QCOMPARE(ellipses[0]->center().y, 509.754);
  QVERIFY(std::fabs(ellipses[1]->semiMajor() - ellipses[0]->semiMajor()) < 2e-3);
  QVERIFY(std::fabs(ellipses[1]->semiMinor() - ellipses[0]->semiMinor()) < 2e-3);
  QVERIFY(std::fabs(ellipses[1]->center().x - ellipses[0]->center().x) < 2e-3);
  QVERIFY(std::fabs(ellipses[1]->center().y - ellipses[0]->center().y) < 2e-3);
}

void FrnAdapterTest::denormalizesRotatedAndInternalShapes() {
  // Окружность нормировки: центр (500, 400) в перевёрнутых пикселях,
  // радиус 200; высота изображения 1000. В [BOUNDS] попадает только
  // первый (неповёрнутый) эллипс -- по нему восстанавливается окружность,
  // остальные фигуры денормализуются ею же.
  const QByteArray text =
      "[GENERAL]\nTitle=\nScaleFactor=0.500\nFiScan=0.00\n\n"
      "[ELLIPSES]\n"
      " 1.000000 0.500000 0.000000 0.000000 0.000 1 1 \n"
      " 0.250000 0.100000 0.500000 0.250000 30.000 0 1 \n"
      "END\n\n"
      "[RECTANGLES]\n"
      " 0.100000 0.050000 -0.500000 0.000000 0.000 0 1 \n"
      "END\n\n"
      "[POLYGONS]\n"
      " 0 1 10.000 900.000 20.000 900.000\n"
      "     20.000 800.000 E\n"
      "END\n\n"
      "[BOUNDS]\n"
      " 300.000 700.000 300.000 500.000 1 1 \n"
      "END\n\n"
      "[FRINGES]\nEND\n"
      "[IMAGE_FILE]\nSize=1200 1000\nName=x.bmp\n";

  ImportedData data;
  std::string error;
  QVERIFY2(import(text, data, error), error.c_str());
  QVERIFY(hasNote(data, Fidelity::Converted, "3 normalized shape(s)"));
  QVERIFY(hasNote(data, Fidelity::Dropped, "ScaleFactor=0.500"));

  const auto &external = data.boundaries->getExternal();
  const auto &internal = data.boundaries->getInternal();
  QCOMPARE(external.size(), size_t(1));
  QCOMPARE(internal.size(), size_t(3));

  const auto *outer = dynamic_cast<const aperture::Ellipse *>(external[0].get());
  QVERIFY(outer);
  QCOMPARE(outer->semiMajor(), 200.0);
  QCOMPARE(outer->semiMinor(), 100.0);
  QCOMPARE(outer->center().x, 500.0);
  QCOMPARE(outer->center().y, 1000.0 - 400.0);

  const aperture::Ellipse *rotated = nullptr;
  const aperture::Rectangle *rect = nullptr;
  const aperture::Polygon *polygon = nullptr;
  for (const auto &shape : internal) {
    if (const auto *e = dynamic_cast<const aperture::Ellipse *>(shape.get())) rotated = e;
    if (const auto *r = dynamic_cast<const aperture::Rectangle *>(shape.get())) rect = r;
    if (const auto *p = dynamic_cast<const aperture::Polygon *>(shape.get())) polygon = p;
  }
  QVERIFY(rotated && rect && polygon);

  QCOMPARE(rotated->semiMajor(), 50.0);
  QCOMPARE(rotated->center().x, 600.0);
  QCOMPARE(rotated->center().y, 1000.0 - 450.0);
  QCOMPARE(rotated->rotationDegrees(), -30.0);

  QCOMPARE(rect->width(), 40.0);
  QCOMPARE(rect->height(), 20.0);
  QCOMPARE(rect->center().x, 400.0);
  QCOMPARE(rect->center().y, 600.0);

  QCOMPARE(polygon->vertexCount(), size_t(3));
  QCOMPARE(polygon->vertex(0).x, 10.0);
  QCOMPARE(polygon->vertex(0).y, 100.0);
  QCOMPARE(polygon->vertex(2).y, 200.0);
}

void FrnAdapterTest::rejectsFileWithoutImageSection() {
  ImportedData data;
  std::string error;
  QVERIFY(!import("[GENERAL]\nFiScan=0.00\n[FRINGES]\nNFringe=0.0\n 1.0 2.0\nE\nEND\n", data, error));
  QVERIFY(error.find("[IMAGE]") != std::string::npos);
  // WinFringe без высоты изображения: Y не перевернуть.
  QVERIFY(!import("[GENERAL]\nFiScan=0.00\n[FRINGES]\nEND\n[IMAGE_FILE]\nName=x.bmp\n", data, error));
}

void FrnAdapterTest::importsFringes() {
  ImportedData digit, winFringe;
  std::string error;
  QVERIFY2(import(readAll(kDigitFile), digit, error), error.c_str());
  QVERIFY2(import(readAll(kWinFringeFile), winFringe, error), error.c_str());

  QCOMPARE(digit.fringeLines.size(), size_t(57));
  QCOMPARE(winFringe.fringeLines.size(), size_t(57));

  // Первая полоса Digit-файла: "NFringe=0.0", 25 точек, первая "5.072 563.000".
  const auto &first = digit.fringeLines.front();
  QCOMPARE(first.order, 0.0);
  QVERIFY(first.orderIsManual);
  QCOMPARE(first.points.size(), size_t(25));
  QCOMPARE(first.points[0].x, 5.072);
  QCOMPARE(first.points[0].y, 563.0);
  QCOMPARE(digit.fringeLines.back().order, 56.0);

  // После переворота Y оба диалекта дают одни и те же линии.
  size_t totalPoints = 0;
  for (size_t i = 0; i < digit.fringeLines.size(); ++i) {
    const auto &a = digit.fringeLines[i];
    const auto &b = winFringe.fringeLines[i];
    QCOMPARE(a.order, b.order);
    QCOMPARE(a.points.size(), b.points.size());
    for (size_t j = 0; j < a.points.size(); ++j) {
      QCOMPARE(a.points[j].x, b.points[j].x);
      QCOMPARE(a.points[j].y, b.points[j].y);
    }
    totalPoints += a.points.size();
  }
  QCOMPARE(totalPoints, size_t(4801));

  // Неполная пара координат -- ошибка, а не молчаливый пропуск.
  ImportedData broken;
  QVERIFY(!import("[GENERAL]\nFiScan=0.00\n[FRINGES]\nNFringe=0.0\n 1.0\nE\nEND\n"
                  "[IMAGE]\nSize=10 10\nFileName=x.bmp\n",
                  broken, error));
}

}  // namespace

QTEST_APPLESS_MAIN(FrnAdapterTest)
#include "FrnAdapterTest.moc"
