#pragma once

#include <limits>
#include <vector>

namespace digitqt::core {

/**
 * @brief Плтоная карта восстановленной фазы, в единицах порядка плосы (ещё не переведена в радианы
 * или физическую длину волны)
 *
 * Значение вне апертуры - NaN. Индексация как у изображения: (0,0) - левый верхний угол, x растёт
 * вправо, y растёт вниз
 */
class PhaseMap {
public:
  PhaseMap() = default;

  PhaseMap(int width, int height)
      : m_width(width),
        m_height(height),
        m_values(static_cast<size_t>(width) * static_cast<size_t>(height),
                 std::numeric_limits<double>::quiet_NaN()) {}

  int width() const { return m_width; }
  int height() const { return m_height; }
  bool isEmpty() const { return m_width <= 0 || m_height <= 0; }

  // Значение в пикселе (x,y), или NaN, если пиксель вне сетки/апертуры
  double value(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
      return std::numeric_limits<double>::quiet_NaN();
    return m_values[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
  }

  void setValue(int x, int y, double v) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
      return;
    m_values[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)] = v;
  }

  /// true, если значение определено (не NaN).
  bool hasValue(int x, int y) const {
    const double v = value(x, y);
    return v == v;  // для NaN сравнение всегда false
  }

  void clear() {
    m_width = 0;
    m_height = 0;
    m_values.clear();
  }

private:
  int m_width = 0;
  int m_height = 0;
  std::vector<double> m_values;
};

}  // namespace digitqt::core
