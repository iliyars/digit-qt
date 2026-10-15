#include "formats/IdfFile.h"

#include <QtTest/QtTest>

namespace {

using digitqt::formats::IdfDescription;
using digitqt::formats::parseIdf;
using digitqt::formats::probeIdf;

// Байт в байт C33_.IDF из присланных Серегиным серий (CRLF, пустая
// строка после заголовка, ширина раньше высоты).
const char kC33[] = "Interferograms descriptive file\r\n\r\n5\r\n400\r\n400\r\n"
                    "c33_0.bmp\r\nc33_1.bmp\r\nc33_2.bmp\r\nc33_3.bmp\r\nc33_4.bmp";

class IdfFileTest : public QObject {
  Q_OBJECT

private slots:
  void parsesRealFile();
  void widthComesBeforeHeight();
  void ignoresTrailingLines();
  void rejectsWrongHeader();
  void rejectsMissingFrames();
  void rejectsBadNumbers();
};

void IdfFileTest::parsesRealFile() {
  QVERIFY(probeIdf(kC33));
  IdfDescription idf;
  std::string error;
  QVERIFY2(parseIdf(kC33, idf, error), error.c_str());
  QCOMPARE(idf.width, 400);
  QCOMPARE(idf.height, 400);
  QCOMPARE(idf.frameFiles.size(), size_t{5});
  QCOMPARE(QString::fromStdString(idf.frameFiles.front()), QStringLiteral("c33_0.bmp"));
  QCOMPARE(QString::fromStdString(idf.frameFiles.back()), QStringLiteral("c33_4.bmp"));
}

void IdfFileTest::widthComesBeforeHeight() {
  // big.idf: кадры 320x240, файл заканчивается переводом строки.
  IdfDescription idf;
  std::string error;
  QVERIFY(parseIdf("Interferograms descriptive file\n\n3\n320\n240\na.bmp\nb.bmp\nc.bmp\n", idf,
                   error));
  QCOMPARE(idf.width, 320);
  QCOMPARE(idf.height, 240);
}

void IdfFileTest::ignoresTrailingLines() {
  IdfDescription idf;
  std::string error;
  QVERIFY(parseIdf("Interferograms descriptive file\n3\n8\n8\na.bmp\nb.bmp\nc.bmp\nextra.bmp\n",
                   idf, error));
  QCOMPARE(idf.frameFiles.size(), size_t{3});
}

void IdfFileTest::rejectsWrongHeader() {
  QVERIFY(!probeIdf("Size=3\r\n[MATRIX]\r\n"));
  IdfDescription idf;
  std::string error;
  QVERIFY(!parseIdf("Size=3\n3\n8\n8\na\nb\nc\n", idf, error));
  QVERIFY(!error.empty());
}

void IdfFileTest::rejectsMissingFrames() {
  IdfDescription idf;
  std::string error;
  QVERIFY(!parseIdf("Interferograms descriptive file\n5\n8\n8\na.bmp\nb.bmp\n", idf, error));
  QVERIFY(QString::fromStdString(error).contains(QStringLiteral("only 2")));
}

void IdfFileTest::rejectsBadNumbers() {
  IdfDescription idf;
  std::string error;
  QVERIFY(!parseIdf("Interferograms descriptive file\n5\nwide\n8\n", idf, error));
  QVERIFY(QString::fromStdString(error).contains(QStringLiteral("width")));
  QVERIFY(!parseIdf("Interferograms descriptive file\n0\n8\n8\n", idf, error));
  QVERIFY(QString::fromStdString(error).contains(QStringLiteral("count")));
}

}  // namespace

QTEST_GUILESS_MAIN(IdfFileTest)
#include "IdfFileTest.moc"
