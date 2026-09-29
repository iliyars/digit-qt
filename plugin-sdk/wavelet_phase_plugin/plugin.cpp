/**
 * @file plugin.cpp
 * @brief Реконструкция фазы методом непрерывного вейвлет-анализа (Zhong &
 * Weng, 2004) -- самодостаточный DqtPhaseReconstructor C ABI-плагин.
 * Линкует OpenCV напрямую (сторонняя библиотека, не DigitQt::Core) -- см.
 * тот же приём в fourier_phase_plugin/plugin.cpp. Порт
 * core::pipeline::WaveletPhaseExtractor (см. git-историю
 * WaveletPhaseExtractor.{h,cpp} до выноса) без изменений в математике.
 */

#include "dqt_phase_reconstructor_abi.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <queue>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr double kOmega0 = 5.5;

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

class WaveletPhaseReconstructor {
 public:
  bool reconstruct(int width, int height, const uint8_t *pixels,
                   std::function<bool(int, int)> isVisible, double *outValues) {
    m_lastError.clear();

    std::fill(outValues, outValues + static_cast<size_t>(width) * height,
             std::numeric_limits<double>::quiet_NaN());

    const int W = width;
    const int H = height;

    cv::Mat gray(H, W, CV_64F);
    cv::Mat hardMask(H, W, CV_8U);
    int totalCount = 0;
    for (int y = 0; y < H; ++y) {
      const uint8_t *row = pixels + static_cast<size_t>(y) * W;
      for (int x = 0; x < W; ++x) {
        gray.at<double>(y, x) = static_cast<double>(row[x]);
        const bool vis = isVisible(x, y);
        hardMask.at<uchar>(y, x) = vis ? 1 : 0;
        if (vis)
          ++totalCount;
      }
    }
    if (totalCount < 100) {
      m_lastError = "Aperture too small or empty";
      return false;
    }

    const double period0 = estimateCarrierPeriod(gray, hardMask, W, H);
    if (period0 <= 1.0) {
      m_lastError = "Could not find a clear carrier frequency -- fringes may be too faint or absent";
      return false;
    }

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
      std::vector<double> re;
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
        k.re[static_cast<size_t>(dx + support)] = envelope * std::cos(angle);
        k.im[static_cast<size_t>(dx + support)] = -envelope * std::sin(angle);
      }
      kernels[static_cast<size_t>(s)] = std::move(k);
    }

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
      m_lastError = "Not enough valid ridge points to unwrap the phase";
      return false;
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

    bool any = false;
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (!visited.at<uchar>(y, x))
          continue;
        outValues[static_cast<size_t>(y) * W + x] = unwrapped.at<double>(y, x) / (2.0 * M_PI);
        any = true;
      }
    }

    if (!any) {
      m_lastError = "Not enough visible pixels to reconstruct phase";
      return false;
    }

    return true;
  }

  const std::string &lastError() const { return m_lastError; }

 private:
  std::string m_lastError;
};

// --- Граница ABI --------------------------------------------------------

struct PluginState {
  WaveletPhaseReconstructor reconstructor;
  std::string lastErrorUtf8;
};

DqtPhaseReconstructorHandle create() {
  return reinterpret_cast<DqtPhaseReconstructorHandle>(new (std::nothrow) PluginState());
}

void destroy(DqtPhaseReconstructorHandle self) {
  delete reinterpret_cast<PluginState *>(self);
}

int reconstruct(DqtPhaseReconstructorHandle self, int32_t gridWidth, int32_t gridHeight,
                const DqtPhaseBitmapView *image, DqtPhaseVisibilityFn isVisible,
                void *isVisibleUserData, const DqtPhaseNumberedFringeLine * /*lines*/,
                size_t /*lineCount*/, double *outValues) {
  auto *state = reinterpret_cast<PluginState *>(self);

  if (!image || gridWidth != image->width || gridHeight != image->height) {
    state->lastErrorUtf8 =
        "WaveletPhaseExtractor requires gridWidth/gridHeight to equal image dimensions";
    return 0;
  }

  auto predicate = [isVisible, isVisibleUserData](int x, int y) {
    return isVisible(x, y, isVisibleUserData) != 0;
  };

  const bool ok =
      state->reconstructor.reconstruct(gridWidth, gridHeight, image->pixels, predicate, outValues);
  state->lastErrorUtf8 = state->reconstructor.lastError();
  return ok ? 1 : 0;
}

const char *name(DqtPhaseReconstructorHandle /*self*/) {
  static const char *const kName = "Wavelet Phase Extractor";
  return kName;
}

const char *lastError(DqtPhaseReconstructorHandle self) {
  return reinterpret_cast<PluginState *>(self)->lastErrorUtf8.c_str();
}

const DqtPhaseReconstructorVTable kVTable = {
    &create, &destroy, &reconstruct, &name, &lastError,
};

}  // namespace

extern "C" DQT_ABI_EXPORT int dqt_phase_plugin_entry(uint32_t hostAbiVersion,
                                                      DqtPhasePluginInfo *outInfo,
                                                      const DqtPhaseReconstructorVTable **outVTable) {
  if (hostAbiVersion != DQT_PHASE_RECONSTRUCTOR_ABI_VERSION) {
    *outVTable = nullptr;
    return 0;
  }
  outInfo->pluginName = "WaveletPhaseExtractor";
  outInfo->pluginVersion = "1.0.0";
  outInfo->needsFringeLines = 0;  // работает прямо по пикселям изображения
  *outVTable = &kVTable;
  return 1;
}
