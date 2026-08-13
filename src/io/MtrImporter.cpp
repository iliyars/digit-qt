#include "MtrImporter.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <cmath>

namespace digitqt::core::io {

bool readMtrFile(const QString &path, digitqt::core::PhaseMap &outPhase, QString &errorMessage) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    errorMessage = QStringLiteral("Could not open file for reading: %1").arg(path);
    return false;
  }

  QTextStream in(&file);
  const QString all = in.readAll();

  static const QRegularExpression sizeRe(QStringLiteral("^Size=(\\d+)"),
                                         QRegularExpression::MultilineOption);
  const auto sizeMatch = sizeRe.match(all);
  if (!sizeMatch.hasMatch()) {
    errorMessage = QStringLiteral("Missing or invalid Size= header");
    return false;
  }
  const int n = sizeMatch.captured(1).toInt();
  if (n < 3) {
    errorMessage = QStringLiteral("Invalid matrix size: %1").arg(n);
    return false;
  }

  const int matrixTag = all.indexOf(QStringLiteral("[MATRIX]"));
  if (matrixTag < 0) {
    errorMessage = QStringLiteral("Missing [MATRIX] section");
    return false;
  }
  const int dataStart = matrixTag + static_cast<int>(QStringLiteral("[MATRIX]").size());
  const int endTag = all.indexOf(QStringLiteral("END"), dataStart);
  const QString matrixSection =
      endTag >= 0 ? all.mid(dataStart, endTag - dataStart) : all.mid(dataStart);

  // Tokens are: Y (start of a row), then "Z X" pairs, then "E" (end of
  // row) -- regardless of how export wrapped them across physical lines,
  // since we tokenize across the whole section at once.
  static const QRegularExpression wsRe(QStringLiteral("\\s+"));
  const QStringList tokens = matrixSection.split(wsRe, Qt::SkipEmptyParts);

  digitqt::core::PhaseMap phase(n, n);
  const double ratio = 2.0 / static_cast<double>(n - 1);
  const double center = (n - 1) / 2.0;

  bool any = false;
  int i = 0;
  const int count = tokens.size();
  while (i < count) {
    bool yOk = false;
    const double yNorm = tokens[i].toDouble(&yOk);
    if (!yOk) {
      errorMessage = QStringLiteral("Malformed row: expected a Y value at token %1").arg(i);
      return false;
    }
    ++i;
    const int row = static_cast<int>(std::lround(center - yNorm / ratio));

    while (i < count && tokens[i] != QLatin1String("E")) {
      if (i + 1 >= count) {
        errorMessage = QStringLiteral("Truncated row: a Z value is missing its X");
        return false;
      }
      bool zOk = false, xOk = false;
      const double z = tokens[i].toDouble(&zOk);
      const double xNorm = tokens[i + 1].toDouble(&xOk);
      if (!zOk || !xOk) {
        errorMessage = QStringLiteral("Malformed Z/X pair at token %1").arg(i);
        return false;
      }
      i += 2;

      const int col = static_cast<int>(std::lround(xNorm / ratio + center));
      if (col >= 0 && col < n && row >= 0 && row < n) {
        phase.setValue(col, row, z);
        any = true;
      }
    }
    if (i < count)
      ++i;  // skip the "E" row terminator
  }

  if (!any) {
    errorMessage = QStringLiteral("No data points found in the matrix");
    return false;
  }

  outPhase = std::move(phase);
  return true;
}

}  // namespace digitqt::core::io
