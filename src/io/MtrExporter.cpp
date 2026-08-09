#include "MtrExporter.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace digitqt::core::io {

namespace {

struct Pt {
  double x, y;
};

double dist(const Pt &a, const Pt &b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

struct Circ {
  Pt center{0.0, 0.0};
  double radius = 0.0;
  bool valid = false;
};

bool ptInside(const Circ &c, const Pt &p, double eps) {
  return dist(c.center, p) <= c.radius + eps;
}

Circ circleFrom2(const Pt &a, const Pt &b) {
  const Pt center{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
  return {center, dist(a, b) / 2.0, true};
}

Circ circleFrom3(const Pt &a, const Pt &b, const Pt &c) {
  const double d = 2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
  if (std::fabs(d) < 1e-9)
    return circleFrom2(a, b);  // почти коллинеарны -- разумное приближение

  const double a2 = a.x * a.x + a.y * a.y;
  const double b2 = b.x * b.x + b.y * b.y;
  const double c2 = c.x * c.x + c.y * c.y;
  const double ux = (a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d;
  const double uy = (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d;
  const Pt center{ux, uy};
  return {center, dist(center, a), true};
}

Circ trivialCircle(const std::vector<Pt> &b) {
  if (b.empty())
    return {};
  if (b.size() == 1)
    return {b[0], 0.0, true};
  if (b.size() == 2)
    return circleFrom2(b[0], b[1]);

  Circ c = circleFrom2(b[0], b[1]);
  if (ptInside(c, b[2], 1e-7))
    return c;
  c = circleFrom2(b[0], b[2]);
  if (ptInside(c, b[1], 1e-7))
    return c;
  c = circleFrom2(b[1], b[2]);
  if (ptInside(c, b[0], 1e-7))
    return c;
  return circleFrom3(b[0], b[1], b[2]);
}

/// Классический алгоритм Уэлзла: минимальная окружность, охватывающая
/// первые n точек pts, с известными граничными точками boundary
/// (не более 3).
Circ welzl(const std::vector<Pt> &pts, size_t n, std::vector<Pt> boundary) {
  if (n == 0 || boundary.size() == 3)
    return trivialCircle(boundary);

  const Pt &p = pts[n - 1];
  const Circ c = welzl(pts, n - 1, boundary);
  if (c.valid && ptInside(c, p, 1e-7))
    return c;

  boundary.push_back(p);
  return welzl(pts, n - 1, boundary);
}

Circ minEnclosingCircle(std::vector<Pt> pts) {
  if (pts.empty())
    return {};
  std::mt19937 rng(12345);  // фиксированный seed -- результат от него не зависит, только скорость
  std::shuffle(pts.begin(), pts.end(), rng);
  return welzl(pts, pts.size(), {});
}

}  // namespace

bool writeMtrFile(const QString &path, const digitqt::core::PhaseMap &waves,
                  QString &errorMessage) {
  if (waves.isEmpty()) {
    errorMessage = QStringLiteral("Empty wavefront map");
    return false;
  }

  const int cols = waves.width();
  const int rows = waves.height();

  // Граничные точки видимых данных -- крайний левый/правый видимый
  // столбец в каждой строке (не все пиксели маски), ровно как в
  // оригинальном computeMaskBoundingCircle().
  std::vector<Pt> boundaryPts;
  boundaryPts.reserve(static_cast<size_t>(rows) * 2);
  for (int row = 0; row < rows; ++row) {
    int first = -1, last = -1;
    for (int col = 0; col < cols; ++col) {
      if (!waves.hasValue(col, row))
        continue;
      if (first < 0)
        first = col;
      last = col;
    }
    if (first < 0)
      continue;
    boundaryPts.push_back({static_cast<double>(first), static_cast<double>(row)});
    if (last != first)
      boundaryPts.push_back({static_cast<double>(last), static_cast<double>(row)});
  }

  if (boundaryPts.empty()) {
    errorMessage = QStringLiteral("No valid data in the wavefront map");
    return false;
  }

  const Circ circle = minEnclosingCircle(boundaryPts);
  if (!circle.valid || circle.radius <= 0.0) {
    errorMessage = QStringLiteral("Could not determine the aperture's bounding circle");
    return false;
  }

  // Use bounding circle diameter as matrix size; force odd for symmetry
  // (matches the original's "sizeMatrix |= 1").
  size_t sizeMatrix = static_cast<size_t>(circle.radius * 2.0);
  sizeMatrix |= 1;
  const double ratio = 2.0 / static_cast<double>(sizeMatrix - 1);
  const int xc = static_cast<int>(circle.center.x);
  const int yc = static_cast<int>(circle.center.y);

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    errorMessage = QStringLiteral("Could not open file for writing: %1").arg(path);
    return false;
  }

  QTextStream os(&file);
  os.setRealNumberNotation(QTextStream::FixedNotation);
  os.setRealNumberPrecision(4);

  const QDateTime now = QDateTime::currentDateTime();
  os << "Title=\n";
  os << "Date=" << now.toString(QStringLiteral("yyyy-MM-dd")) << "\n";
  os << "Time=" << now.toString(QStringLiteral("HH:mm:ss")) << "\n";
  os << "Units=WAV\n\n";
  os << "Size=" << static_cast<qulonglong>(sizeMatrix) << "\n";
  os << "[MATRIX]\n";

  constexpr int kPairsPerLine = 6;
  const QString indent(7, QChar(' '));

  for (int row = 0; row < rows; ++row) {
    // Инвертируем Y: карта хранит строки в экранной системе (Y растёт
    // вниз), а .mtr/WinFringe ожидает математическую (Y растёт вверх) --
    // так же, как оригинальный Digit явно конвертирует SCREEN->MATH перед
    // экспортом (WavefrontFromContoursContext). Без этого TiltY попадает
    // в файл с обратным знаком.
    const double yNorm = (yc - row) * ratio;
    if (yNorm > 1.0 || yNorm < -1.0)
      continue;  // за пределами -1..1 ломает расчёты WinFringe -- пропускаем строку целиком

    bool firstLineOfRow = true;
    int pairCount = 0;
    int rowCount = 0;

    for (int col = 0; col < cols; ++col) {
      if (!waves.hasValue(col, row))
        continue;
      const double z = waves.value(col, row);
      if (std::isnan(z) || std::isinf(z))
        continue;

      const double xNorm = (col - xc) * ratio;
      if (xNorm > 1.0 || xNorm < -1.0)
        continue;

      if (pairCount == 0) {
        if (firstLineOfRow) {
          os << yNorm;
          firstLineOfRow = false;
        } else {
          os << "\n" << indent;
        }
      }

      os << " " << z << " " << xNorm;
      ++pairCount;
      ++rowCount;

      if (pairCount == kPairsPerLine && col < cols - 1)
        pairCount = 0;
    }

    if (rowCount != 0)
      os << " E\n";
  }

  os << "END\n";
  os.flush();

  if (file.error() != QFile::NoError) {
    errorMessage = file.errorString();
    return false;
  }

  return true;
}

}  // namespace digitqt::core::io
