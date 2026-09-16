#include "core/NumberedFringeLine.h"
#include "core/pipeline/stages/phase_reconstruction/PhaseReconstructor.h"

#include <QtTest/QtTest>

#include <cmath>

namespace {

using digitqt::core::NumberedFringeLine;
using digitqt::core::tracing::TracedPoint;

NumberedFringeLine makeLine(double order, std::initializer_list<std::pair<double, double>> points) {
  NumberedFringeLine line;
  line.order = order;
  for (const auto &[x, y] : points) {
    TracedPoint p;
    p.x = x;
    p.y = y;
    line.points.push_back(p);
  }
  return line;
}

}  // namespace

class PhaseReconstructorTest : public QObject {
  Q_OBJECT

private slots:
  // Two traced lines whose paths locally cross (one dips to nearly the
  // same X as its neighbor at a single row) must not blow up the
  // per-row cubic spline: this is the exact mechanism found in practice
  // -- a line pushed slightly past its neighbor by auto-extension
  // (core::extrapolateFringesVertically) crosses it at one row, giving
  // the spline two almost-coincident X knots with very different
  // fringe-order values, and an unguarded natural cubic spline's
  // Thomas-algorithm solve divides by that near-zero gap.
  void crossingLinesDoNotBlowUpTheSpline();
};

void PhaseReconstructorTest::crossingLinesDoNotBlowUpTheSpline() {
  std::vector<NumberedFringeLine> lines = {
      makeLine(0.0, {{10.0, 0.0}, {10.0, 10.0}, {10.0, 20.0}}),
      // Dips to x=10.05 at y=10 -- almost touching the order-0 line right
      // there, while staying to the right of it (x=30) elsewhere.
      makeLine(3.0, {{30.0, 0.0}, {10.05, 10.0}, {30.0, 20.0}}),
      makeLine(6.0, {{50.0, 0.0}, {50.0, 10.0}, {50.0, 20.0}}),
  };

  digitqt::core::pipeline::PhaseReconstructor reconstructor;
  auto isVisible = [](int, int) { return true; };
  const auto phase = reconstructor.reconstruct(60, 21, isVisible, lines);

  QVERIFY(reconstructor.lastError().isEmpty());
  QVERIFY(!phase.isEmpty());

  // The whole line set only spans fringe orders 0..6 -- any well-behaved
  // interpolation (crossing or not) must stay within a small margin of
  // that range at every visible pixel on the affected row, not blow up
  // to the thousands a divide-by-near-zero in the spline would produce.
  for (int x = 0; x < 60; ++x) {
    if (!phase.hasValue(x, 10))
      continue;
    const double v = phase.value(x, 10);
    QVERIFY2(std::abs(v) < 100.0,
            qPrintable(QStringLiteral("row 10, x=%1: value %2 is not bounded").arg(x).arg(v)));
  }
}

QTEST_MAIN(PhaseReconstructorTest)
#include "PhaseReconstructorTest.moc"
