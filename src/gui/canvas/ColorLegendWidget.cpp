#include "ColorLegendWidget.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>

namespace digitqt::gui::canvas {

namespace {
constexpr int kBarWidth = 20;
constexpr int kBarHeight = 140;
constexpr int kMargin = 8;
constexpr int kLabelWidth = 64;
}  // namespace

ColorLegendWidget::ColorLegendWidget(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
}

void ColorLegendWidget::setRange(double minValue, double maxValue, const QString &unitSuffix) {
  m_minValue = minValue;
  m_maxValue = maxValue;
  m_unitSuffix = unitSuffix;
  m_hasData = true;
  update();
}

void ColorLegendWidget::setNoData() {
  m_hasData = false;
  update();
}

QSize ColorLegendWidget::sizeHint() const {
  return QSize(kBarWidth + kMargin * 2 + kLabelWidth, kBarHeight + kMargin * 2);
}

void ColorLegendWidget::paintEvent(QPaintEvent * /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Полупрозрачная подложка, чтобы легенда читалась на любом фоне.
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(30, 30, 30, 170));
  painter.drawRoundedRect(rect(), 6, 6);

  if (!m_hasData) {
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, tr("no data"));
    return;
  }

  const QRectF barRect(kMargin, kMargin, kBarWidth, kBarHeight);

  // top = максимум (красный), bottom = минимум (синий) -- те же самые
  // hue-точки, что rebuildHeatmap() использует для самой карты:
  // hue = (1 - t) * 240, t=1 (максимум) -> hue=0 (красный), t=0 -> hue=240 (синий).
  QLinearGradient gradient(barRect.topLeft(), barRect.bottomLeft());
  gradient.setColorAt(0.0, QColor::fromHsv(0, 255, 255));
  gradient.setColorAt(0.5, QColor::fromHsv(120, 255, 255));
  gradient.setColorAt(1.0, QColor::fromHsv(240, 255, 255));

  painter.setBrush(gradient);
  painter.setPen(QPen(Qt::white, 1));
  painter.drawRect(barRect);

  painter.setPen(Qt::white);
  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);

  const int decimals = m_unitSuffix.isEmpty() ? 2 : 0;
  const auto formatValue = [&](double v) {
    return QString::number(v, 'f', decimals) + m_unitSuffix;
  };

  painter.drawText(QRectF(barRect.right() + 4, barRect.top() - 6, kLabelWidth, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, formatValue(m_maxValue));
  painter.drawText(QRectF(barRect.right() + 4, barRect.center().y() - 8, kLabelWidth, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, formatValue((m_minValue + m_maxValue) / 2.0));
  painter.drawText(QRectF(barRect.right() + 4, barRect.bottom() - 10, kLabelWidth, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, formatValue(m_minValue));
}

}  // namespace digitqt::gui::canvas
