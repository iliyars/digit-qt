#include "FourierPhaseExtractor.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>  // cv::DIST_L2 (OpenCV 5 moved DistanceTypes out of imgproc.hpp)
#endif

#include <algorithm>
#include <cmath>
#include <queue>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace digitqt::core::pipeline {

namespace {

void fftShift(cv::Mat &m) {
  const int cx = m.cols / 2, cy = m.rows / 2;
  cv::Mat q0(m, cv::Rect(0, 0, cx, cy));
  cv::Mat q1(m, cv::Rect(cx, 0, cx, cy));
  cv::Mat q2(m, cv::Rect(0, cy, cx, cy));
  cv::Mat q3(m, cv::Rect(cx, cy, cx, cy));
  cv::Mat tmp;
  q0.copyTo(tmp);
  q3.copyTo(q0);
  tmp.copyTo(q3);
  q1.copyTo(tmp);
  q2.copyTo(q1);
  tmp.copyTo(q2);
}

}  // namespace

FourierPhaseExtractor::Result FourierPhaseExtractor::extract(
    const QImage &image, const std::function<bool(int, int)> &isVisible) const {
  Result result;
  const int W = image.width();
  const int H = image.height();

  // --- 1. Изображение и маска апертуры в OpenCV ---
  const QImage grayImg = image.convertToFormat(QImage::Format_Grayscale8);
  cv::Mat gray(H, W, CV_64F);
  cv::Mat hardMask(H, W, CV_8U);
  double sum = 0.0;
  int count = 0;
  for (int y = 0; y < H; ++y) {
    const uchar *row = grayImg.constScanLine(y);
    for (int x = 0; x < W; ++x) {
      const double v = static_cast<double>(row[x]);
      gray.at<double>(y, x) = v;
      const bool vis = isVisible(x, y);
      hardMask.at<uchar>(y, x) = vis ? 1 : 0;
      if (vis) {
        sum += v;
        ++count;
      }
    }
  }
  if (count < 100) {
    result.errorMessage = QStringLiteral("Aperture too small or empty");
    return result;
  }
  const double meanVal = sum / count;

  // --- 2. Апподизация края апертуры (приподнятый косинус) ---
  // Резкий край даёт сильный "крест" паразитных частот в спектре,
  // который иначе забивает настоящий боковой пик несущей -- проверено
  // экспериментально, без этого шага метод не работает вообще.
  cv::Mat distToOutside;
  cv::distanceTransform(hardMask, distToOutside, cv::DIST_L2, 3);

  constexpr double kTaperWidth = 40.0;
  cv::Mat windowed(H, W, CV_64F, cv::Scalar(0));
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (!hardMask.at<uchar>(y, x))
        continue;
      const double d = distToOutside.at<float>(y, x);
      const double t = std::clamp(d / kTaperWidth, 0.0, 1.0);
      const double soft = 0.5 - 0.5 * std::cos(M_PI * t);
      windowed.at<double>(y, x) = (gray.at<double>(y, x) - meanVal) * soft;
    }
  }

  // --- 3. БПФ ---
  cv::Mat planes[2] = {windowed, cv::Mat::zeros(H, W, CV_64F)};
  cv::Mat complexImg;
  cv::merge(planes, 2, complexImg);
  cv::dft(complexImg, complexImg);
  fftShift(complexImg);

  std::vector<cv::Mat> parts(2);
  cv::split(complexImg, parts);
  cv::Mat mag;
  cv::magnitude(parts[0], parts[1], mag);

  // --- 4. Ищем боковой пик несущей (исключая окрестность DC) ---
  const int cx0 = W / 2, cy0 = H / 2;
  constexpr double kDcRadius = 8.0;
  double maxVal = -1.0;
  int peakX = cx0, peakY = cy0;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (std::hypot(x - cx0, y - cy0) <= kDcRadius)
        continue;
      const double m = mag.at<double>(y, x);
      if (m > maxVal) {
        maxVal = m;
        peakX = x;
        peakY = y;
      }
    }
  }

  const double peakDist = std::hypot(peakX - cx0, peakY - cy0);
  if (peakDist < kDcRadius + 1.0) {
    result.errorMessage = QStringLiteral(
        "Could not find a clear carrier frequency -- fringes may be too faint or absent");
    return result;
  }

  // --- 5. Гауссов фильтр вокруг пика ---
  const double filterRadius = peakDist * 0.6;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const double d = std::hypot(x - peakX, y - peakY);
      const double g = std::exp(-(d * d) / (2.0 * filterRadius * filterRadius));
      parts[0].at<double>(y, x) *= g;
      parts[1].at<double>(y, x) *= g;
    }
  }
  cv::merge(parts, complexImg);

  // --- 6. Демодуляция: сдвигаем найденный пик в центр, обратное БПФ ---
  cv::Mat shifted(H, W, complexImg.type());
  const int shiftX = cx0 - peakX;
  const int shiftY = cy0 - peakY;
  for (int y = 0; y < H; ++y) {
    const int sy = ((y + shiftY) % H + H) % H;
    for (int x = 0; x < W; ++x) {
      const int sx = ((x + shiftX) % W + W) % W;
      shifted.at<cv::Vec2d>(sy, sx) = complexImg.at<cv::Vec2d>(y, x);
    }
  }
  fftShift(shifted);

  cv::Mat inv;
  cv::idft(shifted, inv, cv::DFT_SCALE);
  cv::split(inv, parts);

  // --- 7. Свёрнутая фаза ---
  cv::Mat wrapped(H, W, CV_64F);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      wrapped.at<double>(y, x) = std::atan2(parts[1].at<double>(y, x), parts[0].at<double>(y, x));

  // --- 8. Разворачивание фазы (BFS от точки внутри апертуры) ---
  int startX = -1, startY = -1;
  {
    double sxA = 0.0, syA = 0.0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        if (hardMask.at<uchar>(y, x)) {
          sxA += x;
          syA += y;
        }
    startX = static_cast<int>(sxA / count);
    startY = static_cast<int>(syA / count);
    if (!hardMask.at<uchar>(startY, startX)) {
      // Центр масс не попал в саму апертуру (например, апертура с
      // вырезом) -- берём первую попавшуюся видимую точку.
      for (int y = 0; y < H && startX < 0; ++y) {
        for (int x = 0; x < W; ++x) {
          if (hardMask.at<uchar>(y, x)) {
            startX = x;
            startY = y;
            break;
          }
        }
      }
    }
  }

  cv::Mat unwrapped(H, W, CV_64F, cv::Scalar(0));
  cv::Mat visited(H, W, CV_8U, cv::Scalar(0));
  unwrapped.at<double>(startY, startX) = wrapped.at<double>(startY, startX);
  visited.at<uchar>(startY, startX) = 1;

  std::queue<std::pair<int, int>> q;
  q.push({startY, startX});
  constexpr int dy[4] = {-1, 1, 0, 0};
  constexpr int dx[4] = {0, 0, -1, 1};
  while (!q.empty()) {
    const auto [y, x] = q.front();
    q.pop();
    const double base = unwrapped.at<double>(y, x);
    for (int k = 0; k < 4; ++k) {
      const int ny = y + dy[k], nx = x + dx[k];
      if (ny < 0 || ny >= H || nx < 0 || nx >= W)
        continue;
      if (!hardMask.at<uchar>(ny, nx) || visited.at<uchar>(ny, nx))
        continue;
      visited.at<uchar>(ny, nx) = 1;
      const double wv = wrapped.at<double>(ny, nx);
      const double k2pi = std::round((wv - base) / (2 * M_PI));
      unwrapped.at<double>(ny, nx) = wv - k2pi * 2 * M_PI;
      q.push({ny, nx});
    }
  }

  // --- 9. Восстанавливаем "потерянный" при демодуляции наклон ---
  // Найденный пик соответствует пространственной частоте несущей
  // (peakX-cx0)/W циклов/пиксель по x и (peakY-cy0)/H по y. Переводим
  // это обратно в фазовую плоскость и складываем с развёрнутым
  // остатком -- без этого шага общий наклон почти полностью теряется
  // (проверено экспериментально на синтетических данных с заранее
  // известным наклоном).
  double minX = W, maxX = -1, minY = H, maxY = -1;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (!hardMask.at<uchar>(y, x))
        continue;
      minX = std::min<double>(minX, x);
      maxX = std::max<double>(maxX, x);
      minY = std::min<double>(minY, y);
      maxY = std::max<double>(maxY, y);
    }
  }
  const double apCx = (minX + maxX) / 2.0;
  const double apCy = (minY + maxY) / 2.0;

  const double carrierFreqX = (peakX - cx0) / static_cast<double>(W);
  const double carrierFreqY = (peakY - cy0) / static_cast<double>(H);

  result.phaseMap = digitqt::core::PhaseMap(W, H);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (!hardMask.at<uchar>(y, x))
        continue;
      const double carrierPhase = 2 * M_PI * (carrierFreqX * (x - apCx) + carrierFreqY * (y - apCy));
      const double totalPhase = unwrapped.at<double>(y, x) + carrierPhase;
      result.phaseMap.setValue(x, y, totalPhase / (2 * M_PI));  // -> номер полосы N
    }
  }

  result.ok = true;
  return result;
}

}  // namespace digitqt::core::pipeline
