#include "core/FringeEdgeExtrapolation.h"

#include <QtTest/QtTest>

namespace {

using digitqt::core::NumberedFringeLine;
using digitqt::core::extrapolateFringesHorizontally;
using digitqt::core::extrapolateFringesVertically;
using digitqt::core::removeFringeExtensions;
using digitqt::core::tracing::TracedPoint;

NumberedFringeLine makeLine(std::initializer_list<std::pair<double, double>> points) {
  NumberedFringeLine line;
  for (const auto &[x, y] : points) {
    TracedPoint p;
    p.x = x;
    p.y = y;
    line.points.push_back(p);
  }
  return line;
}

auto alwaysVisible() {
  return [](double, double) { return true; };
}

class FringeEdgeExtrapolationTest : public QObject {
  Q_OBJECT

private slots:
  // Horizontal
  void horizontalNeedsAtLeastTwoLines();
  void horizontalGrowsBothSidesByLocalStep();
  void horizontalAsymmetricVisibilitySkipsOneSide();
  void horizontalDegenerateStepSkipsThatSide();
  void horizontalZeroCapAddsNothing();
  void horizontalOvershootMarginAddsMultipleLines();

  // Vertical
  void verticalStraightLineGrowsBothEnds();
  void verticalDegenerateStepDoesNotGrow();
  void verticalStopsAtLastVisiblePoint();
  void verticalSkipsLinesWithFewerThanTwoPoints();
  void verticalOvershootMarginAddsMultiplePoints();

  // Remove extensions
  void removeExtensionsDropsWhollySyntheticLine();
  void removeExtensionsStripsPartiallyExtendedLine();
  void removeExtensionsLeavesNonSyntheticLinesUntouched();
  void removeExtensionsUndoesHorizontalExtension();
  void removeExtensionsUndoesVerticalExtension();
};

void FringeEdgeExtrapolationTest::horizontalNeedsAtLeastTwoLines() {
  std::vector<NumberedFringeLine> lines = {makeLine({{100.0, 0.0}, {100.0, 10.0}})};
  const auto result = extrapolateFringesHorizontally(lines, alwaysVisible(), 5, 1);
  QCOMPARE(result.size(), size_t{1});
  QCOMPARE(result[0].points.size(), size_t{2});
}

void FringeEdgeExtrapolationTest::horizontalGrowsBothSidesByLocalStep() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{150.0, 0.0}, {150.0, 10.0}}),
      makeLine({{200.0, 0.0}, {200.0, 10.0}}),
  };

  const auto result = extrapolateFringesHorizontally(lines, alwaysVisible(), 3, 1);

  // 3 original + 3 synthetic on each side.
  QCOMPARE(result.size(), size_t{9});

  int syntheticCount = 0;
  bool sawX50 = false, sawX0 = false, sawXMinus50 = false;
  bool sawX250 = false, sawX300 = false, sawX350 = false;
  for (const auto &line : result) {
    const double x = line.points.front().x;
    if (!line.isSynthetic()) {
      // Originals must stay untouched.
      QVERIFY(x == 100.0 || x == 150.0 || x == 200.0);
      continue;
    }
    ++syntheticCount;
    if (x == 50.0) sawX50 = true;
    if (x == 0.0) sawX0 = true;
    if (x == -50.0) sawXMinus50 = true;
    if (x == 250.0) sawX250 = true;
    if (x == 300.0) sawX300 = true;
    if (x == 350.0) sawX350 = true;
  }
  QCOMPARE(syntheticCount, 6);
  QVERIFY(sawX50 && sawX0 && sawXMinus50 && sawX250 && sawX300 && sawX350);
}

void FringeEdgeExtrapolationTest::horizontalAsymmetricVisibilitySkipsOneSide() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{150.0, 0.0}, {150.0, 10.0}}),
      makeLine({{200.0, 0.0}, {200.0, 10.0}}),
  };
  // Only x >= 90 is visible: the first left candidate (x=50) is entirely
  // invisible -- with a margin of 1 it still gets added (one deliberate
  // overshoot past the edge), then the left side stops there. The right
  // side never leaves the visible region within 3 steps, so it keeps
  // growing to the cap.
  auto isVisible = [](double x, double) { return x >= 90.0; };

  const auto result = extrapolateFringesHorizontally(lines, isVisible, 3, 1);

  int syntheticCount = 0;
  bool sawLeftOvershoot = false;
  int rightCount = 0;
  for (const auto &line : result) {
    if (!line.isSynthetic())
      continue;
    ++syntheticCount;
    const double x = line.points.front().x;
    if (x < 90.0) {
      QCOMPARE(x, 50.0);  // the single deliberate overshoot, one step past the edge
      sawLeftOvershoot = true;
    } else {
      QVERIFY(x > 200.0);
      ++rightCount;
    }
  }
  QVERIFY(sawLeftOvershoot);
  QCOMPARE(rightCount, 3);
  QCOMPARE(syntheticCount, 4);
}

void FringeEdgeExtrapolationTest::horizontalDegenerateStepSkipsThatSide() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{100.1, 0.0}, {100.1, 10.0}}),  // step to the leftmost line is 0.1 -- degenerate
      makeLine({{300.0, 0.0}, {300.0, 10.0}}),
  };

  const auto result = extrapolateFringesHorizontally(lines, alwaysVisible(), 5, 1);

  int syntheticCount = 0;
  for (const auto &line : result) {
    if (!line.isSynthetic())
      continue;
    ++syntheticCount;
    // Only the right side (from the x=300 line) should have grown.
    QVERIFY(line.points.front().x > 300.0);
  }
  QCOMPARE(syntheticCount, 5);
}

void FringeEdgeExtrapolationTest::horizontalZeroCapAddsNothing() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{150.0, 0.0}, {150.0, 10.0}}),
  };

  const auto result = extrapolateFringesHorizontally(lines, alwaysVisible(), 0, 1);
  QCOMPARE(result.size(), size_t{2});
  for (const auto &line : result)
    QVERIFY(!line.isSynthetic());
}

void FringeEdgeExtrapolationTest::horizontalOvershootMarginAddsMultipleLines() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{150.0, 0.0}, {150.0, 10.0}}),
      makeLine({{200.0, 0.0}, {200.0, 10.0}}),
  };
  // Only x >= 90 is visible: the left side crosses out immediately
  // (x=50, 0, -50 are all invisible) -- with a margin of 3, all three
  // are added before the left side stops, not just the first.
  auto isVisible = [](double x, double) { return x >= 90.0; };

  const auto result = extrapolateFringesHorizontally(lines, isVisible, 10, /*overshootMargin=*/3);

  bool sawX50 = false, sawX0 = false, sawXMinus50 = false;
  int leftCount = 0;
  for (const auto &line : result) {
    if (!line.isSynthetic())
      continue;
    const double x = line.points.front().x;
    if (x >= 90.0)
      continue;  // right-side growth, not under test here
    ++leftCount;
    if (x == 50.0) sawX50 = true;
    if (x == 0.0) sawX0 = true;
    if (x == -50.0) sawXMinus50 = true;
  }
  QCOMPARE(leftCount, 3);
  QVERIFY(sawX50 && sawX0 && sawXMinus50);
}

void FringeEdgeExtrapolationTest::verticalStraightLineGrowsBothEnds() {
  std::vector<NumberedFringeLine> lines = {makeLine({{100.0, 50.0}, {100.0, 60.0}})};

  const auto result = extrapolateFringesVertically(lines, alwaysVisible(), 3, 1);

  QCOMPARE(result.size(), size_t{1});
  const auto &points = result[0].points;
  QCOMPARE(points.size(), size_t{2 + 3 + 3});
  QCOMPARE(points.front().y, 20.0);  // 50 - 3*10
  QCOMPARE(points.back().y, 90.0);   // 60 + 3*10
  QVERIFY(result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::verticalDegenerateStepDoesNotGrow() {
  std::vector<NumberedFringeLine> lines = {makeLine({{100.0, 50.0}, {100.0, 50.0}})};

  const auto result = extrapolateFringesVertically(lines, alwaysVisible(), 5, 1);

  QCOMPARE(result[0].points.size(), size_t{2});
  QVERIFY(!result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::verticalStopsAtLastVisiblePoint() {
  std::vector<NumberedFringeLine> lines = {makeLine({{100.0, 90.0}, {100.0, 100.0}})};
  // Visible only for y in [70, 115]: front steps by -10 from 90 -> 80
  // (visible), 70 (visible), 60 (invisible -- with a margin of 1, added
  // anyway as the deliberate overshoot, then stops). Back steps by +10
  // from 100 -> 110 (visible), 120 (invisible -- added, then stops).
  auto isVisible = [](double, double y) { return y >= 70.0 && y <= 115.0; };

  const auto result = extrapolateFringesVertically(lines, isVisible, 100, 1);

  const auto &points = result[0].points;
  QCOMPARE(points.size(), size_t{2 + 3 + 2});
  QCOMPARE(points.front().y, 60.0);
  QCOMPARE(points.back().y, 120.0);
  QVERIFY(result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::verticalSkipsLinesWithFewerThanTwoPoints() {
  std::vector<NumberedFringeLine> lines = {makeLine({{100.0, 50.0}})};

  const auto result = extrapolateFringesVertically(lines, alwaysVisible(), 5, 1);

  QCOMPARE(result[0].points.size(), size_t{1});
  QVERIFY(!result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::verticalOvershootMarginAddsMultiplePoints() {
  std::vector<NumberedFringeLine> lines = {makeLine({{100.0, 90.0}, {100.0, 100.0}})};
  // Same visible range as verticalStopsAtLastVisiblePoint, but with a
  // margin of 3: front keeps going past 60 to 50 and 40 (3 invisible
  // points total); back keeps going past 120 to 130 and 140.
  auto isVisible = [](double, double y) { return y >= 70.0 && y <= 115.0; };

  const auto result = extrapolateFringesVertically(lines, isVisible, 100, /*overshootMargin=*/3);

  const auto &points = result[0].points;
  QCOMPARE(points.size(), size_t{2 + 5 + 4});
  QCOMPARE(points.front().y, 40.0);
  QCOMPARE(points.back().y, 140.0);
  QVERIFY(result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::removeExtensionsDropsWhollySyntheticLine() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),  // real
      makeLine({{50.0, 0.0}, {50.0, 10.0}}),    // wholly synthetic (as growSide() produces it)
  };
  lines[1].syntheticFrontCount = static_cast<int>(lines[1].points.size());

  const auto result = removeFringeExtensions(lines);

  QCOMPARE(result.size(), size_t{1});
  QCOMPARE(result[0].points.front().x, 100.0);
  QVERIFY(!result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::removeExtensionsStripsPartiallyExtendedLine() {
  // A real 2-point line extended by 2 points at the front and 1 at the back.
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 70.0}, {100.0, 80.0}, {100.0, 90.0}, {100.0, 100.0}, {100.0, 110.0}}),
  };
  lines[0].syntheticFrontCount = 2;
  lines[0].syntheticBackCount = 1;

  const auto result = removeFringeExtensions(lines);

  QCOMPARE(result.size(), size_t{1});
  const auto &points = result[0].points;
  QCOMPARE(points.size(), size_t{2});
  QCOMPARE(points.front().y, 90.0);
  QCOMPARE(points.back().y, 100.0);
  QVERIFY(!result[0].isSynthetic());
}

void FringeEdgeExtrapolationTest::removeExtensionsLeavesNonSyntheticLinesUntouched() {
  std::vector<NumberedFringeLine> lines = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{200.0, 0.0}, {200.0, 10.0}}),
  };

  const auto result = removeFringeExtensions(lines);

  QCOMPARE(result.size(), size_t{2});
  for (const auto &line : result)
    QVERIFY(!line.isSynthetic());
}

void FringeEdgeExtrapolationTest::removeExtensionsUndoesHorizontalExtension() {
  std::vector<NumberedFringeLine> original = {
      makeLine({{100.0, 0.0}, {100.0, 10.0}}),
      makeLine({{150.0, 0.0}, {150.0, 10.0}}),
      makeLine({{200.0, 0.0}, {200.0, 10.0}}),
  };

  const auto extended = extrapolateFringesHorizontally(original, alwaysVisible(), 3, 1);
  QVERIFY(extended.size() > original.size());  // sanity: it actually grew

  const auto restored = removeFringeExtensions(extended);

  QCOMPARE(restored.size(), original.size());
  for (const auto &line : restored)
    QVERIFY(!line.isSynthetic());
}

void FringeEdgeExtrapolationTest::removeExtensionsUndoesVerticalExtension() {
  std::vector<NumberedFringeLine> original = {
      makeLine({{100.0, 50.0}, {100.0, 60.0}}),
  };

  const auto extended = extrapolateFringesVertically(original, alwaysVisible(), 3, 1);
  QVERIFY(extended[0].points.size() > original[0].points.size());  // sanity: it actually grew

  const auto restored = removeFringeExtensions(extended);

  QCOMPARE(restored.size(), size_t{1});
  QCOMPARE(restored[0].points.size(), original[0].points.size());
  QCOMPARE(restored[0].points.front().y, original[0].points.front().y);
  QCOMPARE(restored[0].points.back().y, original[0].points.back().y);
  QVERIFY(!restored[0].isSynthetic());
}

}  // namespace

QTEST_MAIN(FringeEdgeExtrapolationTest)
#include "FringeEdgeExtrapolationTest.moc"
