#include "FringeTracingController.h"

#include "core/AutoSeedPlacement.h"
#include "core/Measurement.h"
#include "core/commands/AddSeedCommand.h"
#include "core/commands/AddSeedsCommand.h"
#include "core/commands/RemoveSeedCommand.h"
#include "core/commands/ReplaceTracedLineCommand.h"
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/PipelineStageId.h"

#include <aperture/include/visibility/VisibilityChecker.h>

#include <algorithm>
#include <cmath>

namespace digitqt::gui::canvas {

using digitqt::commands::AddSeedCommand;
using digitqt::commands::AddSeedsCommand;
using digitqt::commands::RemoveSeedCommand;
using digitqt::commands::ReplaceTracedLineCommand;

namespace {

double pointSegmentDistance(const QPointF &p, const QPointF &a,
                            const QPointF &b) {
  const QPointF ab = b - a;
  const double lenSq = QPointF::dotProduct(ab, ab);
  double t = 0.0;
  if (lenSq > 1e-9)
    t = QPointF::dotProduct(p - a, ab) / lenSq;
  t = std::clamp(t, 0.0, 1.0);
  const QPointF proj = a + ab * t;
  const QPointF d = p - proj;
  return std::sqrt(QPointF::dotProduct(d, d));
}

} // namespace

FringeTracingController::FringeTracingController(QUndoStack *undoStack,
                                                 QObject *parent)
    : QObject(parent), m_undoStack(undoStack) {}

void FringeTracingController::setMeasurement(
    digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  m_selection.reset();
  m_editingLineIndex.reset();
  m_selectedPointIndex.reset();
  m_draggingPoint = false;
  emit seedsChanged();
  emit tracedLinesChanged();
  emit selectionChanged();
  emit lineEditModeChanged();
}

void FringeTracingController::setPipeline(
    digitqt::core::pipeline::Pipeline *pipeline) {
  m_pipeline = pipeline;
}

void FringeTracingController::handlePress(const QPointF &pos,
                                          bool isPrimaryButton) {
  if (!m_measurement || !isPrimaryButton)
    return;

  if (m_editingLineIndex) {
    auto pointHit = hitTestPointInEditingLine(pos);
    if (pointHit)
      beginPointDrag(*pointHit, pos);
    else
      insertPointOnEditingLine(pos);
    return;
  }

  if (m_mode == FringeEditMode::AddSeed) {
    const digitqt::core::tracing::SeedPoint seed{static_cast<int>(pos.x()),
                                                 static_cast<int>(pos.y())};
    m_undoStack->push(new AddSeedCommand(*m_measurement, seed));
    emit seedsChanged();
    return;
  }

  auto hit = hitTestSeed(pos);
  if (hit != m_selection) {
    m_selection = hit;
    emit selectionChanged();
  }
}

void FringeTracingController::handleMove(const QPointF &pos) {
  if (m_draggingPoint)
    updatePointDrag(pos);
}

void FringeTracingController::handleRelease(const QPointF & /*pos*/) {
  if (m_draggingPoint)
    commitPointDrag();
}

void FringeTracingController::handleDoubleClick(const QPointF &pos) {
  if (!m_measurement)
    return;
  m_editingLineIndex = hitTestAnyLine(pos);
  m_selectedPointIndex.reset();
  m_draggingPoint = false;
  emit lineEditModeChanged();
}

void FringeTracingController::exitLineEditMode() {
  if (!m_editingLineIndex)
    return;
  m_editingLineIndex.reset();
  m_selectedPointIndex.reset();
  m_draggingPoint = false;
  emit lineEditModeChanged();
}

std::optional<size_t>
FringeTracingController::hitTestSeed(const QPointF &pos) const {
  if (!m_measurement)
    return std::nullopt;
  const auto &seeds = m_measurement->fringeTracing().seeds();

  constexpr double kHitRadius = 8.0;
  std::optional<size_t> best;
  double bestDist = kHitRadius;

  for (size_t i = 0; i < seeds.size(); ++i) {
    const double dx = pos.x() - seeds[i].x;
    const double dy = pos.y() - seeds[i].y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

std::optional<size_t>
FringeTracingController::hitTestAnyLine(const QPointF &pos) const {
  if (!m_measurement)
    return std::nullopt;

  constexpr double kHitTolerance = 8.0;
  const auto &lines = m_measurement->fringeTracing().tracedLines();

  std::optional<size_t> best;
  double bestDist = kHitTolerance;

  for (size_t li = 0; li < lines.size(); ++li) {
    const auto &line = lines[li];
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      const QPointF a(line[i].x, line[i].y);
      const QPointF b(line[i + 1].x, line[i + 1].y);
      const double dist = pointSegmentDistance(pos, a, b);
      if (dist < bestDist) {
        bestDist = dist;
        best = li;
      }
    }
  }
  return best;
}

std::optional<size_t>
FringeTracingController::hitTestPointInEditingLine(const QPointF &pos) const {
  if (!m_measurement || !m_editingLineIndex)
    return std::nullopt;
  const auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*m_editingLineIndex >= lines.size())
    return std::nullopt;
  const auto &line = lines[*m_editingLineIndex];

  constexpr double kHitRadius = 8.0;
  std::optional<size_t> best;
  double bestDist = kHitRadius;

  for (size_t i = 0; i < line.size(); ++i) {
    const double dx = pos.x() - line[i].x;
    const double dy = pos.y() - line[i].y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

void FringeTracingController::beginPointDrag(size_t pointIndex,
                                             const QPointF & /*pos*/) {
  if (!m_measurement || !m_editingLineIndex)
    return;
  auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*m_editingLineIndex >= lines.size())
    return;

  m_draggingPoint = true;
  m_dragPointIndex = pointIndex;
  m_selectedPointIndex = pointIndex;
  m_dragLineBefore = lines[*m_editingLineIndex]; // snapshot for undo
  emit lineEditModeChanged();
}

void FringeTracingController::updatePointDrag(const QPointF &pos) {
  if (!m_measurement || !m_editingLineIndex)
    return;
  auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*m_editingLineIndex >= lines.size())
    return;
  auto &line = lines[*m_editingLineIndex];
  if (m_dragPointIndex >= line.size())
    return;

  line[m_dragPointIndex].x = pos.x();
  line[m_dragPointIndex].y = pos.y();
  emit tracedLinesChanged();
}

void FringeTracingController::commitPointDrag() {
  if (!m_draggingPoint)
    return;
  m_draggingPoint = false;

  if (!m_measurement || !m_editingLineIndex)
    return;
  auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*m_editingLineIndex >= lines.size())
    return;

  auto after = lines[*m_editingLineIndex]; // already-live-updated state
  // Roll back to the pre-drag snapshot; the command's redo() re-applies
  // it, so the whole drag lands as a single undo step.
  lines[*m_editingLineIndex] = m_dragLineBefore;
  m_undoStack->push(new ReplaceTracedLineCommand(
      *m_measurement, *m_editingLineIndex, m_dragLineBefore, after));
  emit tracedLinesChanged();
}

void FringeTracingController::insertPointOnEditingLine(const QPointF &pos) {
  if (!m_measurement || !m_editingLineIndex)
    return;
  auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*m_editingLineIndex >= lines.size())
    return;
  const auto &line = lines[*m_editingLineIndex];
  if (line.size() < 2)
    return;

  constexpr double kInsertTolerance = 15.0;
  size_t bestSegment = 0;
  double bestDist = kInsertTolerance;
  bool found = false;

  for (size_t i = 0; i + 1 < line.size(); ++i) {
    const QPointF a(line[i].x, line[i].y);
    const QPointF b(line[i + 1].x, line[i + 1].y);
    const double dist = pointSegmentDistance(pos, a, b);
    if (dist < bestDist) {
      bestDist = dist;
      bestSegment = i;
      found = true;
    }
  }

  if (!found)
    return;

  auto before = line;
  auto after = line;

  digitqt::core::tracing::TracedPoint newPoint;
  newPoint.x = pos.x();
  newPoint.y = pos.y();
  newPoint.width =
      (line[bestSegment].width + line[bestSegment + 1].width) / 2.0f;
  newPoint.intensity =
      (line[bestSegment].intensity + line[bestSegment + 1].intensity) / 2.0f;

  after.insert(after.begin() + static_cast<long>(bestSegment) + 1, newPoint);

  m_selectedPointIndex.reset();
  m_undoStack->push(new ReplaceTracedLineCommand(
      *m_measurement, *m_editingLineIndex, before, after));
  emit tracedLinesChanged();
}

void FringeTracingController::deleteSelection() {
  if (m_editingLineIndex && m_selectedPointIndex) {
    deleteSelectedPoint();
    return;
  }

  if (!m_measurement || !m_selection)
    return;
  m_undoStack->push(new RemoveSeedCommand(*m_measurement, *m_selection));
  m_selection.reset();
  emit seedsChanged();
  emit selectionChanged();
}

void FringeTracingController::deleteSelectedPoint() {
  if (!m_measurement || !m_editingLineIndex || !m_selectedPointIndex)
    return;
  auto &lines = m_measurement->fringeTracing().tracedLines();
  if (*m_editingLineIndex >= lines.size())
    return;
  const auto &line = lines[*m_editingLineIndex];
  if (*m_selectedPointIndex >= line.size())
    return;
  if (line.size() <= 2)
    return; // keep at least 2 points -- a shorter "line" isn't meaningful

  auto before = line;
  auto after = line;
  after.erase(after.begin() + static_cast<long>(*m_selectedPointIndex));

  m_selectedPointIndex.reset();
  m_draggingPoint = false;
  m_undoStack->push(new ReplaceTracedLineCommand(
      *m_measurement, *m_editingLineIndex, before, after));
  emit tracedLinesChanged();
}

bool FringeTracingController::runTracing() {
  if (!m_measurement || !m_pipeline)
    return false;

  auto &stage = m_pipeline->stage(digitqt::core::pipeline::StageId::Setup);
  const bool ok = stage.compute(*m_measurement);
  m_lastError = stage.errorMessage();
  emit tracedLinesChanged();
  return ok;
}

void FringeTracingController::autoPlaceSeeds() {
  if (!m_measurement || !m_measurement->hasImage())
    return;

  aperture::VisibilityChecker checker(m_measurement->boundaries());
  auto isVisible = [&checker](int x, int y) {
    return checker.isVisible(
        aperture::Point{static_cast<double>(x), static_cast<double>(y)});
  };

  auto seeds = digitqt::core::findRowSeeds(m_measurement->image(), isVisible);
  if (seeds.empty()) {
    m_lastError = QStringLiteral("No intensity peaks found to place seeds at");
    return;
  }

  m_undoStack->push(new AddSeedsCommand(*m_measurement, std::move(seeds)));
  emit seedsChanged();
}

} // namespace digitqt::gui::canvas
