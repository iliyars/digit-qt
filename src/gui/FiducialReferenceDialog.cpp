#include "FiducialReferenceDialog.h"

#include "core/Measurement.h"
#include "core/commands/SetFiducialReferenceCommand.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace digitqt::gui {

namespace {
constexpr int kColId = 0;
constexpr int kColImageX = 1;
constexpr int kColImageY = 2;
constexpr int kColRefX = 3;
constexpr int kColRefY = 4;
}  // namespace

FiducialReferenceDialog::FiducialReferenceDialog(digitqt::core::Measurement *measurement,
                                                  QUndoStack *undoStack, QWidget *parent)
    : QDialog(parent), m_measurement(measurement), m_undoStack(undoStack) {
  setWindowTitle(tr("Fiducial Reference Positions"));
  resize(480, 320);

  m_table = new QTableWidget(this);
  m_table->setColumnCount(5);
  m_table->setHorizontalHeaderLabels(
      {tr("ID"), tr("Image X"), tr("Image Y"), tr("Reference X"), tr("Reference Y")});
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_table->verticalHeader()->setVisible(false);

  populateTable();

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &FiducialReferenceDialog::applyChanges);
  connect(buttons, &QDialogButtonBox::rejected, this, &FiducialReferenceDialog::reject);

  auto *hint = new QLabel(
      tr("Enter the known true position for each fiducial you have an external reference for "
         "(e.g. a calibration target). Leave Reference X/Y blank for fiducials without one -- "
         "they're excluded from the transform fit."),
      this);
  hint->setWordWrap(true);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(hint);
  layout->addWidget(m_table);
  layout->addWidget(buttons);
}

void FiducialReferenceDialog::populateTable() {
  const auto &fiducials = m_measurement->fiducials().fiducials();
  m_table->setRowCount(static_cast<int>(fiducials.size()));

  for (int row = 0; row < static_cast<int>(fiducials.size()); ++row) {
    const auto &f = fiducials[static_cast<size_t>(row)];

    auto *idItem = new QTableWidgetItem(QString::number(f.id));
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, kColId, idItem);

    auto *imgXItem = new QTableWidgetItem(QString::number(f.imageX, 'f', 2));
    imgXItem->setFlags(imgXItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, kColImageX, imgXItem);

    auto *imgYItem = new QTableWidgetItem(QString::number(f.imageY, 'f', 2));
    imgYItem->setFlags(imgYItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, kColImageY, imgYItem);

    m_table->setItem(
        row, kColRefX,
        new QTableWidgetItem(f.hasReference ? QString::number(f.referenceX) : QString()));
    m_table->setItem(
        row, kColRefY,
        new QTableWidgetItem(f.hasReference ? QString::number(f.referenceY) : QString()));
  }
}

void FiducialReferenceDialog::applyChanges() {
  const auto &fiducials = m_measurement->fiducials().fiducials();

  for (int row = 0; row < m_table->rowCount(); ++row) {
    if (static_cast<size_t>(row) >= fiducials.size())
      break;
    const auto &current = fiducials[static_cast<size_t>(row)];

    const QString xText = m_table->item(row, kColRefX)->text().trimmed();
    const QString yText = m_table->item(row, kColRefY)->text().trimmed();

    bool okX = false, okY = false;
    const double x = xText.toDouble(&okX);
    const double y = yText.toDouble(&okY);
    const bool hasReference = okX && okY;

    const bool unchanged = current.hasReference == hasReference &&
                           (!hasReference ||
                            (current.referenceX == x && current.referenceY == y));
    if (unchanged)
      continue;

    m_undoStack->push(new digitqt::commands::SetFiducialReferenceCommand(
        *m_measurement, current.id, hasReference, hasReference ? x : 0.0,
        hasReference ? y : 0.0));
  }

  accept();
}

}  // namespace digitqt::gui
