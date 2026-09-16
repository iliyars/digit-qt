// mtr_diff -- сравнивает две .mtr-карты (например, эталонную от
// WinFringe и нашу собственную реконструкцию/импорт) РОВНО тем же кодом,
// что использует сама программа: io::readMtrFile, WavefrontReconstructionStage,
// core::computeApertureGeometry/collectApertureSamples и ModalAnalysisStage.
// Никакой параллельной реимплементации формул -- если раньше нужно было
// сверять числа вручную в отдельном Python-скрипте (легко ошибиться в
// согласовании координат, как показала практика), то этот инструмент
// прогоняет оба файла и их разность через настоящий, уже проверенный
// пайплайн.
//
// Usage:
//   mtr_diff <reference.mtr> <mine.mtr> [--wavelength=NM] [--double-pass]
//
// По умолчанию: wavelength=632.8, double-pass выключен -- те же
// настройки, что использовались во всей текущей сессии сверки с
// test_data/report.txt.

#include "core/ApertureSamples.h"
#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/ModalFitMethod.h"
#include "core/PhaseMap.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"
#include "core/pipeline/stages/WavefrontReconstructionStage.h"
#include "io/MtrImporter.h"
#include "io/ModalReportExporter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct Loaded {
  digitqt::core::Measurement measurement;
  digitqt::core::ApertureGeometry geometry;
  std::vector<digitqt::core::ApertureSample> samples;  // erosion=0, полный набор -- для карты разницы
};

bool loadMtr(const QString &path, double wavelengthNm, bool doublePass, Loaded &out, QString &err) {
  digitqt::core::PhaseMap phase;
  if (!digitqt::core::io::readMtrFile(path, phase, err))
    return false;

  out.measurement.setImportedPhaseMap(std::move(phase));
  out.measurement.setWavelengthNm(wavelengthNm);
  out.measurement.setDoublePass(doublePass);
  out.measurement.modalFitMethod() = digitqt::core::ModalFitMethod::SequentialSeregin;

  digitqt::core::pipeline::WavefrontReconstructionStage s4;
  if (!s4.compute(out.measurement)) {
    err = s4.errorMessage();
    return false;
  }

  out.geometry = digitqt::core::computeApertureGeometry(out.measurement.wavefrontMap());
  out.samples =
      digitqt::core::collectApertureSamples(out.measurement.wavefrontMap(), out.geometry, 0);
  return true;
}

// Печатает уже готовый текстовый отчёт (io::writeModalReport) в консоль --
// сначала считает S5 (SequentialSeregin, тот же способ для всех трёх
// карт: ref/mine/diff, чтобы сравнение было честным), потом экспортирует
// во временный файл и просто перепечатывает его. Так форматирование и
// сама подгонка остаются РОВНО тем кодом, что уже проверен и используется
// в самом приложении -- ничего не переизобретается здесь.
void printReport(const QString &label, digitqt::core::Measurement &measurement) {
  digitqt::core::pipeline::ModalAnalysisStage s5;
  QTextStream out(stdout);
  out << "\n===== " << label << " =====\n";
  if (!s5.compute(measurement)) {
    out << "S5 FAILED: " << s5.errorMessage() << "\n";
    return;
  }

  QTemporaryFile tmp;
  if (!tmp.open()) {
    out << "(could not open temp file for report text)\n";
    return;
  }
  const QString tmpPath = tmp.fileName();
  tmp.close();

  QString err;
  if (!digitqt::core::io::writeModalReport(tmpPath, measurement, err)) {
    out << "writeModalReport FAILED: " << err << "\n";
    return;
  }

  QFile f(tmpPath);
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif
    out << in.readAll();
  }
  QFile::remove(tmpPath);
}

// Общая (общая для обоих файлов) регулярная сетка над [-1,1]x[-1,1],
// усредняющая сэмплы каждой карты по ячейке -- нужна только чтобы
// построить карту РАЗНИЦЫ (сами S5-числа выше считаются на ПОЛНЫХ,
// неусреднённых сэмплах каждой карты по отдельности).
digitqt::core::PhaseMap buildDiffMap(const std::vector<digitqt::core::ApertureSample> &a,
                                     const std::vector<digitqt::core::ApertureSample> &b,
                                     int grid) {
  std::vector<double> sumA(static_cast<size_t>(grid) * grid, 0.0);
  std::vector<double> sumB(static_cast<size_t>(grid) * grid, 0.0);
  std::vector<int> cntA(static_cast<size_t>(grid) * grid, 0);
  std::vector<int> cntB(static_cast<size_t>(grid) * grid, 0);

  auto accumulate = [&](const std::vector<digitqt::core::ApertureSample> &samples,
                       std::vector<double> &sum, std::vector<int> &cnt) {
    for (const auto &s : samples) {
      const int gx = static_cast<int>((s.x + 1.0) / 2.0 * (grid - 1) + 0.5);
      const int gy = static_cast<int>((s.y + 1.0) / 2.0 * (grid - 1) + 0.5);
      if (gx < 0 || gx >= grid || gy < 0 || gy >= grid)
        continue;
      const size_t idx = static_cast<size_t>(gy) * grid + gx;
      sum[idx] += s.z;
      cnt[idx] += 1;
    }
  };
  accumulate(a, sumA, cntA);
  accumulate(b, sumB, cntB);

  digitqt::core::PhaseMap diff(grid, grid);
  for (int gy = 0; gy < grid; ++gy) {
    for (int gx = 0; gx < grid; ++gx) {
      const size_t idx = static_cast<size_t>(gy) * grid + gx;
      if (cntA[idx] == 0 || cntB[idx] == 0)
        continue;
      const double avgA = sumA[idx] / cntA[idx];
      const double avgB = sumB[idx] / cntB[idx];
      diff.setValue(gx, gy, avgB - avgA);
    }
  }
  return diff;
}

void printDiffStats(const digitqt::core::PhaseMap &diff) {
  QTextStream out(stdout);
  const int grid = diff.width();
  const double center = (grid - 1) / 2.0;

  double sumSq = 0.0, sumSqCenter = 0.0, sumSqEdge = 0.0;
  int n = 0, nCenter = 0, nEdge = 0;
  double minV = 0.0, maxV = 0.0;
  bool any = false;

  for (int y = 0; y < grid; ++y) {
    for (int x = 0; x < grid; ++x) {
      if (!diff.hasValue(x, y))
        continue;
      const double v = diff.value(x, y);
      if (!any || v < minV)
        minV = v;
      if (!any || v > maxV)
        maxV = v;
      any = true;
      sumSq += v * v;
      ++n;
      const double r = std::hypot(x - center, y - center) / center;
      if (r < 0.5) {
        sumSqCenter += v * v;
        ++nCenter;
      } else if (r > 0.85) {
        sumSqEdge += v * v;
        ++nEdge;
      }
    }
  }

  out << "\n===== raw diff map (mine - ref), binned " << grid << "x" << grid << " =====\n";
  if (n == 0) {
    out << "no overlapping cells\n";
    return;
  }
  out << "overlapping cells: " << n << " / " << (grid * grid) << "\n";
  out << "min/max: " << minV << " / " << maxV << " nm\n";
  out << "RMS: " << std::sqrt(sumSq / n) << " nm\n";
  if (nCenter > 0)
    out << "center (r<0.5) RMS: " << std::sqrt(sumSqCenter / nCenter) << " nm, n=" << nCenter
        << "\n";
  if (nEdge > 0)
    out << "edge (r>0.85) RMS: " << std::sqrt(sumSqEdge / nEdge) << " nm, n=" << nEdge << "\n";

  out << "\ncoarse ASCII map (nm), downsampled:\n";
  const int cells = 21;
  const int step = std::max(1, grid / cells);
  for (int y = 0; y < grid; y += step) {
    QStringList row;
    for (int x = 0; x < grid; x += step) {
      if (!diff.hasValue(x, y))
        row << QStringLiteral("   .");
      else
        row << QString::number(diff.value(x, y), 'f', 0).rightJustified(4);
    }
    out << row.join(QStringLiteral(" ")) << "\n";
  }
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const QStringList args = QCoreApplication::arguments();

  // Double pass -- ОТДЕЛЬНО для ref и mine, не общий флаг: файл-эталон
  // (нативный экспорт WinFringe) и наш собственный экспорт (через
  // Measurement::phaseMap(), т.е. "сырой" порядок полосы) кодируют
  // высоту по-разному и почти никогда не нуждаются в ОДНОЙ и той же
  // настройке при повторном импорте -- см. ту самую путаницу, которая
  // и привела к появлению этого инструмента.
  QStringList positional;
  double wavelengthNm = 632.8;
  bool refDoublePass = false;
  bool mineDoublePass = false;
  for (int i = 1; i < args.size(); ++i) {
    const QString &a = args[i];
    if (a.startsWith(QStringLiteral("--wavelength="))) {
      wavelengthNm = a.mid(QStringLiteral("--wavelength=").size()).toDouble();
    } else if (a == QStringLiteral("--ref-double-pass")) {
      refDoublePass = true;
    } else if (a == QStringLiteral("--mine-double-pass")) {
      mineDoublePass = true;
    } else {
      positional << a;
    }
  }

  if (positional.size() != 2) {
    QTextStream err(stderr);
    err << "Usage: mtr_diff <reference.mtr> <mine.mtr> [--wavelength=NM] "
           "[--ref-double-pass] [--mine-double-pass]\n";
    return 1;
  }

  const QString refPath = positional[0];
  const QString minePath = positional[1];

  QTextStream out(stdout);
  out << "Wavelength: " << wavelengthNm << " nm, ref double-pass: "
      << (refDoublePass ? "ON" : "OFF")
      << ", mine double-pass: " << (mineDoublePass ? "ON" : "OFF") << "\n";

  Loaded ref, mine;
  QString err;
  if (!loadMtr(refPath, wavelengthNm, refDoublePass, ref, err)) {
    QTextStream(stderr) << "Failed to load " << refPath << ": " << err << "\n";
    return 1;
  }
  if (!loadMtr(minePath, wavelengthNm, mineDoublePass, mine, err)) {
    QTextStream(stderr) << "Failed to load " << minePath << ": " << err << "\n";
    return 1;
  }

  out << "ref (" << refPath << "): aperture center=(" << ref.geometry.centerX << ", "
      << ref.geometry.centerY << ") radius=" << ref.geometry.radius
      << " samples=" << ref.samples.size() << "\n";
  out << "mine(" << minePath << "): aperture center=(" << mine.geometry.centerX << ", "
      << mine.geometry.centerY << ") radius=" << mine.geometry.radius
      << " samples=" << mine.samples.size() << "\n";

  printReport(QStringLiteral("ref: %1").arg(refPath), ref.measurement);
  printReport(QStringLiteral("mine: %1").arg(minePath), mine.measurement);

  constexpr int kGrid = 201;
  digitqt::core::PhaseMap diffMap = buildDiffMap(ref.samples, mine.samples, kGrid);
  printDiffStats(diffMap);

  // Прогоняем саму разницу через настоящий S5 (SequentialSeregin) --
  // раскладывает разницу по тем же термам (наклон/дефокус/астигматизм/
  // кома/трилистник/сферическая), что и остальные два отчёта выше,
  // тем же самым, уже проверенным кодом.
  digitqt::core::Measurement diffMeasurement;
  diffMeasurement.wavefrontMap() = diffMap;
  diffMeasurement.setWavelengthNm(wavelengthNm);
  diffMeasurement.modalFitMethod() = digitqt::core::ModalFitMethod::SequentialSeregin;
  printReport(QStringLiteral("diff (mine - ref), decomposed by S5"), diffMeasurement);

  return 0;
}
