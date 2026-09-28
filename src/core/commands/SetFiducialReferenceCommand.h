#pragma once

#include <QUndoCommand>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

/// Задаёт (или снимает) эталонную позицию одного репера, по id -- не по
/// индексу, потому что диалог, который это редактирует, может показывать
/// реперы в любом порядке.
class SetFiducialReferenceCommand : public QUndoCommand {
public:
  SetFiducialReferenceCommand(digitqt::core::Measurement &measurement, int fiducialId,
                              bool hasReference, double referenceX, double referenceY,
                              QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  int m_fiducialId;
  bool m_newHasReference;
  double m_newReferenceX;
  double m_newReferenceY;

  bool m_oldHasReference = false;
  double m_oldReferenceX = 0.0;
  double m_oldReferenceY = 0.0;
  bool m_hasOld = false;
};

}  // namespace digitqt::commands
