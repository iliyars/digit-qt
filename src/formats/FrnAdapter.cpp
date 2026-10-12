#include "FrnAdapter.h"

#include "formats/TextParsing.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/geometry/Polygon.h>
#include <aperture/include/geometry/Rectangle.h>

#include <cmath>
#include <memory>

namespace digitqt::formats {

using text::lineValue;
using text::parseDouble;
using text::Tokenizer;

namespace {

constexpr std::string_view kGeneralTag = "[GENERAL]";

/// Одно расширение -- два диалекта (см. test_data/Digit_frn.frn и
/// FinFringe_FRN.frn, записанные оригинальным Digit для одной и той же
/// интерферограммы). Различаются только записью геометрии:
///   Digit     -- всё в пикселях изображения, Y вниз; секция [IMAGE].
///   WinFringe -- Y перевёрнут (y' = H - y), эллипсы и прямоугольники
///                нормированы на окружность апертуры; секция [IMAGE_FILE].
enum class Dialect { Digit, WinFringe };

// --- Стадия 3: "сырые" данные, числа ровно как записаны в файле ---------

struct RawPoint {
  double x = 0.0;
  double y = 0.0;
};

struct RawFringe {
  double order = 0.0;  ///< значение NFringe=
  std::vector<RawPoint> points;
};

/// Строка [ELLIPSES]/[RECTANGLES]: полуоси (или полуширина/полувысота),
/// центр, поворот, тип ограничения, тип координат.
struct RawBox {
  double halfA = 0.0, halfB = 0.0, xc = 0.0, yc = 0.0, rotationDeg = 0.0;
  bool external = true;     ///< в файле: 1 = external, 0 = internal
  bool normalized = false;  ///< в файле: 0 = пиксели, 1 = нормированные
};

struct RawPolygon {
  bool external = true;
  std::vector<RawPoint> vertices;
};

/// Строка [BOUNDS] диалекта WinFringe: рамка неповёрнутой фигуры в
/// пикселях (Y перевёрнут, без нормировки).
struct RawBound {
  double xMin = 0.0, xMax = 0.0, yMin = 0.0, yMax = 0.0;
  bool isEllipse = true;  ///< в файле: 1 = эллипс, 2 = прямоугольник
};

/// Окружность, на которую WinFringe нормирует фигуры (в перевёрнутых
/// по Y пикселях). В файле её нет -- см. deriveNormCircle().
struct NormCircle {
  double xc = 0.0, yc = 0.0, radius = 0.0;
};

// --- Стадии 1 и 3: структура текста и разбор секций ---------------------

/// Тело секции: текст от строки с её заголовком до следующей строки,
/// начинающейся с '[' (или до конца файла). nullopt, если секции нет.
std::optional<std::string_view> sectionBody(std::string_view data, std::string_view tag) {
  size_t pos = 0;
  while ((pos = data.find(tag, pos)) != std::string_view::npos) {
    if (pos == 0 || data[pos - 1] == '\n') {
      const size_t start = pos + tag.size();
      const size_t end = data.find("\n[", start);
      return data.substr(start, end == std::string_view::npos ? end : end - start);
    }
    pos += tag.size();
  }
  return std::nullopt;
}

/// Читает count чисел подряд. false, если токены кончились или попалось
/// не число.
bool nextNumbers(Tokenizer &tokens, double *values, int count) {
  std::string_view token;
  for (int i = 0; i < count; ++i)
    if (!tokens.next(token) || !parseDouble(token, values[i]))
      return false;
  return true;
}

/// [ELLIPSES] и [RECTANGLES] устроены одинаково: по 7 чисел на фигуру,
/// END в конце.
bool parseBoxes(std::string_view body, std::string_view sectionName, std::vector<RawBox> &out,
                std::string &error) {
  Tokenizer tokens(body);
  std::string_view token;
  while (tokens.next(token) && token != "END") {
    double v[7] = {};
    if (!parseDouble(token, v[0]) || !nextNumbers(tokens, v + 1, 6)) {
      error = "Malformed line in " + std::string(sectionName);
      return false;
    }
    out.push_back({v[0], v[1], v[2], v[3], v[4], v[5] != 0.0, v[6] != 0.0});
  }
  return true;
}

/// [POLYGONS]: "тип_ограничения тип_координат x y x y ... E" на полигон
/// (с переносами строк где угодно), END в конце.
bool parsePolygons(std::string_view body, std::vector<RawPolygon> &out, std::string &error) {
  Tokenizer tokens(body);
  std::string_view token;
  while (tokens.next(token) && token != "END") {
    double flags[2] = {};
    if (!parseDouble(token, flags[0]) || !nextNumbers(tokens, flags + 1, 1)) {
      error = "Malformed polygon header in [POLYGONS]";
      return false;
    }
    RawPolygon polygon;
    polygon.external = flags[0] != 0.0;
    while (tokens.next(token) && token != "E") {
      RawPoint p;
      if (!parseDouble(token, p.x) || !nextNumbers(tokens, &p.y, 1)) {
        error = "Malformed vertex in [POLYGONS]";
        return false;
      }
      polygon.vertices.push_back(p);
    }
    out.push_back(std::move(polygon));
  }
  return true;
}

/// [BOUNDS] диалекта WinFringe: "xmin xmax ymin ymax тип_фигуры
/// тип_ограничения" на строку, END в конце. (У диалекта Digit формат
/// другой -- одна строка "xmin ymin xmax ymax E" -- и он не читается:
/// там фигуры и так в пикселях.)
bool parseWinFringeBounds(std::string_view body, std::vector<RawBound> &out, std::string &error) {
  Tokenizer tokens(body);
  std::string_view token;
  while (tokens.next(token) && token != "END") {
    double v[6] = {};
    if (!parseDouble(token, v[0]) || !nextNumbers(tokens, v + 1, 5)) {
      error = "Malformed line in [BOUNDS]";
      return false;
    }
    out.push_back({v[0], v[1], v[2], v[3], v[4] == 1.0});
  }
  return true;
}

/**
 * @brief Разбирает тело секции [FRINGES] в "сырые" полосы -- числа ровно
 * как в файле, без переворота Y (его делает importData(), на стадии 4).
 *
 * Тело выглядит так (одинаково в обоих диалектах):
 *
 *   NFringe=0.0
 *    5.072 563.000
 *    11.167 572.000
 *   E
 *   NFringe=1.0
 *    ...
 *   E
 *   END
 *
 * @param body Текст секции после строки "[FRINGES]".
 * @param out Сюда добавляются полосы, в порядке следования в файле.
 * @param error Заполняется при неудаче.
 * @return false, если текст повреждён.
 */
bool parseFringes(std::string_view body, std::vector<RawFringe> &out, std::string &error) {
  constexpr std::string_view kOrderKey = "NFringe=";

  Tokenizer tokens(body);
  std::string_view token;
  while (tokens.next(token) && token != "END") {
    RawFringe fringe;
    if (token.substr(0, kOrderKey.size()) != kOrderKey ||
        !parseDouble(token.substr(kOrderKey.size()), fringe.order)) {
      error = "Malformed fringe header in [FRINGES]: expected NFringe=<number>";
      return false;
    }
    while (tokens.next(token) && token != "E") {
      RawPoint p;
      if (!parseDouble(token, p.x) || !nextNumbers(tokens, &p.y, 1)) {
        error = "Malformed point in [FRINGES]";
        return false;
      }
      fringe.points.push_back(p);
    }
    out.push_back(std::move(fringe));
  }
  return true;
}

// --- Стадия 4: перевод сырых данных в модель ----------------------------

aperture::TypeLimits limitsOf(bool external) {
  return external ? aperture::TypeLimits::EXTERNAL : aperture::TypeLimits::INTERNAL;
}

bool isUnrotated(const RawBox &box) {
  return std::fabs(box.rotationDeg) < 1e-6;
}

/**
 * @brief Восстанавливает окружность нормировки WinFringe из самого файла.
 *
 * Неповёрнутая фигура записана дважды: нормированной в [ELLIPSES]/
 * [RECTANGLES] и рамкой в пикселях в [BOUNDS] (k-я неповёрнутая фигура
 * своего вида <-> k-я строка [BOUNDS] того же вида). Одной такой пары
 * достаточно: radius = полуширина_в_пикселях / нормированная_полуширина,
 * центр -- из того же соотношения для центра фигуры.
 *
 * Внутренние прямоугольники в [BOUNDS] не пишутся (см. FrnExporter.cpp),
 * поэтому в счёте "k-й" они не участвуют.
 */
std::optional<NormCircle> deriveNormCircle(const std::vector<RawBox> &boxes, bool ellipses,
                                           const std::vector<RawBound> &bounds) {
  size_t boundIndex = 0;
  for (const RawBox &box : boxes) {
    if (!box.normalized || !isUnrotated(box) || (!ellipses && !box.external))
      continue;
    while (boundIndex < bounds.size() && bounds[boundIndex].isEllipse != ellipses)
      ++boundIndex;
    if (boundIndex >= bounds.size())
      return std::nullopt;
    const RawBound &bound = bounds[boundIndex++];

    const double halfWidthPx = (bound.xMax - bound.xMin) / 2.0;
    if (box.halfA <= 0.0 || halfWidthPx <= 0.0)
      continue;
    NormCircle circle;
    circle.radius = halfWidthPx / box.halfA;
    circle.xc = (bound.xMin + bound.xMax) / 2.0 - box.xc * circle.radius;
    circle.yc = (bound.yMin + bound.yMax) / 2.0 - box.yc * circle.radius;
    return circle;
  }
  return std::nullopt;
}

}  // namespace

std::string FrnAdapter::displayName() const {
  return "WinFringe Interferogram";
}

std::vector<std::string> FrnAdapter::extensions() const {
  return {"frn"};
}

bool FrnAdapter::probe(std::string_view head) const {
  return lineValue(head, "FiScan=").has_value() && lineValue(head, kGeneralTag).has_value();
}

bool FrnAdapter::importData(std::string_view data, ImportedData &out, std::string &error) const {
  // Стадия 2: диалект. В самих точках полос признака нет (отличается
  // только соглашение о направлении Y), поэтому он определяется по имени
  // секции с описанием изображения -- оттуда же берётся высота для
  // переворота Y.
  Dialect dialect = Dialect::Digit;
  std::optional<std::string_view> image = sectionBody(data, "[IMAGE]");
  std::string_view nameKey = "FileName=";
  if (!image) {
    image = sectionBody(data, "[IMAGE_FILE]");
    nameKey = "Name=";
    dialect = Dialect::WinFringe;
  }
  if (!image) {
    error = "Neither [IMAGE] nor [IMAGE_FILE] section found: cannot tell the Digit and WinFringe "
            "dialects apart";
    return false;
  }

  double size[2] = {};
  bool hasSize = false;
  if (const auto sizeValue = lineValue(*image, "Size=")) {
    Tokenizer sizeTokens(*sizeValue);
    hasSize = nextNumbers(sizeTokens, size, 2) && size[0] > 0.0 && size[1] > 0.0;
  }
  if (dialect == Dialect::WinFringe && !hasSize) {
    error = "Missing or invalid Size= in [IMAGE_FILE]: the image height is needed to flip Y back";
    return false;
  }
  const double imageHeight = size[1];
  const bool flipY = dialect == Dialect::WinFringe;

  // Стадия 3: секции -> сырые данные.
  std::vector<RawBox> ellipses, rectangles;
  std::vector<RawPolygon> polygons;
  std::vector<RawBound> bounds;
  std::vector<RawFringe> fringes;
  if (const auto body = sectionBody(data, "[ELLIPSES]"))
    if (!parseBoxes(*body, "[ELLIPSES]", ellipses, error))
      return false;
  if (const auto body = sectionBody(data, "[RECTANGLES]"))
    if (!parseBoxes(*body, "[RECTANGLES]", rectangles, error))
      return false;
  if (const auto body = sectionBody(data, "[POLYGONS]"))
    if (!parsePolygons(*body, polygons, error))
      return false;
  if (dialect == Dialect::WinFringe)
    if (const auto body = sectionBody(data, "[BOUNDS]"))
      if (!parseWinFringeBounds(*body, bounds, error))
        return false;
  if (const auto body = sectionBody(data, "[FRINGES]"))
    if (!parseFringes(*body, fringes, error))
      return false;

  if (ellipses.empty() && rectangles.empty() && polygons.empty() && fringes.empty()) {
    error = "No aperture shapes and no fringes found";
    return false;
  }

  // Стадия 4: сырые данные -> модель (пиксели изображения, Y вниз).
  std::optional<NormCircle> circle = deriveNormCircle(ellipses, true, bounds);
  if (!circle)
    circle = deriveNormCircle(rectangles, false, bounds);

  aperture::ShapeCollection shapes;
  int denormalized = 0, droppedNormalized = 0;

  // Обратный порядок к FrnExporter.cpp (там: inverseY, потом normalize).
  auto addBox = [&](const RawBox &box, bool isEllipse) {
    if (box.normalized && !circle) {
      ++droppedNormalized;
      return;
    }
    std::unique_ptr<aperture::Shape> shape;
    if (isEllipse)
      shape = std::make_unique<aperture::Ellipse>(box.halfA, box.halfB, box.xc, box.yc,
                                                  box.rotationDeg, limitsOf(box.external));
    else
      shape = std::make_unique<aperture::Rectangle>(box.halfA * 2.0, box.halfB * 2.0, box.xc, box.yc,
                                                    box.rotationDeg, limitsOf(box.external));
    if (box.normalized) {
      shape->denormalize(circle->xc, circle->yc, circle->radius);
      ++denormalized;
    }
    if (flipY)
      shape->inverseY(imageHeight);  // y' = H - y, см. комментарий в FrnExporter.cpp
    shapes.addShape(std::move(shape));
  };
  for (const RawBox &box : ellipses)
    addBox(box, true);
  for (const RawBox &box : rectangles)
    addBox(box, false);

  // Полигоны не нормируются ни в одном диалекте (несмотря на флаг типа
  // координат 1, который пишет FrnExporter.cpp) -- только переворот Y.
  for (const RawPolygon &polygon : polygons) {
    std::vector<aperture::Point> vertices;
    vertices.reserve(polygon.vertices.size());
    for (const RawPoint &p : polygon.vertices)
      vertices.push_back({p.x, flipY ? imageHeight - p.y : p.y});
    shapes.addShape(std::make_unique<aperture::Polygon>(vertices, limitsOf(polygon.external)));
  }

  for (const RawFringe &fringe : fringes) {
    if (fringe.points.empty())
      continue;
    core::NumberedFringeLine line;
    line.order = fringe.order;
    // Номер из файла -- это решение автора файла, а не наша автонумерация:
    // autoAssignFringeOrder() не должен его перезаписать.
    line.orderIsManual = true;
    line.points.reserve(fringe.points.size());
    for (const RawPoint &p : fringe.points) {
      core::tracing::TracedPoint point;
      point.x = p.x;
      point.y = flipY ? imageHeight - p.y : p.y;
      line.points.push_back(point);
    }
    out.fringeLines.push_back(std::move(line));
  }

  // Отчёт.
  if (flipY)
    out.report.push_back({Fidelity::Converted, "WinFringe dialect: Y axis flipped back using image height " +
                                                   std::to_string(static_cast<int>(imageHeight))});
  else
    out.report.push_back({Fidelity::Lossless, "Digit dialect: pixel coordinates taken unchanged"});
  if (denormalized > 0)
    out.report.push_back(
        {Fidelity::Converted, std::to_string(denormalized) +
                                  " normalized shape(s) scaled back to pixels; the normalization "
                                  "circle was derived from [BOUNDS] (radius " +
                                  std::to_string(circle->radius) + ")"});
  if (droppedNormalized > 0)
    out.report.push_back(
        {Fidelity::Dropped, std::to_string(droppedNormalized) +
                                " normalized shape(s): no unrotated shape in [BOUNDS] to derive the "
                                "normalization circle from"});
  if (!out.fringeLines.empty())
    out.report.push_back({Fidelity::Missing, "Fringe width and intensity are not stored in .frn"});
  if (!hasSize)
    out.report.push_back({Fidelity::Missing, "No image Size=; the image size is unknown"});

  if (const auto general = sectionBody(data, kGeneralTag)) {
    out.report.push_back({Fidelity::Dropped, "Title/Date/Time headers are not kept"});
    double scaleFactor = 1.0, fiScan = 0.0;
    const auto scaleValue = lineValue(*general, "ScaleFactor=");
    const auto fiScanValue = lineValue(*general, "FiScan=");
    if (scaleValue && parseDouble(*scaleValue, scaleFactor) && scaleFactor != 1.0)
      out.report.push_back({Fidelity::Dropped, "ScaleFactor=" + std::string(*scaleValue) + " is ignored"});
    if (fiScanValue && parseDouble(*fiScanValue, fiScan) && fiScan != 0.0)
      out.report.push_back({Fidelity::Dropped, "FiScan=" + std::string(*fiScanValue) + " is ignored"});
  }

  if (!shapes.isEmpty())
    out.boundaries = std::move(shapes);
  if (const auto name = lineValue(*image, nameKey))
    out.imageFileName = std::string(*name);
  out.imageWidth = static_cast<int>(size[0]);
  out.imageHeight = static_cast<int>(size[1]);
  return true;
}

}  // namespace digitqt::formats
