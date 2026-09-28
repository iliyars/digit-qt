#include "FiducialController.h"

#include "core/Measurement.h"
#include "core/commands/AddFiducialCommand.h"
#include "core/commands/RemoveFiducialCommand.h"

#include <cmath>

namespace digitqt::gui::canvas {

using digitqt::commands::AddFiducialCommand;
using digitqt::commands::RemoveFiducialCommand;

FiducialController::FiducialController(QUndoStack *undoStack, QObject *parent)
    : QObject(parent), m_undoStack(undoStack) {}

void FiducialController::setMeasurement(digitqt::core::Measurement *measurement) {
  m_measurement = measurement;
  m_selection.reset();
  emit fiducialsChanged();
  emit selectionChanged();
}

void FiducialController::handlePress(const QPointF &pos, bool isPrimaryButton) {
  if (!m_measurement || !isPrimaryButton)
    return;

  if (m_mode == FiducialEditMode::AddFiducial) {
    m_undoStack->push(new AddFiducialCommand(*m_measurement, pos.x(), pos.y()));
    emit fiducialsChanged();
    return;
  }

  auto hit = hitTest(pos);
  if (hit != m_selection) {
    m_selection = hit;
    emit selectionChanged();
  }
}

void FiducialController::deleteSelection() {
  if (!m_measurement || !m_selection)
    return;
  m_undoStack->push(new RemoveFiducialCommand(*m_measurement, *m_selection));
  m_selection.reset();
  emit fiducialsChanged();
  emit selectionChanged();
}

void FiducialController::clearSelection() {
  if (!m_selection)
    return;
  m_selection.reset();
  emit selectionChanged();
}

std::optional<size_t> FiducialController::hitTest(const QPointF &pos) const {
  if (!m_measurement)
    return std::nullopt;
  const auto &fiducials = m_measurement->fiducials().fiducials();

  constexpr double kHitRadius = 8.0;
  std::optional<size_t> best;
  double bestDist = kHitRadius;

  for (size_t i = 0; i < fiducials.size(); ++i) {
    const double dx = pos.x() - fiducials[i].imageX;
    const double dy = pos.y() - fiducials[i].imageY;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

}  // namespace digitqt::gui::canvas
