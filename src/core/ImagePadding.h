#pragma once

#include <QImage>

namespace digitqt::core {

/**
 * @brief Pads an image with a solid background border.
 *
 * Fringe patterns/apertures that touch the raw image edge cause clipping
 * and fragmentation artifacts in the fringe tracers (e.g. the distance
 * transform in BinaryThinningTracker, the row scan in
 * ScanlineExtremumTracker) -- they have no visibility beyond the image
 * bounds to tell a real edge from an aperture that simply runs off the
 * frame. Adding a margin of background color around the loaded image
 * gives them room so the aperture edge is never the image edge.
 *
 * The background color is the mode of the pixel values sampled along the
 * original image's border (top/bottom rows, left/right columns), so it
 * adapts to whatever background the interferogram was captured against.
 * The image is normalized to Format_Grayscale8 in the process, matching
 * every downstream consumer (they all convertToFormat this way already).
 *
 * @param marginFraction Margin added on each side, as a fraction of the
 *        image's larger dimension.
 */
QImage padImageBackground(const QImage &image, double marginFraction = 0.15);

}  // namespace digitqt::core
