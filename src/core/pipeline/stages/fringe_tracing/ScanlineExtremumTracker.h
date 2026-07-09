#pragma once

#include "core/pipeline/stages/fringe_tracing/IFringeTracer.h"
#include "core/pipeline/stages/fringe_tracing/scanline_extremum/ScanlineExtremumTypes.h"

#include <QImage>

namespace digitqt::core::tracing {

/**
 * @brief Scanline Extremum Method (FTM) -- global, seed-free fringe
 * tracer ported from the original Digit project's RedCenterDetector +
 * FringeConstructor.
 *
 * Unlike SequentialFringeTracker, this doesn't need starting points: it
 * scans the whole image row by row, finds intensity extrema per row
 * (RedCenterDetector), then links and numbers them into continuous
 * fringes across rows (FringeConstructor). extract()'s seeds argument is
 * ignored -- see IFringeTracer's contract.
 */
class ScanlineExtremumTracker : public IFringeTracer {
public:
  struct Params {
    scanline_extremum::FringeCenterMode fringeCenterAs =
        scanline_extremum::FringeCenterMode::MinMax;
    double fringeStep = 1.0;
    double toleranceFactor = 0.3;
  };

  bool initialize(const QImage &image,
                  std::function<bool(int, int)> isVisible) override;
  std::vector<TracedLine> extract(const std::vector<SeedPoint> &seeds) override;
  QString name() const override {
    return QStringLiteral("Scanline Extremum Method (FTM)");
  }
  const QString &lastError() const override { return m_lastError; }

  void setParams(const Params &params) { m_params = params; }
  const Params &params() const { return m_params; }

private:
  QImage m_grayImage;
  std::function<bool(int, int)> m_isVisible;
  Params m_params;
  QString m_lastError;
};

}  // namespace digitqt::core::tracing
