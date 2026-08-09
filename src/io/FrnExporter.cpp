#include "FrnExporter.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/geometry/Polygon.h>
#include <aperture/include/geometry/Rectangle.h>
#include <aperture/include/visibility/ShapeCollection.h>
#include <aperture/include/visibility/VisibilityChecker.h>

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace digitqt::core::io {

namespace {

double distSq(const aperture::Point &a, const aperture::Point &b) {
  const double dx = a.x - b.x, dy = a.y - b.y;
  return dx * dx + dy * dy;
}

struct Circ {
  aperture::Point center{0.0, 0.0};
  double radius = 0.0;
  bool valid = false;
};

bool ptInside(const Circ &c, const aperture::Point &p, double eps) {
  const double r = c.radius + eps;
  return distSq(c.center, p) <= r * r;
}

Circ circleFrom2(const aperture::Point &a, const aperture::Point &b) {
  const aperture::Point center{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
  return {center, std::sqrt(distSq(a, b)) / 2.0, true};
}

Circ circleFrom3(const aperture::Point &a, const aperture::Point &b, const aperture::Point &c) {
  const double d = 2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
  if (std::fabs(d) < 1e-9)
    return circleFrom2(a, b);  // почти коллинеарны -- разумное приближение

  const double a2 = a.x * a.x + a.y * a.y;
  const double b2 = b.x * b.x + b.y * b.y;
  const double c2 = c.x * c.x + c.y * c.y;
  const double ux = (a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d;
  const double uy = (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d;
  const aperture::Point center{ux, uy};
  return {center, std::sqrt(distSq(center, a)), true};
}

Circ trivialCircle(const std::vector<aperture::Point> &b) {
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
/// (не более 3). Тот же алгоритм, что и в MtrExporter.cpp, только над
/// aperture::Point вместо локального Pt -- общей утилиты для этого в
/// кодовой базе нет, каждый экспортёр держит свою копию.
Circ welzl(const std::vector<aperture::Point> &pts, size_t n, std::vector<aperture::Point> boundary) {
  if (n == 0 || boundary.size() == 3)
    return trivialCircle(boundary);

  const aperture::Point &p = pts[n - 1];
  const Circ c = welzl(pts, n - 1, boundary);
  if (c.valid && ptInside(c, p, 1e-7))
    return c;

  boundary.push_back(p);
  return welzl(pts, n - 1, boundary);
}

Circ minEnclosingCircle(std::vector<aperture::Point> pts) {
  if (pts.empty())
    return {};
  std::mt19937 rng(42);  // фиксированный seed, как в оригинальном CalcWinFringeBoundCircle
  std::shuffle(pts.begin(), pts.end(), rng);
  return welzl(pts, pts.size(), {});
}

/// Порт CalcWinFringeBoundCircle: минимальная окружность, охватывающая
/// видимую область апертуры, по контурным точкам всех неповёрнутых
/// EXTERNAL эллипсов/прямоугольников (после фильтрации по фактической
/// видимости -- так INTERNAL-препятствия и пересечение нескольких
/// EXTERNAL-фигур учитываются автоматически).
Circ computeWinFringeBoundCircle(const aperture::ShapeCollection &boundaries) {
  constexpr double kRotationEps = 1e-6;
  constexpr int kContourSamples = 500;

  std::vector<aperture::Point> candidates;
  for (const auto &shape : boundaries.getExternal()) {
    double rotationDeg = 0.0;
    if (const auto *e = dynamic_cast<const aperture::Ellipse *>(shape.get()))
      rotationDeg = e->rotationDegrees();
    else if (const auto *r = dynamic_cast<const aperture::Rectangle *>(shape.get()))
      rotationDeg = r->rotationDegrees();
    else
      continue;  // полигоны в исходном CalcWinFringeBoundCircle не участвуют

    if (std::fabs(rotationDeg) > kRotationEps)
      continue;

    const double perim = shape->perimeter();
    const double step = perim > 0.0 ? std::max(perim / kContourSamples, 0.1) : 1.0;
    for (const auto &p : shape->getContour(step))
      candidates.push_back(p);
  }

  const aperture::VisibilityChecker checker(boundaries);
  std::vector<aperture::Point> visible;
  visible.reserve(candidates.size());
  for (const auto &p : candidates)
    if (checker.isVisible(p))
      visible.push_back(p);

  return minEnclosingCircle(std::move(visible));
}

}  // namespace

bool writeFrnFile(const QString &path, const aperture::ShapeCollection &boundaries,
                  const std::vector<digitqt::core::NumberedFringeLine> &lines,
                  int imageWidth, int imageHeight, const QString &imageFileName,
                  QString &errorMessage) {
  if (lines.empty()) {
    errorMessage = QStringLiteral("No numbered fringe lines to export");
    return false;
  }

  const Circ circle = computeWinFringeBoundCircle(boundaries);
  if (!circle.valid || circle.radius <= 0.0) {
    errorMessage = QStringLiteral("Could not determine the aperture's bounding circle");
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    errorMessage = QStringLiteral("Could not open file for writing: %1").arg(path);
    return false;
  }
  QTextStream os(&file);

  // Несмотря на название параметра, Shape::inverseY(centerY) в этой
  // библиотеке отражает как y' = centerY - y (см. Ellipse/Rectangle/
  // Polygon::inverseY), а не y' = 2*centerY - y, как можно было бы
  // предположить по названию -- значит, чтобы получить обычный переворот
  // изображения y' = H - y, нужно передавать саму высоту H, а не половину.
  const double flipCenterY = imageHeight;
  // Контурные точки для computeWinFringeBoundCircle собирались ДО
  // переворота Y, а сами фигуры при записи переворачиваются -- значит,
  // центр нормализующей окружности тоже нужно перевернуть тем же образом,
  // чтобы совпасть с уже перевёрнутыми фигурами (см. оригинал:
  // "normYc = ImageSize[1] - normYc" сразу после CalcWinFringeBoundCircle).
  const double normXc = circle.center.x;
  const double normYc = static_cast<double>(imageHeight) - circle.center.y;
  const double normRad = circle.radius;

  const QDateTime now = QDateTime::currentDateTime();
  os << "[GENERAL]\n";
  os << "Title=\n";
  os << "Date=" << now.toString(QStringLiteral("dd.MM.yyyy")) << "\n";
  os << "Time=" << now.toString(QStringLiteral("HH:mm:ss")) << "\n";
  os << QString::asprintf("ScaleFactor=%-1.3lf\n", 1.0);
  os << QString::asprintf("FiScan=%-1.2lf\n", 0.0);
  os << "\n";

  // Собираем эллипсы/прямоугольники/полигоны апертуры: EXTERNAL и INTERNAL
  // (обычные для WinFringe апертура/препятствие). APERTURE -- более новая
  // концепция DigitQt, аналога в формате .frn нет, такие фигуры
  // пропускаются.
  std::vector<const aperture::Shape *> apertureShapes;
  for (const auto &s : boundaries.getExternal()) apertureShapes.push_back(s.get());
  for (const auto &s : boundaries.getInternal()) apertureShapes.push_back(s.get());

  auto fileTypeLimits = [](const aperture::Shape *shape) {
    // .frn исторически кодирует обскурацию как 0=internal, 1=external --
    // ровно противоположно текущему aperture::TypeLimits (EXTERNAL=0).
    return shape->getTypeLimits() == aperture::TypeLimits::EXTERNAL ? 1 : 0;
  };
  // Вторая координата (typeSystCoor) в оригинальном формате не
  // задокументирована; во всех реальных .frn-образцах, что удалось найти,
  // стоит 1 -- пишем то же значение.
  constexpr int kTypeSystCoor = 1;

  std::vector<const aperture::Shape *> ellipses, rectangles, polygons;
  for (const auto *shape : apertureShapes) {
    if (dynamic_cast<const aperture::Ellipse *>(shape))
      ellipses.push_back(shape);
    else if (dynamic_cast<const aperture::Rectangle *>(shape))
      rectangles.push_back(shape);
    else if (dynamic_cast<const aperture::Polygon *>(shape))
      polygons.push_back(shape);
  }

  if (!ellipses.empty()) {
    os << "[ELLIPSES]\n";
    for (const auto *shape : ellipses) {
      auto clone = shape->clone();
      clone->inverseY(flipCenterY);
      clone->normalize(normXc, normYc, normRad);
      const auto &e = static_cast<const aperture::Ellipse &>(*clone);
      os << QString::asprintf(" %1.6lf %1.6lf %1.6lf %1.6lf %1.3lf %1d %1d \n", e.semiMajor(),
                              e.semiMinor(), e.center().x, e.center().y, e.rotationDegrees(),
                              fileTypeLimits(shape), kTypeSystCoor);
    }
    os << "END\n\n";
  }

  if (!rectangles.empty()) {
    os << "[RECTANGLES]\n";
    for (const auto *shape : rectangles) {
      auto clone = shape->clone();
      clone->inverseY(flipCenterY);
      clone->normalize(normXc, normYc, normRad);
      const auto &r = static_cast<const aperture::Rectangle &>(*clone);
      os << QString::asprintf(" %1.6lf %1.6lf %1.6lf %1.6lf %1.3lf %1d %1d \n", r.width() / 2.0,
                              r.height() / 2.0, r.center().x, r.center().y, r.rotationDegrees(),
                              fileTypeLimits(shape), kTypeSystCoor);
    }
    os << "END\n\n";
  }

  if (!polygons.empty()) {
    os << "[POLYGONS]\n";
    constexpr int kPointsPerLine = 4;
    for (const auto *shape : polygons) {
      auto clone = shape->clone();
      clone->inverseY(flipCenterY);
      const auto &poly = static_cast<const aperture::Polygon &>(*clone);

      QString line;
      int inLine = 0;
      const size_t n = poly.vertexCount();
      for (size_t i = 0; i < n; ++i) {
        const auto &v = poly.vertex(i);
        if (i == 0) {
          line = QString::asprintf(" %1d %1d %1.3lf %1.3lf", fileTypeLimits(shape), kTypeSystCoor,
                                   v.x, v.y);
          inLine = 1;
        } else if (inLine == 0) {
          line += QString::asprintf("     %1.3lf %1.3lf", v.x, v.y);
          inLine = 1;
        } else {
          line += QString::asprintf(" %1.3lf %1.3lf", v.x, v.y);
          ++inLine;
        }
        if (inLine == kPointsPerLine) {
          os << line << (i == n - 1 ? " E\n" : "\n");
          line.clear();
          inLine = 0;
        }
      }
      if (inLine > 0)
        os << line << " E\n";
    }
    os << "END\n\n";
  }

  // [BOUNDS]: по одной строке на неповёрнутый ellipse/rect, Y перевёрнут,
  // БЕЗ нормализации. Повёрнутые фигуры и внутренние (INTERNAL)
  // прямоугольники WinFringe в bounds не поддерживает -- пропускаются, как
  // и в оригинале.
  os << "[BOUNDS]\n";
  for (const auto *shape : ellipses) {
    const auto &e = static_cast<const aperture::Ellipse &>(*shape);
    if (std::fabs(e.rotationDegrees()) > 1e-6)
      continue;
    auto clone = shape->clone();
    clone->inverseY(flipCenterY);
    const auto bnd = clone->getBounds();
    os << QString::asprintf(" %1.3lf %1.3lf %1.3lf %1.3lf %1d %1d \n", bnd.minX(), bnd.maxX(),
                            bnd.minY(), bnd.maxY(), 1, fileTypeLimits(shape));
  }
  for (const auto *shape : rectangles) {
    const auto &r = static_cast<const aperture::Rectangle &>(*shape);
    if (std::fabs(r.rotationDegrees()) > 1e-6 || shape->getTypeLimits() == aperture::TypeLimits::INTERNAL)
      continue;
    auto clone = shape->clone();
    clone->inverseY(flipCenterY);
    const auto bnd = clone->getBounds();
    os << QString::asprintf(" %1.3lf %1.3lf %1.3lf %1.3lf %1d %1d \n", bnd.minX(), bnd.maxX(),
                            bnd.minY(), bnd.maxY(), 2, fileTypeLimits(shape));
  }
  os << "END\n\n";

  // [FRINGES]: одна пронумерованная линия DigitQt -- один блок NFringe=,
  // без разбиения на под-сегменты (в отличие от оригинала, где несколько
  // кусков одной и той же линии хранились как отдельные "Index" внутри
  // общего плоского списка точек -- в DigitQt каждая ломаная уже своя
  // NumberedFringeLine).
  os << "[FRINGES]\n";
  for (const auto &line : lines) {
    if (line.points.empty())
      continue;
    os << QString::asprintf("NFringe=%1.1lf\n", line.order);
    for (const auto &p : line.points)
      os << QString::asprintf(" %1.3lf %1.3lf\n", p.x, static_cast<double>(imageHeight) - p.y);
    os << "E\n";
  }
  os << "END\n";

  os << "[IMAGE_FILE]\n";
  os << QString::asprintf("Size=%-d %d\n", imageWidth, imageHeight);
  os << "Name=" << imageFileName << "\n";

  os.flush();
  if (file.error() != QFile::NoError) {
    errorMessage = file.errorString();
    return false;
  }
  return true;
}

}  // namespace digitqt::core::io
