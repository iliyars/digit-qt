#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QImage>
#include <cstdint>

namespace digitqt::core::tracing {

struct StructureTensorParams {
  int maxSteps = 200;
  bool bidirectional = true;

  // Local ridge-direction estimation via the image structure tensor --
  // a continuous angle from the gradient field, not quantized to
  // 45-degree steps.
  int orientationWindowRadius = 3; // px
  int directionSmoothingWindow =
      5; // average direction over this many past steps

  // Stepping and 2D local-maximum centering.
  float stepFraction = 0.5f; // step size = current width * this
  float minStepSize = 2.0f;
  float maxStepSize = 15.0f;
  float searchRadiusFraction = 0.5f; // 2D search radius = current width * this
  float minSearchRadius = 2.0f;
  float maxSearchRadius = 15.0f;
  float maxCenteringJumpFactor =
      1.0f; // reject a maximum further than this * width from the prediction

  // Loss-of-fringe detection, based on local contrast (peak - background),
  // not raw intensity -- robust to vignetting since it's measured fresh
  // at every point instead of compared against one earlier sample.
  float contrastSmoothingAlpha = 0.3f;
  float minContrastFraction = 0.4f;
};

/**
 * @brief Ridge Tracking (Structure Tensor) -- seed-based tracer, offered
 * as an alternative to SequentialFringeTracker (the classic
 * SCAN360/STEP.C-derived tracer).
 *
 * Starting from a user-given seed point, follows the fringe's intensity
 * ridge step by step:
 *   1. Estimate the local ridge direction via the image structure tensor
 *      (the eigenvector of the smaller eigenvalue of the local gradient
 *      tensor -- the direction of least intensity variation, i.e. along
 *      the ridge). A continuous angle, unlike SequentialFringeTracker's
 *      45-degree-quantized direction.
 *   2. Step forward along that direction.
 *   3. Re-center by finding the sub-pixel intensity maximum in a genuine
 *      2D neighborhood around the predicted point (not a single line --
 *      SequentialFringeTracker only searches along the one perpendicular
 *      line implied by its quantized direction, which can miss an
 *      obviously brighter pixel just off that line).
 *   4. Re-estimate direction/width at the new point; direction is
 *      smoothed over recent history to resist single-step noise.
 *   5. Stop on: loss of local contrast (EMA-tracked, adapts to gradual
 *      vignetting), too large a centering jump (would mean locking onto a
 *      neighboring fringe), leaving the aperture, loop closure, or max steps.
 *
 * Needs one seed point per fringe -- see ScanlineExtremumTracker for a
 * seed-free alternative.
 */
class StructureTensorTracker : public IFringeTracer {
public:
  StructureTensorTracker();

  bool initialize(const QImage &image,
                  std::function<bool(int, int)> isVisible) override;
  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds) override;
  QString name() const override {
    return QStringLiteral("Ride Tracking (Structure Tensor)");
  }
  const QString &lastError() const override { return m_lastError; }

  void setParams(const StructureTensorParams &params) { m_params = params; }
  const StructureTensorParams &params() const { return m_params; }

  /// Traces a single fringe starting at (startX, startY). Returns an
  /// empty line on failure -- see lastError().
  TracedLine traceLine(int startX, int startY);

private:
  bool traceLineInto(int startX, int startY, TracedLine &outPoints);
  void traceDirectionally(TracedLine &line, double dirX, double dirY);

  bool isInside(int x, int y) const;
  uint8_t getPixel(int x, int y) const;
  float sampleBilinear(double x, double y) const;

  /// Ridge direction at (x, y) via the local structure tensor. Returns
  /// false if the neighborhood is entirely outside the image/aperture.
  bool estimateDirection(double x, double y, double &dirX, double &dirY) const;

  /// Full-width-half-max style width estimate along the direction
  /// perpendicular to (dirX, dirY); also reports the local background
  /// level it used, for contrast-based loss-of-fringe detection.
  float estimateWidth(double x, double y, double dirX, double dirY,
                      float &outBackground) const;

  /// Sub-pixel 2D local maximum within `radius` of (cx, cy): a
  /// brute-force search over the window, refined via independent 1D
  /// quadratic fits in x and y through the winning pixel.
  bool findLocalMaximum(double cx, double cy, float radius, double &outX,
                        double &outY, float &outIntensity) const;

  QImage m_grayImage; // owns the pixel data m_image points into
  const uint8_t *m_image = nullptr;
  int m_width = 0;
  int m_height = 0;
  int m_stride = 0;
  std::function<bool(int, int)> m_isVisible;

  StructureTensorParams m_params;
  float m_contrastEma =
      0.0f; // running local-contrast estimate for the current trace direction

  QString m_lastError;
};

} // namespace digitqt::core::tracing
