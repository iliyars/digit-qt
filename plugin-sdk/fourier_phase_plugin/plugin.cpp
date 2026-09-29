/**
 * @file plugin.cpp
 * @brief Метод Фурье-анализа полос (Такеда, 1982) -- самодостаточный
 * DqtPhaseReconstructor C ABI-плагин. Линкует OpenCV напрямую (сторонняя
 * библиотека, естественная зависимость самого алгоритма), но НЕ
 * DigitQt::Core -- см. тот же приём в binary_thinning_tracker_plugin/
 * plugin.cpp. Порт core::pipeline::FourierPhaseExtractor (см. git-историю
 * FourierPhaseExtractor.{h,cpp} до выноса) без изменений в математике --
 * core::Bitmap/PhaseMap там использовались только на входе/выходе.
 */

#include "dqt_phase_reconstructor_abi.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>  // cv::DIST_L2 (OpenCV 5 moved DistanceTypes out of imgproc.hpp)
#endif

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

class FourierPhaseReconstructor {
 public:
  bool reconstruct(int width, int height, const uint8_t *pixels,
                   std::function<bool(int, int)> isVisible, double *outValues) {
    m_lastError.clear();

    // NaN по умолчанию -- "вне апертуры/не вычислено" (тот же контракт,
    // что у core::PhaseMap и у horizontal_spline_phase_plugin).
    std::fill(outValues, outValues + static_cast<size_t>(width) * height,
             std::numeric_limits<double>::quiet_NaN());

    const int W = width;
    const int H = height;

    // --- 1. Изображение и маска апертуры в OpenCV ---
    cv::Mat gray(H, W, CV_64F);
    cv::Mat hardMask(H, W, CV_8U);
    double sum = 0.0;
    int count = 0;
    for (int y = 0; y < H; ++y) {
      const uint8_t *row = pixels + static_cast<size_t>(y) * W;
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
      m_lastError = "Aperture too small or empty";
      return false;
    }
    const double meanVal = sum / count;

    // --- 2. Апподизация края апертуры (приподнятый косинус) ---
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
        // Спектр реального сигнала эрмитов: (x,y) и его зеркало
        // (2*cx0-x, 2*cy0-y) равны по модулю, но комплексно сопряжены --
        // выбор произвольного из них демодулирует СОПРЯЖЁННУЮ боковую
        // полосу примерно в половине случаев, переворачивая знак каждого
        // нечётного члена (наклон, кома, трефойл). Ограничение поиска
        // положительной половиной частот (x > cx0, при равенстве -- y >
        // cy0) детерминированно выбирает истинную +phase полосу.
        if (x < cx0 || (x == cx0 && y <= cy0))
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
      m_lastError = "Could not find a clear carrier frequency -- fringes may be too faint or absent";
      return false;
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

    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (!hardMask.at<uchar>(y, x))
          continue;
        const double carrierPhase =
            2 * M_PI * (carrierFreqX * (x - apCx) + carrierFreqY * (y - apCy));
        const double totalPhase = unwrapped.at<double>(y, x) + carrierPhase;
        outValues[static_cast<size_t>(y) * W + x] = totalPhase / (2 * M_PI);  // -> номер полосы N
      }
    }

    return true;
  }

  const std::string &lastError() const { return m_lastError; }

 private:
  std::string m_lastError;
};

// --- Граница ABI --------------------------------------------------------

struct PluginState {
  FourierPhaseReconstructor reconstructor;
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
        "FourierPhaseExtractor requires gridWidth/gridHeight to equal image dimensions";
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
  static const char *const kName = "Fourier Phase Extractor";
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
  outInfo->pluginName = "FourierPhaseExtractor";
  outInfo->pluginVersion = "1.0.0";
  outInfo->needsFringeLines = 0;  // работает прямо по пикселям изображения
  *outVTable = &kVTable;
  return 1;
}
