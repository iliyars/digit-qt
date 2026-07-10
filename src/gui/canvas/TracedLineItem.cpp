#include "TracedLineItem.h"

#include <QFont>
#include <QPainterPath>
#include <QPen>
#include <cmath>

namespace digitqt::gui::canvas {

namespace {
const QColor kPalette[] = {
    QColor(255, 80, 80), QColor(80, 180, 255),  QColor(120, 220, 80),
    QColor(255, 170, 0), QColor(200, 100, 255), QColor(0, 210, 200),
};
constexpr int kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);

QString formatOrder(double order) {
  // Whole numbers (the common case) print without a decimal point;
  // manually-edited orders may be fractional.
  if (std::abs(order - std::round(order)) < 1e-6)
    return QString::number(static_cast<qlonglong>(std::llround(order)));
  return QString::number(order, 'g', 4);
}
}  // namespace

TracedLineItem::TracedLineItem(const digitqt::core::tracing::TracedLine &line, size_t index,
                               double order)
    : m_orderLabel(new QGraphicsSimpleTextItem(this)) {
  setZValue(25.0);

  QPainterPath path;
  if (!line.empty()) {
    path.moveTo(line.front().x, line.front().y);
    for (size_t i = 1; i < line.size(); ++i)
      path.lineTo(line[i].x, line[i].y);
  }
  setPath(path);

  const QColor color = kPalette[index % kPaletteSize];

  QPen pen(color);
  pen.setCosmetic(true);
  pen.setWidth(2);
  setPen(pen);
  setBrush(Qt::NoBrush);
  m_basePen = pen;

  m_orderLabel->setText(formatOrder(order));
  m_orderLabel->setBrush(color);
  QFont font = m_orderLabel->font();
  font.setBold(true);
  font.setPointSize(9);
  m_orderLabel->setFont(font);
  m_orderLabel->setZValue(27.0);
  if (!line.empty())
    m_orderLabel->setPos(line.front().x + 4.0, line.front().y - 14.0);
}

void TracedLineItem::setEditing(bool editing) {
  QPen pen = m_basePen;
  if (editing) {
    pen.setWidth(3);
    pen.setColor(Qt::white);
  }
  setPen(pen);
  setZValue(editing ? 26.0 : 25.0);
}

}  // namespace digitqt::gui::canvas
