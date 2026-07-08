#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"

#include <QImage>
#include <cstdint>

namespace digitqt::core::tracing {

/// Direction a fringe locally runs in, at 45-degree resolution.
enum class TraceDirection {
  Vertical = 0, // 0 deg   (dx=0, dy=1)
  Diagonal45,   // 45 deg  (dx=1, dy=1)
  Horizontal,   // 90 deg  (dx=1, dy=0)
  Diagonal135,  // 135 deg (dx=1, dy=-1)
};

struct TracerParams {
  float initialWidth =
      20.0f; // expected initial fringe width, px (currently informational)
  float maxWidthChange =
      1.5f; // (currently informational; the ported algorithm hardcodes 1.5)
  float intensityThreshold =
      0.5f; // (currently informational; reserved for future tuning)
  int maxSteps = 200;
  bool bidirectional = true;
  float curvatureCoeff =
      1.5f; // (currently informational; reserved for future tuning)
};

/**
 * @brief Sequential Fringe Tracking (FTM) -- step-by-step tracer ported
 * from the classic SCAN360/STEP.C algorithm.
 *
 * Starting from a user-given seed point, repeatedly measures the local
 * fringe width and direction, steps forward along the fringe, and
 * re-centers perpendicular to the direction of travel to stay locked onto
 * the fringe's intensity ridge. Needs one seed point per fringe -- see
 * ScanlineExtremumTracker for a seed-free alternative.
 */
class SequentialFringeTracker : public IFringeTracer {
public:
  SequentialFringeTracker();

  bool initialize(const QImage &image,
                  std::function<bool(int, int)> isVisible) override;
  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds) override;
  QString name() const override {
    return QStringLiteral("Sequential Fringe Tracking (FTM)");
  }
  const QString &lastError() const override { return m_lastError; }

  void setParams(const TracerParams &params) { m_params = params; }
  const TracerParams &params() const { return m_params; }

  /// Traces a single fringe starting at (startX, startY). Returns an
  /// empty line on failure -- see lastError().
  TracedLine traceLine(int startX, int startY);

private:
  bool traceLineInto(int startX, int startY, TracedLine &outPoints);
  bool isInside(int x, int y) const;
  uint8_t getPixel(int x, int y) const;
  float averageIntensity(int x, int y) const;
  void directionToVector(TraceDirection direction, int &dx, int &dy) const;

  bool firstStep(int x, int y, TracedPoint &point1, TracedPoint &point2);
  int step(TracedLine &line);
  bool measureWidth(int x, int y, float &outWidth,
                    TraceDirection &outDirection);
  bool findMaxAlong(int &x, int &y, int dx, int dy, float searchDist);
  bool centerPerpendicular(int &x, int &y, int dx, int dy);
  void linStepToBoundary(int x1, int y1, int x2, int y2, int &outX,
                         int &outY) const;

  QImage m_grayImage; // owns the pixel data m_image points into
  const uint8_t *m_image = nullptr;
  int m_width = 0;
  int m_height = 0;
  int m_stride = 0;
  std::function<bool(int, int)> m_isVisible;

  TracerParams m_params;

  float m_curWidth = 0.0f;
  float m_curAverage = 0.0f;
  TraceDirection m_curDirection = TraceDirection::Vertical;
  float m_wideLine = 0.0f;
  float m_average = 0.0f;

  TracedLine m_tempLine;
  QString m_lastError;
};

} // namespace digitqt::core::tracing
