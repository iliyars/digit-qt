#include "WaveletPhaseExtractor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <opencv2/core.hpp>
#include <queue>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace digitqt::core::pipeline {

namespace {

// Комплексный вейвлет Морле psi(t) = exp(i*omega0*t) * exp(-t^2/2).
// omega0 = 5.5 -- стандартный компромисс между разрешением по частоте и
// по пространству, используемый в литературе по WTP (Zhong & Weng,
// "Spatial carrier-fringe pattern analysis by means of wavelet
// transform", 2004).
constexpr double kOmega0 = 5.5;

/// Грубая оценка периода несущей (в пикселях) по одному глобальному
/// 2D БПФ -- нужна только чтобы задать диапазон масштабов для CWT ниже;
/// точность не критична, сам гребень CWT потом находит частоту заново
/// и локально в каждой точке.
double estimateCarrierPeriod(const cv::Mat &gray, const cv::Mat &hardMask, int W, int H) {
  double sum = 0.0;
  int count = 0;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (hardMask.at<uchar>(y, x)) {
        sum += gray.at<double>(y, x);
        ++count;
      }
  if (count < 100)
    return 0.0;
  const double meanVal = sum / count;

  cv::Mat windowed(H, W, CV_64F, cv::Scalar(0));
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (hardMask.at<uchar>(y, x))
        windowed.at<double>(y, x) = gray.at<double>(y, x) - meanVal;

  cv::Mat planes[2] = {windowed, cv::Mat::zeros(H, W, CV_64F)};
  cv::Mat complexImg;
  cv::merge(planes, 2, complexImg);
  cv::dft(complexImg, complexImg);

  std::vector<cv::Mat> parts(2);
  cv::split(complexImg, parts);
  cv::Mat mag;
  cv::magnitude(parts[0], parts[1], mag);

  // dft() без fftShift -- низкие частоты в углах матрицы; сворачиваем
  // индексы > N/2 в отрицательные, как в обычной конвенции fftfreq.
  constexpr double kDcRadius = 8.0;
  double maxVal = -1.0;
  double bestFx = 0.0, bestFy = 0.0;
  for (int y = 0; y < H; ++y) {
    const int fy = (y <= H / 2) ? y : y - H;
    for (int x = 0; x < W; ++x) {
      const int fx = (x <= W / 2) ? x : x - W;
      if (std::hypot(static_cast<double>(fx), static_cast<double>(fy)) <= kDcRadius)
        continue;
      const double m = mag.at<double>(y, x);
      if (m > maxVal) {
        maxVal = m;
        bestFx = fx;
        bestFy = fy;
      }
    }
  }
  const double freqPerPixel = std::hypot(bestFx / W, bestFy / H);
  return (freqPerPixel > 1e-9) ? 1.0 / freqPerPixel : 0.0;
}

}  // namespace

WaveletPhaseExtractor::Result WaveletPhaseExtractor::extract(
    const QImage &image, const std::function<bool(int, int)> &isVisible) const {
  Result result;
  const int W = image.width();
  const int H = image.height();

  const QImage grayImg = image.convertToFormat(QImage::Format_Grayscale8);
  cv::Mat gray(H, W, CV_64F);
  cv::Mat hardMask(H, W, CV_8U);
  int totalCount = 0;
  for (int y = 0; y < H; ++y) {
    const uchar *row = grayImg.constScanLine(y);
    for (int x = 0; x < W; ++x) {
      gray.at<double>(y, x) = static_cast<double>(row[x]);
      const bool vis = isVisible(x, y);
      hardMask.at<uchar>(y, x) = vis ? 1 : 0;
      if (vis)
        ++totalCount;
    }
  }
  if (totalCount < 100) {
    result.errorMessage = QStringLiteral("Aperture too small or empty");
    return result;
  }

  const double period0 = estimateCarrierPeriod(gray, hardMask, W, H);
  if (period0 <= 1.0) {
    result.errorMessage = QStringLiteral(
        "Could not find a clear carrier frequency -- fringes may be too faint or absent");
    return result;
  }

  // Диапазон масштабов CWT -- на октаву в каждую сторону от грубой
  // оценки периода несущей. Сам гребень (максимум |W(a,x)| по a) ищет
  // точную локальную частоту в каждой точке отдельно, включая места,
  // где период уже заметно изменился из-за аберраций -- диапазон здесь
  // только ограничивает поиск разумными пределами.
  constexpr int kNumScales = 16;
  const double periodMin = period0 * 0.5;
  const double periodMax = period0 * 2.0;
  std::vector<double> scales(kNumScales);
  for (int i = 0; i < kNumScales; ++i) {
    const double t = static_cast<double>(i) / (kNumScales - 1);
    const double period = periodMin * std::pow(periodMax / periodMin, t);
    scales[i] = kOmega0 * period / (2.0 * M_PI);
  }

  constexpr double kSupportSigmas = 3.0;
  constexpr int kMaxSupport = 120;

  struct Kernel {
    int support = 0;
    double invSqrtA = 0.0;
    std::vector<double> re;  // индекс: dx + support
    std::vector<double> im;
  };
  std::vector<Kernel> kernels(kNumScales);
  for (int s = 0; s < kNumScales; ++s) {
    const double a = scales[static_cast<size_t>(s)];
    const int support =
        std::min(kMaxSupport, std::max(1, static_cast<int>(std::ceil(kSupportSigmas * a))));
    Kernel k;
    k.support = support;
    k.invSqrtA = 1.0 / std::sqrt(a);
    k.re.resize(static_cast<size_t>(2 * support + 1));
    k.im.resize(static_cast<size_t>(2 * support + 1));
    for (int dx = -support; dx <= support; ++dx) {
      const double t = dx / a;
      const double envelope = std::exp(-0.5 * t * t);
      const double angle = kOmega0 * t;
      // conj(psi(t)) = exp(-i*omega0*t) * envelope
      k.re[static_cast<size_t>(dx + support)] = envelope * std::cos(angle);
      k.im[static_cast<size_t>(dx + support)] = -envelope * std::sin(angle);
    }
    kernels[static_cast<size_t>(s)] = std::move(k);
  }

  // --- Гребень CWT: свёрнутая (по модулю 2*pi) фаза в каждой видимой
  // точке апертуры. Только по x в пределах строки -- вейвлет Морле тут
  // одномерный, ищет локальную частоту вдоль строки. ---
  cv::Mat wrapped(H, W, CV_64F, cv::Scalar(std::numeric_limits<double>::quiet_NaN()));
  cv::Mat hasWrapped(H, W, CV_8U, cv::Scalar(0));

  for (int y = 0; y < H; ++y) {
    const double *grayRow = gray.ptr<double>(y);
    const uchar *maskRow = hardMask.ptr<uchar>(y);

    int xMin = W, xMax = -1;
    for (int x = 0; x < W; ++x) {
      if (maskRow[x]) {
        xMin = std::min(xMin, x);
        xMax = std::max(xMax, x);
      }
    }
    if (xMax - xMin < 8)
      continue;

    for (int x = xMin; x <= xMax; ++x) {
      if (!maskRow[x])
        continue;

      double bestMag = -1.0;
      double bestPhase = 0.0;
      for (const auto &k : kernels) {
        const int lo = std::max(0, x - k.support);
        const int hi = std::min(W - 1, x + k.support);
        double reSum = 0.0, imSum = 0.0;
        for (int sx = lo; sx <= hi; ++sx) {
          if (!maskRow[sx])
            continue;
          const int idx = sx - x + k.support;
          const double v = grayRow[sx];
          reSum += v * k.re[static_cast<size_t>(idx)];
          imSum += v * k.im[static_cast<size_t>(idx)];
        }
        const double mag = std::hypot(reSum, imSum) * k.invSqrtA;
        if (mag > bestMag) {
          bestMag = mag;
          bestPhase = std::atan2(imSum, reSum);
        }
      }
      wrapped.at<double>(y, x) = bestPhase;
      hasWrapped.at<uchar>(y, x) = 1;
    }
  }

  // --- Разворачивание: 2D заливка (BFS) от одной точки, а не построчно. ---
  // Раньше каждая строка разворачивалась независимо (используя только
  // соседей по x), а затем соседние строки "подгонялись" друг под друга
  // отдельным проходом по столбцам, сравнивая с одним пикселем сверху.
  // На практике это ненадёжно: из-за шума соседние строки могут
  // независимо "соскочить" на целое число периодов, а подгонка только
  // по одному соседу сверху не всегда может это выправить и местами
  // только добавляет новых искажений (проверено экспериментально: на
  // чистом наклоне без единой аберрации формы это давало ложные
  // "астигматизм"/"дефокус" в разы больше нуля).
  //
  // Заливка от одной точки, сравнивающая каждый новый пиксель сразу со
  // ВСЕМИ четырьмя уже развёрнутыми соседями (а не только с одним по
  // вертикали), устраняет саму возможность рассинхронизации между
  // строками -- весь разворот растёт из одного согласованного центра.
  // Тот же приём уже используется в FourierPhaseExtractor.
  int startX = -1, startY = -1;
  {
    double sxA = 0.0, syA = 0.0;
    int cnt = 0;
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (hardMask.at<uchar>(y, x)) {
          sxA += x;
          syA += y;
          ++cnt;
        }
      }
    }
    startX = static_cast<int>(sxA / cnt);
    startY = static_cast<int>(syA / cnt);
    if (!hasWrapped.at<uchar>(startY, startX)) {
      for (int y = 0; y < H && startX >= 0; ++y) {
        for (int x = 0; x < W; ++x) {
          if (hasWrapped.at<uchar>(y, x)) {
            startX = x;
            startY = y;
            y = H;  // выйти из обоих циклов
            break;
          }
        }
      }
    }
  }
  if (startX < 0 || !hasWrapped.at<uchar>(startY, startX)) {
    result.errorMessage = QStringLiteral("Not enough valid ridge points to unwrap the phase");
    return result;
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
      if (!hasWrapped.at<uchar>(ny, nx) || visited.at<uchar>(ny, nx))
        continue;
      visited.at<uchar>(ny, nx) = 1;
      const double wv = wrapped.at<double>(ny, nx);
      const double k2pi = std::round((wv - base) / (2 * M_PI));
      unwrapped.at<double>(ny, nx) = wv - k2pi * 2 * M_PI;
      q.push({ny, nx});
    }
  }

  result.phaseMap = digitqt::core::PhaseMap(W, H);
  bool any = false;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (!visited.at<uchar>(y, x))
        continue;
      result.phaseMap.setValue(x, y, unwrapped.at<double>(y, x) / (2.0 * M_PI));
      any = true;
    }
  }

  if (!any) {
    result.errorMessage = QStringLiteral("Not enough visible pixels to reconstruct phase");
    return result;
  }

  result.ok = true;
  return result;
}

}  // namespace digitqt::core::pipeline
