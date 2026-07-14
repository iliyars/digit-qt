#pragma once

#include <QString>
#include <QWidget>

namespace digitqt::gui::canvas {
/**
 * @brief Цветовая шкала для тепловой карты, показывает, какому цвету какое значение соответствует.
 *
 */
class ColorLegendWidget : public QWidget {
  Q_OBJECT
public:
  explicit ColorLegendWidget(QWidget *parent = nullptr);

  // unitduffix для волнового фронта "нм", для карты фазы - пустая строка.
  void setRange(double minValue, double maxValue, const QString &unitSuffix);

  // Карта ещё не посчитана -- показать заглушку вместо шакалы.
  void setNoData();

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  double m_minValue = 0.0;
  double m_maxValue = 0.0;
  QString m_unitSuffix;
  bool m_hasData = false;
};

}  // namespace digitqt::gui::canvas
