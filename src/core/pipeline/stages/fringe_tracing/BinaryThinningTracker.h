#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QImage>
#include <opencv2/core.hpp>

namespace digitqt::core::tracing {

/**
 * @brief Tuning parameters for BinaryThinningTracker.
 *
 * Optimal values depend on image resolution and fringe contrast/spacing;
 * start from the defaults and adjust.
 */
struct BinaryThinningParams {
  // --- Stage 1: binarization ---
  int gaussianKernel =
      5;  // Gaussian blur kernel size (odd, >= 3). 0 = no smoothing
  int adaptiveBlockSize = 51;  // adaptiveThreshold block size (odd)
  double adaptiveC = -5.0;  // adaptiveThreshold subtraction constant (negative
                            // for bright fringes)
  int morphKernelSize =
      3;  // morphological open/close kernel size (odd, >= 3). 0 = skip

  // --- Stage 2: line extraction ---
  int minLineLength = 30;  // lines shorter than this (in points) are discarded
  bool computeWidth = true;  // measure fringe width via distance transform

  // --- Stage 3: post-processing ---
  bool smoothLines = false;
  int smoothWindow = 3;  // moving-average half-window
  int pruneLength =
      40;  // max length of skeleton spurs removed before extraction
  int linkDistance =
      15;  // max gap (px) bridged between two line endpoints; 0 = disabled
  float maxAvgAngle = 0.4f;  // reject lines whose average turn angle (radians)
                             // between segments exceeds this
};

/**
 * @brief Fringe Binary Method (FBM) -- global, seed-free tracer based on
 * adaptive-threshold binarization and morphological skeletonization.
 *
 * Ported from the uploaded InterferometryApp project's CFringeSkeletonizer.
 * Pipeline: Gaussian blur -> adaptive threshold -> aperture mask ->
 * morphological open/close -> Zhang-Suen thinning -> spur pruning ->
 * polyline extraction from the skeleton -> gap linking (cubic Bezier,
 * with parallelism + "bridge stays on white" checks) -> curvature-based
 * rejection of implausible lines.
 *
 * Adapted from the original: takes a QImage + isVisible(x,y) predicate
 * instead of cv::Mat + CEllipseBoundary (works with our multi-shape
 * aperture, not a single ellipse); development-only file/console debug
 * output removed. The algorithm itself (thresholds, thinning rules,
 * linking heuristics) is otherwise unchanged.
 *
 * Needs no seed points -- extract()'s seeds argument is ignored, see
 * IFringeTracer's contract.
 */
class BinaryThinningTracker : public IFringeTracer {
public:
  BinaryThinningTracker() = default;

  bool initialize(const QImage &image,
                  std::function<bool(int, int)> isVisible) override;
  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds) override;
  QString name() const override {
    return QStringLiteral("Binary Thinning Method (FBM)");
  }
  const QString &lastError() const override { return m_lastError; }

  void setParams(const BinaryThinningParams &params) { m_params = params; }
  const BinaryThinningParams &params() const { return m_params; }

  // Debug/visualization accessors (mirrors the original's GetBinary()/
  // GetSkeleton()/GetDistMap(); not currently wired into the UI).
  const cv::Mat &mask() const { return m_mask; }
  const cv::Mat &binary() const { return m_binary; }
  const cv::Mat &skeleton() const { return m_skeleton; }
  const cv::Mat &distanceMap() const { return m_distMap; }

private:
  bool buildBinary();
  void skeletonize(const cv::Mat &binary, cv::Mat &skeleton) const;
  int countNeighbors(const cv::Mat &skel, int x, int y) const;
  void pruneSkeleton(cv::Mat &skel, int maxBranchLength) const;
  TracedLine traceBranch(const cv::Mat &skel, cv::Mat &visited, int sx,
                         int sy) const;
  std::vector<TracedLine> extractPolylines() const;
  void smoothLine(TracedLine &line) const;
  void linkBrokenLines(std::vector<TracedLine> &lines) const;
  void filterByCurvature(std::vector<TracedLine> &lines) const;

  BinaryThinningParams m_params;

  cv::Mat m_image;   // grayscale input (CV_8UC1), owned copy
  cv::Mat m_mask;    // aperture mask (255 = inside), built from m_isVisible
  cv::Mat m_binary;  // binarized fringes
  cv::Mat m_skeleton;
  cv::Mat m_distMap;  // distance transform, for width (only if computeWidth)

  std::function<bool(int, int)> m_isVisible;
  QString m_lastError;
};

}  // namespace digitqt::core::tracing
