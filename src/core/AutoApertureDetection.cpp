#include "AutoApertureDetection.h"

#include <aperture/include/geometry/Ellipse.h>
#include <aperture/include/visibility/TypeLimits.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>  // cv::contourArea (OpenCV 5 moved it out of imgproc.hpp)
#endif

#include <algorithm>
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

}  // namespace

ApertureDetectionResult detectApertureBoundary(const QImage &image) {
  ApertureDetectionResult result;
  if (image.isNull()) {
    result.errorMessage = QStringLiteral("No image loaded");
    return result;
  }

  const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

  // Wrap QImage's buffer, then clone -- QImage's underlying data is
  // reference-counted/shared and we need a copy we own independently
  // (same pattern as BinaryThinningTracker::initialize()).
  const cv::Mat wrapped(gray.height(), gray.width(), CV_8UC1,
                        const_cast<uchar *>(gray.constBits()),
                        static_cast<size_t>(gray.bytesPerLine()));
  const cv::Mat src = wrapped.clone();

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
  cv::threshold(variance8, activity, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  cv::morphologyEx(activity, activity, cv::MORPH_CLOSE,
                   cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                             cv::Size(kCloseKernelSize, kCloseKernelSize)));

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(activity, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
  if (contours.empty()) {
    result.errorMessage =
        QStringLiteral("No fringe activity found -- the image may be blank, too noisy, or "
                       "too low-contrast for automatic detection");
    return result;
  }

  const auto &largest = *std::max_element(
      contours.begin(), contours.end(),
      [](const auto &a, const auto &b) { return cv::contourArea(a) < cv::contourArea(b); });

  if (cv::contourArea(largest) < 100.0) {
    result.errorMessage = QStringLiteral("Detected aperture region is too small");
    return result;
  }

  // Same ill-conditioning workaround as
  // BoundaryEditController::finalizePointsEllipse(): fit in
  // centroid-relative coordinates, then shift the result back --
  // ApertureCore's FitEllipse has no coordinate normalization of its
  // own, and raw pixel coordinates far from the origin make its 5x5
  // linear system badly conditioned.
  double sumX = 0.0, sumY = 0.0;
  for (const auto &p : largest) {
    sumX += p.x;
    sumY += p.y;
  }
  const double centroidX = sumX / static_cast<double>(largest.size());
  const double centroidY = sumY / static_cast<double>(largest.size());

  const size_t stride = std::max<size_t>(1, largest.size() / kMaxFitPoints);
  std::vector<aperture::Point> points;
  points.reserve(largest.size() / stride + 1);
  for (size_t i = 0; i < largest.size(); i += stride)
    points.push_back(aperture::Point{largest[i].x - centroidX, largest[i].y - centroidY});

  auto ellipse = aperture::Ellipse::FitEllipse(points, aperture::TypeLimits::EXTERNAL);
  if (!ellipse) {
    result.errorMessage = QStringLiteral("Ellipse fit failed");
    return result;
  }
  ellipse->shiftX(centroidX);
  ellipse->shiftY(centroidY);

  result.ellipse = std::move(ellipse);
  return result;
}

}  // namespace digitqt::core
