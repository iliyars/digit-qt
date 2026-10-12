#include "formats/MtrAdapter.h"
#include "io/MtrImporter.h"

#include <QtTest/QtTest>

namespace {

using digitqt::formats::Fidelity;
using digitqt::formats::ImportedData;
using digitqt::formats::MtrAdapter;

// 3x3 grid: ratio = 2/(3-1) = 1, so normalized -1/0/1 map to cells 0/1/2
// (Y inverted: +1 is the top row).
const char kSmallMtr[] =
    "Title=\n"
    "Date=2026-10-12\n"
    "Time=10:00:00\n"
    "Units=WAV\n"
    "\n"
    "Size=3\n"
    "[MATRIX]\n"
    "1.0000 0.5 -1.0000 0.6 0.0000 E\n"
    "0.0000 0.7 0.0000\n"
    "        0.8 1.0000 E\n"
    "END\n";

bool hasNote(const ImportedData &data, Fidelity kind, const char *fragment) {
  for (const auto &note : data.report)
    if (note.kind == kind && note.what.find(fragment) != std::string::npos)
      return true;
  return false;
}

class MtrAdapterTest : public QObject {
  Q_OBJECT

private slots:
  void probeRecognizesMtrByContent();
  void importsSmallMatrix();
  void reportsWhatWasLost();
  void rejectsMalformedInput();
  void matchesLegacyReader_data();
  void matchesLegacyReader();
};

void MtrAdapterTest::probeRecognizesMtrByContent() {
  const MtrAdapter adapter;
  QVERIFY(adapter.probe(kSmallMtr));
  QVERIFY(!adapter.probe("[GENERAL]\nTitle=\n[FRINGES]\nNFringe=1.0\n"));
  QVERIFY(!adapter.probe("Size=3\n"));
  QVERIFY(!adapter.probe(""));
}

void MtrAdapterTest::importsSmallMatrix() {
  ImportedData data;
  std::string error;
  QVERIFY2(MtrAdapter().importData(kSmallMtr, data, error), error.c_str());

  QVERIFY(data.phaseMap.has_value());
  QVERIFY(!data.boundaries.has_value());
  QVERIFY(data.fringeLines.empty());

  const auto &phase = *data.phaseMap;
  QCOMPARE(phase.width(), 3);
  QCOMPARE(phase.height(), 3);
  QCOMPARE(phase.value(0, 0), 0.5);
  QCOMPARE(phase.value(1, 0), 0.6);
  QCOMPARE(phase.value(1, 1), 0.7);
  QCOMPARE(phase.value(2, 1), 0.8);
  QVERIFY(!phase.hasValue(2, 2));
}

void MtrAdapterTest::reportsWhatWasLost() {
  ImportedData clean;
  std::string error;
  QVERIFY2(MtrAdapter().importData(kSmallMtr, clean, error), error.c_str());
  QVERIFY(hasNote(clean, Fidelity::Converted, "3x3 grid"));
  QVERIFY(hasNote(clean, Fidelity::Lossless, "Units=WAV"));
  QVERIFY(hasNote(clean, Fidelity::Dropped, "Title/Date/Time"));
  QVERIFY(!hasNote(clean, Fidelity::Dropped, "outside"));

  // No Units=, one point far outside the grid, one NaN, two points on one cell.
  ImportedData lossy;
  QVERIFY2(MtrAdapter().importData("Size=3\n[MATRIX]\n"
                                   "0.0 0.1 0.0 0.2 5.0 nan 1.0 0.3 0.1 E\nEND\n",
                                   lossy, error),
           error.c_str());
  QVERIFY(hasNote(lossy, Fidelity::Missing, "Units="));
  QVERIFY(hasNote(lossy, Fidelity::Dropped, "1 point(s) outside"));
  QVERIFY(hasNote(lossy, Fidelity::Dropped, "1 NaN/Inf"));
  QVERIFY(hasNote(lossy, Fidelity::Converted, "1 point(s) rounded"));
  QCOMPARE(lossy.phaseMap->value(1, 1), 0.3);
}

void MtrAdapterTest::rejectsMalformedInput() {
  const MtrAdapter adapter;
  ImportedData data;
  std::string error;
  QVERIFY(!adapter.importData("[MATRIX]\n0 1 0 E\nEND\n", data, error));
  QVERIFY(!adapter.importData("Size=3\n0 1 0 E\n", data, error));
  QVERIFY(!adapter.importData("Size=3\n[MATRIX]\nEND\n", data, error));
  QVERIFY(!adapter.importData("Size=3\n[MATRIX]\n0.0 0.1\nEND\n", data, error));
  QVERIFY(!adapter.importData("Size=3\n[MATRIX]\n0.0 abc 0.0 E\nEND\n", data, error));
  QVERIFY(!data.phaseMap.has_value());
}

void MtrAdapterTest::matchesLegacyReader_data() {
  QTest::addColumn<QString>("fileName");
  // Terminal.mtr -- исходный файл WinFringe, остальные два записаны
  // оригинальным Digit и нашим MtrExporter.
  QTest::newRow("WinFringe") << "Terminal.mtr";
  QTest::newRow("legacy Digit") << "Terminal_digit.mtr";
  QTest::newRow("DigitQt") << "my_terminal.mtr";
}

void MtrAdapterTest::matchesLegacyReader() {
  QFETCH(QString, fileName);
  const QString path = QStringLiteral(DIGITQT_TEST_DATA_DIR "/") + fileName;

  digitqt::core::PhaseMap legacy;
  QString legacyError;
  QVERIFY2(digitqt::core::io::readMtrFile(path, legacy, legacyError), qPrintable(legacyError));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray bytes = file.readAll();

  const MtrAdapter adapter;
  QVERIFY(adapter.probe(std::string_view(
      bytes.constData(), std::min<size_t>(bytes.size(), digitqt::formats::kProbeBytes))));

  ImportedData data;
  std::string error;
  QVERIFY2(adapter.importData(std::string_view(bytes.constData(), bytes.size()), data, error),
           error.c_str());

  const auto &phase = *data.phaseMap;
  QCOMPARE(phase.width(), legacy.width());
  QCOMPARE(phase.height(), legacy.height());
  int mismatches = 0;
  for (int y = 0; y < phase.height(); ++y)
    for (int x = 0; x < phase.width(); ++x)
      if (phase.hasValue(x, y) != legacy.hasValue(x, y) ||
          (phase.hasValue(x, y) && phase.value(x, y) != legacy.value(x, y)))
        ++mismatches;
  QCOMPARE(mismatches, 0);

  for (const auto &note : data.report)
    qInfo("%d: %s", static_cast<int>(note.kind), note.what.c_str());
}

}  // namespace

QTEST_APPLESS_MAIN(MtrAdapterTest)
#include "MtrAdapterTest.moc"
