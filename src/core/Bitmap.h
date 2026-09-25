#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace digitqt::core {
/**
 * @brief 8-битное grayscale растровое изображение (замена QImage).
 *
 * Row-major, без выравнивания/padding (stride всегда равен width) --
 * специально плоский layout, чтобы этот тип можно было отдать через
 * C ABI как простой (width, height, pixels) view без переделки.
 *
 * Индексация как у изображения: (0,0) -- левый верхний угол, x растёт вправо,
 * y -- вниз.
 */
class Bitmap {
public:
  Bitmap() = default;

  Bitmap(int width, int height)
      : m_width(width),
        m_height(height),
        m_pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0) {}

  int width() const { return m_width; }
  int height() const { return m_height; }
  bool isNull() const { return m_width <= 0 || m_height <= 0; }

  const uint8_t *data() const {
    return m_pixels.data();
  }
  uint8_t *data() { return m_pixels.data(); }

  // Указатель на начало строки y --- замена QImage::constScanLine()/
  // scanLine(), но без bytesPerLine(): здесь stride == width всегда.
  const uint8_t *scanLine(int y) const {
    return m_pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(m_width);
  }
  uint8_t *scanLine(int y) {
    return m_pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(m_width);
  }

  uint8_t pixel(int x, int y) const {
    return m_pixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
  }

  void setPixel(int x, int y, uint8_t v) {
    m_pixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)] = v;
  }

  void fill(uint8_t value) {
    std::fill(m_pixels.begin(), m_pixels.end(), value);
  }

  void clear() {
    m_width = 0;
    m_height = 0;
    m_pixels.clear();
  }

private:
  int m_width = 0;
  int m_height = 0;
  std::vector<uint8_t> m_pixels;
};
}  // namespace digitqt::core
