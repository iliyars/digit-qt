#include "SetFiducialReferenceCommand.h"

#include "core/Measurement.h"

#include <QCoreApplication>
#include <algorithm>

namespace digitqt::commands {

namespace {

digitqt::core::Fiducial *findById(digitqt::core::Measurement &measurement, int id) {
  auto &fiducials = measurement.fiducials().fiducials();
  auto it = std::find_if(fiducials.begin(), fiducials.end(),
                         [id](const auto &f) { return f.id == id; });
  return it == fiducials.end() ? nullptr : &(*it);
}

}  // namespace

SetFiducialReferenceCommand::SetFiducialReferenceCommand(digitqt::core::Measurement &measurement,
                                                          int fiducialId, bool hasReference,
                                                          double referenceX, double referenceY,
                                                          QUndoCommand *parent)
    : QUndoCommand(parent),
      m_measurement(measurement),
      m_fiducialId(fiducialId),
      m_newHasReference(hasReference),
      m_newReferenceX(referenceX),
      m_newReferenceY(referenceY) {
  setText(QCoreApplication::translate("SetFiducialReferenceCommand",
                                      "Set fiducial reference position"));
}

void SetFiducialReferenceCommand::redo() {
  auto *f = findById(m_measurement, m_fiducialId);
  if (!f)
    return;
  m_oldHasReference = f->hasReference;
  m_oldReferenceX = f->referenceX;
  m_oldReferenceY = f->referenceY;
  m_hasOld = true;

  f->hasReference = m_newHasReference;
  f->referenceX = m_newReferenceX;
  f->referenceY = m_newReferenceY;
}

void SetFiducialReferenceCommand::undo() {
  if (!m_hasOld)
    return;
  auto *f = findById(m_measurement, m_fiducialId);
  if (!f)
    return;
  f->hasReference = m_oldHasReference;
  f->referenceX = m_oldReferenceX;
  f->referenceY = m_oldReferenceY;
}

}  // namespace digitqt::commands
