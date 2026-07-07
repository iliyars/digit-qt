#include "TracedLineItem.h"

#include <QPainterPath>
#include <QPen>

namespace digitqt::gui::canvas {

namespace {
const QColor kPalette[] = {
    QColor(255, 80, 80), QColor(80, 180, 255),  QColor(120, 220, 80),
    QColor(255, 170, 0), QColor(200, 100, 255), QColor(0, 210, 200),
};
constexpr int kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);
} // namespace

TracedLineItem::TracedLineItem(const digitqt::core::tracing::TracedLine &line,
                               size_t index) {
  setZValue(25.0);

  QPainterPath path;
  if (!line.empty()) {
    path.moveTo(line.front().x, line.front().y);
    for (size_t i = 1; i < line.size(); ++i)
      path.lineTo(line[i].x, line[i].y);
  }
  setPath(path);

  QPen pen(kPalette[index % kPaletteSize]);
  pen.setCosmetic(true);
  pen.setWidth(2);
  setPen(pen);
  setBrush(Qt::NoBrush);
  m_basePen = pen;
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

} // namespace digitqt::gui::canvas
