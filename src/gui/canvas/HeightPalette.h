#pragma once

#include <QColor>
#include <algorithm>
#include <array>
#include <cmath>

namespace digitqt::gui::canvas {

enum class HeightPalette { Plasma, Viridis, Inferno, Rainbow, FullRainbow };

// Samples of the Matplotlib sequential colormaps, from low to high.
// Shared by both renderers and the legend; RGB interpolation between samples.
inline QColor heightColor(double value, HeightPalette palette) {
  const double t = std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
  // Violet -> blue -> cyan -> green -> yellow -> orange -> red.
  if (palette == HeightPalette::FullRainbow)
    return QColor::fromHsvF((1.0 - t) * 280.0 / 360.0, 1.0, 1.0);
  if (palette == HeightPalette::Rainbow)
    return QColor::fromHsv(static_cast<int>((1.0 - t) * 240.0), 255, 255);
  static constexpr std::array<unsigned, 17> plasma = {
      0x0d0887, 0x310597, 0x4c02a1, 0x6600a7, 0x7e03a8, 0x950ca1,
      0xaa2395, 0xbd3786, 0xcc4778, 0xda5b69, 0xe66c5c, 0xf0804e,
      0xf89540, 0xfdac33, 0xfdc328, 0xf7dd25, 0xf0f921};
  static constexpr std::array<unsigned, 17> viridis = {
      0x440154, 0x48186a, 0x472d7b, 0x424086, 0x3b528b, 0x33638d,
      0x2c728e, 0x26828e, 0x21918c, 0x1fa088, 0x28ae80, 0x3fbc73,
      0x5ec962, 0x84d44b, 0xaddc30, 0xd8e219, 0xfde725};
  static constexpr std::array<unsigned, 17> inferno = {
      0x000004, 0x0b0724, 0x220c4a, 0x390963, 0x57106e, 0x71196e,
      0x8a226a, 0xa32c61, 0xbc3754, 0xd24644, 0xe45a31, 0xf1731d,
      0xf98e09, 0xfbaa0f, 0xf9c932, 0xf2e661, 0xfcffa4};
  const auto &colors = palette == HeightPalette::Viridis ? viridis
                       : palette == HeightPalette::Inferno ? inferno : plasma;
  const double position = t * (colors.size() - 1);
  const auto index = std::min(static_cast<std::size_t>(position), colors.size() - 2);
  const double fraction = position - index;
  const QColor a = QColor::fromRgb(colors[index]);
  const QColor b = QColor::fromRgb(colors[index + 1]);
  const auto mix = [fraction](int x, int y) {
    return static_cast<int>(std::lround(x + fraction * (y - x)));
  };
  return QColor(mix(a.red(), b.red()), mix(a.green(), b.green()), mix(a.blue(), b.blue()));
}

}  // namespace digitqt::gui::canvas
