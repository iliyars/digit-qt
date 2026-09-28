#pragma once

#include <QDialog>

class QTableWidget;
class QUndoStack;

namespace digitqt::core {
class Measurement;
}

namespace digitqt::gui {

/**
 * @brief Небольшой модальный диалог со списком всех расставленных
 * реперов (id + позиция в СКИ, только чтение) и редактируемыми полями
 * эталонной позиции -- шаг 3 использования реперов (см.
 * core::fitFiducialTransform() для шага 4).
 */
class FiducialReferenceDialog : public QDialog {
  Q_OBJECT
public:
  FiducialReferenceDialog(digitqt::core::Measurement *measurement, QUndoStack *undoStack,
                          QWidget *parent = nullptr);

private slots:
  void applyChanges();

private:
  void populateTable();

  digitqt::core::Measurement *m_measurement;
  QUndoStack *m_undoStack;
  QTableWidget *m_table;
};

}  // namespace digitqt::gui
