#pragma once

#include <QUndoCommand>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::commands {

class SetFringeOrderCommand : public QUndoCommand {
public:
  SetFringeOrderCommand(digitqt::core::Measurement &measurement, size_t lineIndex, double newOrder,
                        QUndoCommand *parent = nullptr);

  void redo() override;
  void undo() override;

private:
  digitqt::core::Measurement &m_measurement;
  size_t m_lineIndex;
  double m_newOrder;
  double m_previousOrder = 0.0;
  bool m_previousWasManual = false;
};

}  // namespace digitqt::commands
