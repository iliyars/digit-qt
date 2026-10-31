#include "ScanlineExtremumTracker.h"

#include "FringeConstructor.h"
#include "RedCenterDetector.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>

namespace scanline_extremum_plugin {

namespace {

// Порт из core::tracing::ScanlineExtremumTracker.cpp -- см. там же
// объяснение (легаси CreateZAPSections()/PutDotsOnZAPSections() не
// эмитили точку на каждую сканированную строку, а RedCenterDetector/
// FringeConstructor здесь эмитят; децимация повторяет плотность легаси).
int minRowGapForDecimation(int fringeCount, int imageWidth, int imageHeight) {
  if (fringeCount < 1)
    fringeCount = 1;
  if (imageWidth < 1 || imageHeight < 1)
    return 1;
  const double nSections = std::max(2.0, 2.0 * fringeCount * imageHeight / imageWidth);
  const int gap = static_cast<int>(std::ceil(imageHeight / (nSections - 1.0)));
  return std::max(gap, 1);
}

TracedLine decimateLine(const TracedLine &line, int minRowGap) {
  if (minRowGap <= 1 || line.size() < 3)
    return line;

  TracedLine out;
  out.reserve(line.size() / static_cast<size_t>(minRowGap) + 2);
  out.push_back(line.front());
  double lastKeptY = line.front().y;
  for (size_t i = 1; i + 1 < line.size(); ++i) {
    if (std::abs(line[i].y - lastKeptY) >= minRowGap) {
      out.push_back(line[i]);
      lastKeptY = line[i].y;
    }
  }
  out.push_back(line.back());
  return out;
}

using Visibility = std::function<bool(int, int)>;

// Сепарабельный гаусс только по видимым точкам (нормированная свёртка:
// blur(I*M) / blur(M)): чёрный фон за апертурой не подмешивается к краевым
// полосам и не сдвигает их экстремумы. Невидимые точки остаются как были.
std::vector<uint8_t> gaussianBlur(const uint8_t *src, int width, int height, double sigma,
                                  const Visibility &isVisible) {
  const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
  std::vector<double> kernel(static_cast<size_t>(2 * radius + 1));
  for (int k = -radius; k <= radius; ++k)
    kernel[static_cast<size_t>(k + radius)] = std::exp(-0.5 * k * k / (sigma * sigma));

  const size_t n = static_cast<size_t>(width) * height;
  std::vector<double> value(n), weight(n);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t i = static_cast<size_t>(y) * width + x;
      const bool visible = isVisible(x, y);
      value[i] = visible ? src[i] : 0.0;
      weight[i] = visible ? 1.0 : 0.0;
    }
  }
  // Проход по одной оси для пары (value, weight).
  auto pass = [&](bool horizontal) {
    std::vector<double> v2(n, 0.0), w2(n, 0.0);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        double accV = 0.0, accW = 0.0;
        for (int k = -radius; k <= radius; ++k) {
          const int xx = horizontal ? x + k : x;
          const int yy = horizontal ? y : y + k;
          if (xx < 0 || xx >= width || yy < 0 || yy >= height)
            continue;
          const size_t j = static_cast<size_t>(yy) * width + xx;
          const double kv = kernel[static_cast<size_t>(k + radius)];
          accV += kv * value[j];
          accW += kv * weight[j];
        }
        const size_t i = static_cast<size_t>(y) * width + x;
        v2[i] = accV;
        w2[i] = accW;
      }
    }
    value.swap(v2);
    weight.swap(w2);
  };
  pass(true);
  pass(false);

  std::vector<uint8_t> out(src, src + n);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t i = static_cast<size_t>(y) * width + x;
      if (isVisible(x, y) && weight[i] > 1e-9)
        out[i] = static_cast<uint8_t>(std::clamp(std::lround(value[i] / weight[i]), 0L, 255L));
    }
  }
  return out;
}

std::vector<uint8_t> transposed(const uint8_t *src, int width, int height) {
  std::vector<uint8_t> out(static_cast<size_t>(width) * height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      out[static_cast<size_t>(x) * height + y] = src[static_cast<size_t>(y) * width + x];
  return out;
}

// Полосы скорее горизонтальны, если яркость сильнее меняется по Y, чем по
// X (сумма квадратов центральных разностей внутри апертуры).
bool fringesMostlyHorizontal(const uint8_t *img, int width, int height,
                             const Visibility &isVisible) {
  double sxx = 0.0;
  double syy = 0.0;
  for (int y = 1; y + 1 < height; ++y) {
    for (int x = 1; x + 1 < width; ++x) {
      if (!isVisible(x - 1, y) || !isVisible(x + 1, y) || !isVisible(x, y - 1) ||
          !isVisible(x, y + 1))
        continue;
      const double gx = double(img[static_cast<size_t>(y) * width + x + 1]) -
                        img[static_cast<size_t>(y) * width + x - 1];
      const double gy = double(img[static_cast<size_t>(y + 1) * width + x]) -
                        img[static_cast<size_t>(y - 1) * width + x];
      sxx += gx * gx;
      syy += gy * gy;
    }
  }
  return syy > sxx;
}

// Период полос вдоль строки (пиксели) по средней автокорреляции нескольких
// строк: первый отрицательный локальный минимум -- полпериода. Шум
// камеры даёт вклад почти только в нулевой сдвиг. 0 -- не удалось.
double estimateFringePeriod(const uint8_t *img, int width, int height,
                            const Visibility &isVisible) {
  constexpr int kLines = 32;
  const int maxLag = std::min(width / 2, 400);
  if (maxLag < 4)
    return 0.0;
  std::vector<double> ac(static_cast<size_t>(maxLag + 1), 0.0);
  std::vector<double> count(ac.size(), 0.0);
  for (int k = 1; k <= kLines; ++k) {
    const int y = height * k / (kLines + 1);
    // Самый длинный видимый отрезок строки.
    int bestStart = 0, bestLen = 0;
    for (int x = 0; x < width;) {
      if (!isVisible(x, y)) {
        ++x;
        continue;
      }
      const int start = x;
      while (x < width && isVisible(x, y))
        ++x;
      if (x - start > bestLen) {
        bestLen = x - start;
        bestStart = start;
      }
    }
    if (bestLen < 32)
      continue;
    const uint8_t *row = img + static_cast<size_t>(y) * width + bestStart;
    double mean = 0.0;
    for (int i = 0; i < bestLen; ++i)
      mean += row[i];
    mean /= bestLen;
    const int lags = std::min(maxLag, bestLen / 2);
    for (int lag = 0; lag <= lags; ++lag) {
      double acc = 0.0;
      for (int i = 0; i + lag < bestLen; ++i)
        acc += (row[i] - mean) * (row[i + lag] - mean);
      ac[static_cast<size_t>(lag)] += acc / (bestLen - lag);
      count[static_cast<size_t>(lag)] += 1.0;
    }
  }
  for (size_t i = 0; i < ac.size(); ++i)
    if (count[i] > 0.0)
      ac[i] /= count[i];
  for (int lag = 2; lag < maxLag; ++lag) {
    if (count[static_cast<size_t>(lag + 1)] <= 0.0)
      break;
    const double v = ac[static_cast<size_t>(lag)];
    if (v < 0.0 && v <= ac[static_cast<size_t>(lag - 1)] && v <= ac[static_cast<size_t>(lag + 1)])
      return 2.0 * lag;
  }
  return 0.0;
}

// Перенумерация полос по соседству. FringeConstructor даёт новой цепочке,
// появившейся левее/правее всех, номер "глобальный минимум/максимум ± шаг"
// -- шумовая цепочка с краю (даже выброшенная потом как обрывок) сдвигает
// номера всех следующих на шаг. Здесь на каждой строке сканирования
// соседние по X линии голосуют за "правая = левая + шаг" (если промежуток
// не больше 1.6 медианного -- иначе между ними могла пропасть полоса), и
// номера раздаются от самой длинной линии по связям с наибольшим числом
// голосов. Линии, не связанные с ней, сохраняют прежние номера.
void renumberByAdjacency(std::vector<NumberedFringe> &fringes, int workHeight, double fringeStep) {
  const size_t n = fringes.size();
  if (n < 2 || workHeight <= 0)
    return;

  std::vector<std::vector<std::pair<double, size_t>>> rows(static_cast<size_t>(workHeight));
  for (size_t i = 0; i < n; ++i) {
    for (const auto &p : fringes[i].points) {
      const long y = std::lround(p.y);
      if (y >= 0 && y < workHeight)
        rows[static_cast<size_t>(y)].push_back({p.x, i});
    }
  }

  // votes[{a, b}] -- сколько строк говорят, что b идёт сразу после a.
  std::map<std::pair<size_t, size_t>, int> votes;
  for (auto &row : rows) {
    if (row.size() < 2)
      continue;
    std::sort(row.begin(), row.end());
    std::vector<double> gaps;
    for (size_t k = 1; k < row.size(); ++k)
      gaps.push_back(row[k].first - row[k - 1].first);
    std::vector<double> sorted = gaps;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double median = sorted[sorted.size() / 2];
    for (size_t k = 1; k < row.size(); ++k) {
      if (row[k].second == row[k - 1].second || gaps[k - 1] > 1.6 * median)
        continue;
      ++votes[{row[k - 1].second, row[k].second}];
    }
  }

  size_t anchor = 0;
  for (size_t i = 1; i < n; ++i)
    if (fringes[i].points.size() > fringes[anchor].points.size())
      anchor = i;

  // Жадно, как алгоритм Прима: каждый раз берётся связь с наибольшим
  // числом голосов между пронумерованной и ещё не пронумерованной линией.
  std::vector<bool> assigned(n, false);
  assigned[anchor] = true;
  for (;;) {
    int bestVotes = 0;
    size_t bestNew = 0;
    double bestNumber = 0.0;
    for (const auto &[edge, count] : votes) {
      const auto [a, b] = edge;
      if (count <= bestVotes || assigned[a] == assigned[b])
        continue;
      bestVotes = count;
      bestNew = assigned[a] ? b : a;
      bestNumber = assigned[a] ? fringes[a].number + fringeStep : fringes[b].number - fringeStep;
    }
    if (bestVotes == 0)
      break;
    fringes[bestNew].number = bestNumber;
    assigned[bestNew] = true;
  }
}

// Срез краевой полосы: концевые точки линии, попавшие в полосу у края
// апертуры, отрезаются (потом extendToApertureEdge() продлит линию до края
// по прямой), а линия, у которой в полосе больше половины точек, -- это
// светящийся обод, а не полоса, и выбрасывается целиком.
void trimEdgeBand(std::vector<NumberedFringe> &fringes, const Visibility &inner) {
  auto isInner = [&](const Point2d &p) {
    return inner(static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y)));
  };
  for (auto &fringe : fringes) {
    auto &pts = fringe.points;
    const auto innerCount = std::count_if(pts.begin(), pts.end(), isInner);
    if (2 * static_cast<size_t>(innerCount) < pts.size()) {
      pts.clear();
      continue;
    }
    const auto firstInner = std::find_if(pts.begin(), pts.end(), isInner);
    pts.erase(pts.begin(), firstInner);
    while (!pts.empty() && !isInner(pts.back()))
      pts.pop_back();
  }
  fringes.erase(std::remove_if(fringes.begin(), fringes.end(),
                               [](const NumberedFringe &f) { return f.points.empty(); }),
                fringes.end());
}

// Срез "крючков" на концах: в рабочей ориентации полоса идёт вдоль y (по
// точке на строку сканирования), и конец, где на участке ~6 строк x
// меняется сильнее y, -- это линия, свернувшая вдоль обода или на соседнюю
// полосу, а не сама полоса. Только у края (вне nearEdgeFree -- полоса
// в два отступа): глубже такой изгиб бывает у настоящих полос (дефокус).
void trimHooks(std::vector<NumberedFringe> &fringes, const Visibility &nearEdgeFree) {
  auto nearEdge = [&](const Point2d &p) {
    return !nearEdgeFree(static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y)));
  };
  constexpr double kWindow = 6.0;
  auto hookAt = [](const std::vector<Point2d> &pts, size_t end, int dir) {
    // Ближайшая точка не ближе kWindow строк от конца (dir: +1 вперёд, -1 назад).
    for (size_t k = 1; k < pts.size(); ++k) {
      const size_t j = dir > 0 ? end + k : end - k;
      if (j >= pts.size())
        return false;
      const double dy = std::abs(pts[j].y - pts[end].y);
      if (dy >= kWindow)
        return std::abs(pts[j].x - pts[end].x) > dy;
      if (dir < 0 && j == 0)
        return false;
    }
    return false;
  };
  for (auto &fringe : fringes) {
    auto &pts = fringe.points;
    while (pts.size() > 2 && nearEdge(pts.front()) && hookAt(pts, 0, +1))
      pts.erase(pts.begin());
    while (pts.size() > 2 && nearEdge(pts.back()) && hookAt(pts, pts.size() - 1, -1))
      pts.pop_back();
  }
}

// Продление концов линий до края апертуры по прямой; направление -- по
// участку длиной baseline строк от конца. HorizontalSpline-фаза у края
// круга опирается на строки, пересекающие хотя бы две линии: укороченные
// (срезанные у обода) линии оставляют у верхнего/нижнего края полосы
// строк, которые заполняются грубым копированием по вертикали. Точки
// линии идут по строкам сканирования (y возрастает).
void extendToApertureEdge(std::vector<NumberedFringe> &fringes, const Visibility &isVisible,
                          double baseline, double maxDistance) {
  auto edgePoint = [&](const Point2d &from, double dx, double dy, Point2d &out) {
    const double len = std::hypot(dx, dy);
    if (len < 1e-9)
      return false;
    dx /= len;
    dy /= len;
    bool moved = false;
    for (double t = 1.0; t <= maxDistance; t += 1.0) {
      const double x = from.x + dx * t;
      const double y = from.y + dy * t;
      if (!isVisible(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y))))
        break;
      out = {x, y};
      moved = true;
    }
    return moved;
  };
  for (auto &fringe : fringes) {
    auto &pts = fringe.points;
    if (pts.size() < 3)
      continue;
    size_t afterFirst = pts.size() - 1;
    for (size_t k = 1; k < pts.size(); ++k) {
      if (pts[k].y - pts.front().y >= baseline) {
        afterFirst = k;
        break;
      }
    }
    size_t beforeLast = 0;
    for (size_t k = pts.size() - 1; k-- > 0;) {
      if (pts.back().y - pts[k].y >= baseline) {
        beforeLast = k;
        break;
      }
    }
    const Point2d first = pts.front();
    const Point2d last = pts.back();
    Point2d head, tail;
    const bool extendHead = edgePoint(first, first.x - pts[afterFirst].x,
                                      first.y - pts[afterFirst].y, head);
    const bool extendTail =
        edgePoint(last, last.x - pts[beforeLast].x, last.y - pts[beforeLast].y, tail);
    if (extendHead)
      pts.insert(pts.begin(), head);
    if (extendTail)
      pts.push_back(tail);
  }
}

}  // namespace

bool ScanlineExtremumTracker::setParam(const std::string &key, const std::string &value) {
  if (key == "fringeCenterMode") {
    if (value == "max")
      m_params.fringeCenterAs = FringeCenterMode::Max;
    else if (value == "min")
      m_params.fringeCenterAs = FringeCenterMode::Min;
    else if (value == "minmax")
      m_params.fringeCenterAs = FringeCenterMode::MinMax;
    else
      return false;
    return true;
  }
  if (key == "scanDirection") {
    if (value == "auto")
      m_params.scanDirection = Params::ScanDirection::Auto;
    else if (value == "rows")
      m_params.scanDirection = Params::ScanDirection::Rows;
    else if (value == "columns")
      m_params.scanDirection = Params::ScanDirection::Columns;
    else
      return false;
    return true;
  }
  if (key == "smoothingSigma") {
    if (value == "auto") {
      m_params.smoothingSigma = -1.0;
      return true;
    }
    try {
      const double sigma = std::stod(value);
      if (sigma < 0.0)
        return false;
      m_params.smoothingSigma = sigma;
      return true;
    } catch (...) {
      return false;
    }
  }
  if (key == "hasInternalObstruction") {
    m_params.hasInternalObstruction = (value == "1" || value == "true");
    return true;
  }
  return false;
}

bool ScanlineExtremumTracker::initialize(const uint8_t *pixels, int width, int height,
                                         std::function<bool(int, int)> isVisible) {
  if (!pixels || width <= 0 || height <= 0) {
    m_lastError = "Empty image";
    return false;
  }
  m_pixelStorage.assign(pixels, pixels + static_cast<size_t>(width) * height);
  m_image = m_pixelStorage.data();
  m_width = width;
  m_height = height;
  m_isVisible = std::move(isVisible);
  m_lastError.clear();
  return true;
}

std::vector<TracedLine> ScanlineExtremumTracker::extract(
    const std::vector<SeedPoint> & /*seeds*/) {
  std::vector<TracedLine> result;
  m_lastFringeNumbers.clear();

  if (!m_image) {
    m_lastError = "Tracer not initialized. Call initialize() first.";
    return result;
  }

  // Направление сканирования: поперёк полос. Решение -- по слегка
  // сглаженному кадру, чтобы спекл не влиял на сравнение градиентов.
  bool columns = m_params.scanDirection == Params::ScanDirection::Columns;
  if (m_params.scanDirection == Params::ScanDirection::Auto) {
    const auto light = gaussianBlur(m_image, m_width, m_height, 2.0, m_isVisible);
    columns = fringesMostlyHorizontal(light.data(), m_width, m_height, m_isVisible);
  }
  m_lastScannedColumns = columns;

  // Рабочий кадр в "строчной" ориентации: при сканировании столбцов --
  // транспонированный, видимость с переставленными координатами.
  std::vector<uint8_t> work =
      columns ? transposed(m_image, m_width, m_height)
              : std::vector<uint8_t>(m_image, m_image + static_cast<size_t>(m_width) * m_height);
  const int workWidth = columns ? m_height : m_width;
  const int workHeight = columns ? m_width : m_height;
  const Visibility workVisible =
      columns ? Visibility([this](int x, int y) { return m_isVisible(y, x); }) : m_isVisible;

  // σ = период/36: на кадрах с ~6 полосами на апертуру (период ~150 px)
  // это ~4 px -- спекл подавлен, а положение полосы почти не сдвигается.
  // При частых полосах (σ < 1 px) не сглаживаем: на чистой синтетике
  // (test_data/Terminal.bmp) даже σ = 1 смещает краевые точки и ухудшает
  // сверку с Terminal_digit.mtr.
  const double period = estimateFringePeriod(work.data(), workWidth, workHeight, workVisible);
  double sigma = m_params.smoothingSigma;
  if (sigma < 0.0)
    sigma = period / 36.0 >= 1.0 ? std::min(period / 36.0, 8.0) : 0.0;
  m_lastSmoothingSigma = sigma;
  if (sigma > 0.0)
    work = gaussianBlur(work.data(), workWidth, workHeight, sigma, workVisible);

  // Краевая полоса шириной период/6 (~25 px на кадрах с ~6 полосами). У
  // реальной оправы светится обод, а профиль полосы у края обрезан --
  // там метод находит обод вместо полосы, и линии загибаются вдоль края.
  // Поиск всё равно идёт по всей апертуре (отступ при поиске теряет полосы
  // у "шапок" круга и портит нумерацию); полоса используется после него,
  // см. trimEdgeBand(). Только вместе со сглаживанием (реальные кадры): на
  // чистой синтетике края честные.
  const int margin = sigma > 0.0 ? static_cast<int>(std::lround(period / 6.0)) : 0;
  m_lastEdgeMargin = margin;
  const int diag = static_cast<int>(std::lround(margin * 0.7071));
  const Visibility inner =
      margin > 0 ? Visibility([workVisible, margin, diag](int x, int y) {
        return workVisible(x, y) && workVisible(x - margin, y) && workVisible(x + margin, y) &&
               workVisible(x, y - margin) && workVisible(x, y + margin) &&
               workVisible(x - diag, y - diag) && workVisible(x + diag, y - diag) &&
               workVisible(x - diag, y + diag) && workVisible(x + diag, y + diag);
      })
                 : workVisible;

  DigitizationInput input;
  input.bitmapData = work.data();
  input.imageWidth = workWidth;
  input.imageHeight = workHeight;
  input.bytesPerLine = workWidth;
  input.isVisible = workVisible;
  input.fringeCenterAs = m_params.fringeCenterAs;
  input.fringeStep = m_params.fringeStep;
  input.toleranceFactor = m_params.toleranceFactor;

  auto scanlines = RedCenterDetector::detectExtrema(input);
  if (scanlines.empty()) {
    m_lastError = "No extrema detected -- check the aperture and fringe contrast";
    return result;
  }

  auto fringes = FringeConstructor::constructFringes(
      scanlines, input.imageWidth, input.imageHeight, workVisible, m_params.fringeCenterAs,
      m_params.fringeStep, m_params.toleranceFactor, m_params.hasInternalObstruction);

  if (fringes.empty()) {
    m_lastError = "Extrema were detected but no continuous fringes could be constructed";
    return result;
  }

  const int minRowGap =
      minRowGapForDecimation(static_cast<int>(fringes.size()), input.imageWidth, input.imageHeight);

  if (margin > 0) {
    trimEdgeBand(fringes, inner);
    const int m2 = 2 * margin, d2 = static_cast<int>(std::lround(m2 * 0.7071));
    trimHooks(fringes, [workVisible, m2, d2](int x, int y) {
      return workVisible(x - m2, y) && workVisible(x + m2, y) && workVisible(x, y - m2) &&
             workVisible(x, y + m2) && workVisible(x - d2, y - d2) &&
             workVisible(x + d2, y - d2) && workVisible(x - d2, y + d2) &&
             workVisible(x + d2, y + d2);
    });
  }

  // Обрывки короче полупериода -- почти всегда шум в плоской светлой зоне,
  // а не полоса; с неверным номером они портят фазу вокруг себя.
  if (period > 0.0) {
    auto isFragment = [period](const NumberedFringe &fringe) {
      if (fringe.points.empty())
        return true;
      double minX = fringe.points.front().x, maxX = minX;
      double minY = fringe.points.front().y, maxY = minY;
      for (const auto &p : fringe.points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
      }
      return std::hypot(maxX - minX, maxY - minY) < period / 2.0;
    };
    fringes.erase(std::remove_if(fringes.begin(), fringes.end(), isFragment), fringes.end());
  }
  renumberByAdjacency(fringes, workHeight, m_params.fringeStep);
  if (margin > 0)
    extendToApertureEdge(fringes, workVisible, period / 2.0, 3.0 * margin);

  result.reserve(fringes.size());
  m_lastFringeNumbers.reserve(fringes.size());
  for (const auto &fringe : fringes) {
    TracedLine line;
    line.reserve(fringe.points.size());
    for (const auto &p : fringe.points) {
      TracedPoint tp;
      tp.x = p.x;
      tp.y = p.y;
      tp.width = 0.0f;
      line.push_back(tp);
    }
    // Прореживание -- в рабочей ориентации (по "строкам" сканирования),
    // затем возврат в координаты исходного кадра; яркость -- из исходного.
    line = decimateLine(line, minRowGap);
    for (auto &tp : line) {
      if (columns)
        std::swap(tp.x, tp.y);
      const int px = static_cast<int>(tp.x + 0.5);
      const int py = static_cast<int>(tp.y + 0.5);
      tp.intensity = (px >= 0 && px < m_width && py >= 0 && py < m_height)
                         ? static_cast<float>(m_image[py * m_width + px])
                         : 0.0f;
    }
    result.push_back(std::move(line));
    m_lastFringeNumbers.push_back(fringe.number);
  }

  m_lastError.clear();
  return result;
}

}  // namespace scanline_extremum_plugin
