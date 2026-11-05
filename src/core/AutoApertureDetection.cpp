#include "AutoApertureDetection.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/TypeLimits.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>  // cv::contourArea (OpenCV 5 moved it out of imgproc.hpp)
#endif

#include <algorithm>
#include <cmath>
#include <vector>

namespace digitqt::core {

namespace {

// Box-filter window for the local-variance map. Large enough to average
// over a few fringe periods (so a single fringe's own bright/dark swing
// doesn't itself look like "no activity" partway through), small enough
// to still separate the aperture from the background at its edge.
constexpr int kVarianceWindow = 15;

// Closes over dark fringe troughs (locally flat, briefly reading as "no
// activity" too) so the detected region is one solid blob instead of a
// ring of disconnected arcs.
constexpr int kCloseKernelSize = 31;

// A least-squares ellipse fit needs nowhere near a full-resolution
// contour's point count (can be thousands of points for a large
// aperture) -- this caps how many get passed to FitEllipse.
constexpr size_t kMaxFitPoints = 200;

ApertureDetectionResult fitActivity(const cv::Mat &activity) {
  ApertureDetectionResult result;
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(activity, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
  if (contours.empty()) {
    result.errorMessage = "No fringe activity found -- check contrast and phase shifts";
    return result;
  }
  const auto &largest = *std::max_element(
      contours.begin(), contours.end(),
      [](const auto &a, const auto &b) { return cv::contourArea(a) < cv::contourArea(b); });
  if (largest.size() < 5 || cv::contourArea(largest) < 100.0) {
    result.errorMessage = "Detected aperture region is too small";
    return result;
  }
  const size_t stride = std::max<size_t>(1, largest.size() / kMaxFitPoints);
  std::vector<cv::Point2f> points;
  for (size_t i = 0; i < largest.size(); i += stride)
    points.emplace_back(static_cast<float>(largest[i].x), static_cast<float>(largest[i].y));
  cv::RotatedRect fit;
  try {
    fit = cv::fitEllipse(points);
  } catch (const cv::Exception &) {
    result.errorMessage = "Ellipse fit failed";
    return result;
  }
  if (!std::isfinite(fit.center.x) || !std::isfinite(fit.center.y) ||
      !std::isfinite(fit.size.width) || !std::isfinite(fit.size.height) ||
      !std::isfinite(fit.angle) || fit.size.width < 4 || fit.size.height < 4 ||
      fit.size.width > 2 * std::max(activity.cols, activity.rows) ||
      fit.size.height > 2 * std::max(activity.cols, activity.rows) || fit.center.x < 0 ||
      fit.center.x >= activity.cols || fit.center.y < 0 || fit.center.y >= activity.rows) {
    result.errorMessage = "Detected ellipse is invalid -- draw the aperture manually";
    return result;
  }
  result.ellipse =
      std::make_unique<aperture::Ellipse>(fit.size.width / 2.0, fit.size.height / 2.0, fit.center.x,
                                          fit.center.y, fit.angle, aperture::TypeLimits::EXTERNAL);
  return result;
}

}  // namespace

ApertureDetectionResult detectApertureBoundary(const digitqt::core::Bitmap &image) {
  ApertureDetectionResult result;
  if (image.isNull()) {
    result.errorMessage = "No image loaded";
    return result;
  }

  // Bitmap is already grayscale (see io::loadImage) and tightly packed
  // (stride == width) -- wrap directly, then clone since cv::Mat doesn't
  // own image.data().
  const cv::Mat wrapped(image.height(), image.width(), CV_8UC1,
                        const_cast<uint8_t *>(image.data()));
  const cv::Mat src = wrapped.clone();

  // With a uniform camera background the pupil can be located directly
  // from contrast against that background. Unlike a variance window,
  // this mask does not grow several pixels beyond the illuminated edge.
  std::vector<uint8_t> border;
  for (int x=0;x<src.cols;++x) {
    border.push_back(src.at<uint8_t>(0,x));
    border.push_back(src.at<uint8_t>(src.rows-1,x));
  }
  for (int y=0;y<src.rows;++y) {
    border.push_back(src.at<uint8_t>(y,0));
    border.push_back(src.at<uint8_t>(y,src.cols-1));
  }
  std::sort(border.begin(),border.end());
  const int spread=border[9*border.size()/10]-border[border.size()/10];
  if(spread<=12) {
    cv::Mat contrast,mask;
    cv::absdiff(src,cv::Scalar(border[border.size()/2]),contrast);
    cv::threshold(contrast,mask,std::max(6,2*spread),255,cv::THRESH_BINARY);
    cv::morphologyEx(mask,mask,cv::MORPH_OPEN,
                    cv::getStructuringElement(cv::MORPH_ELLIPSE,cv::Size(3,3)));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(31, 31)));
    auto contrasted = fitActivity(mask);
    if (contrasted.ok())
      return contrasted;
  }

  cv::Mat blurred;
  cv::GaussianBlur(src, blurred, cv::Size(5, 5), 0);

  // Local variance = E[x^2] - E[x]^2 over a box window. High wherever the
  // image has structure (fringes); near zero over a flat background --
  // deliberately independent of which side is brighter, since real
  // interferograms disagree on that.
  cv::Mat grayF;
  blurred.convertTo(grayF, CV_32F);
  cv::Mat mean, meanOfSquares;
  cv::boxFilter(grayF, mean, CV_32F, cv::Size(kVarianceWindow, kVarianceWindow));
  cv::boxFilter(grayF.mul(grayF), meanOfSquares, CV_32F,
                cv::Size(kVarianceWindow, kVarianceWindow));
  const cv::Mat variance = meanOfSquares - mean.mul(mean);

  cv::Mat variance8;
  cv::normalize(variance, variance8, 0, 255, cv::NORM_MINMAX, CV_8U);

  cv::Mat activity;
  const double otsu =
      cv::threshold(variance8, activity, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  // A high-frequency part of a curved fringe field must not set the
  // threshold for the whole pupil: slow fringes can have much lower local
  // variance while still belonging to the same illuminated aperture.
  // Keep Otsu for low thresholds, cap it for strongly uneven fringe spacing.
  cv::threshold(variance8, activity, std::min(otsu, 5.0), 255, cv::THRESH_BINARY);
  cv::morphologyEx(
      activity, activity, cv::MORPH_CLOSE,
      cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kCloseKernelSize, kCloseKernelSize)));
  // Local windows expand activity beyond the actual image edge. Compensate
  // conservatively after closing, without altering the PSI temporal mask.
  cv::erode(activity, activity, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9)));

  return fitActivity(activity);
}

ApertureDetectionResult detectApertureBoundary(const std::vector<Bitmap> &frames) {
  ApertureDetectionResult result;
  if (frames.size() < 3 || frames[0].isNull()) {
    result.errorMessage = "Aperture detection needs at least 3 non-empty PSI frames";
    return result;
  }
  const int width = frames[0].width(), height = frames[0].height();
  for (const auto &frame : frames)
    if (frame.width() != width || frame.height() != height) {
      result.errorMessage = "All PSI frames must have the same size";
      return result;
    }
  cv::Mat span(height, width, CV_8U);
  size_t histogram[256] = {};
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      int low = 255, high = 0;
      for (const auto &frame : frames) {
        low = std::min(low, int(frame.pixel(x, y)));
        high = std::max(high, int(frame.pixel(x, y)));
      }
      span.at<uint8_t>(y, x) = static_cast<uint8_t>(high - low);
      ++histogram[high - low];
    }
  const double target = .99 * static_cast<double>(width) * height;
  size_t total = 0;
  int percentile = 0;
  while (percentile < 255) {
    total += histogram[percentile];
    if (total >= target)
      break;
    ++percentile;
  }
  cv::Mat activity;
  // 99th percentile prevents isolated hot pixels from setting the threshold.
  // Six levels is an engineering floor for current 8-bit camera data.
  cv::threshold(span, activity, std::max(6.0, .1 * percentile), 255, cv::THRESH_BINARY);
  const auto kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
  cv::morphologyEx(activity, activity, cv::MORPH_OPEN, kernel);
  cv::morphologyEx(activity, activity, cv::MORPH_CLOSE, kernel);
  return fitActivity(activity);
}

}  // namespace digitqt::core
