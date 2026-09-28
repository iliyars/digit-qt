#pragma once

namespace digitqt::core {

/**
 * @brief 2D аффинное преобразование: x' = a*x + b*y + c, y' = d*x + e*y + f.
 *
 * Представляет G2 (СКИ -> СКОС, см. architecture.md) -- сдвиг, масштаб,
 * поворот и срез все укладываются в эти 6 коэффициентов; нелинейной
 * дисторсии здесь нет -- это отдельная, более сложная задача, не
 * покрытая этим типом.
 */
struct AffineTransform2D {
  double a = 1.0, b = 0.0, c = 0.0;
  double d = 0.0, e = 1.0, f = 0.0;

  struct Point {
    double x = 0.0;
    double y = 0.0;
  };

  Point apply(Point p) const { return {a * p.x + b * p.y + c, d * p.x + e * p.y + f}; }
};

}  // namespace digitqt::core
