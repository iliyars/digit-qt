#pragma once

#include <QString>

namespace digitqt::core::pipeline {
/**
 * @brief Canonical processing stages, per the application master
 * specification ("5. Processing Pipeline (Canonical)").
 *
 * This order is also the dependency/invalidation order: changing
 * something at stage X potentially invalidates every stage after it.
 */
enum class StageId {
  S0,  // Raw Images
  S0a, // Aperture & Visibility Masking
  S0b, // Reference Markers
  S0c, // Geometric Calibration
  S1,  // Amplitude Digitization (fringe tracing)
  S2,  // Phase Reconstruction
  S3,  // Phase Unwrapping
  S4,  // Wavefront Reconstruction
  S4b, // Wavefront Calibration (subtraction)
  S5,  // Polynomial / Modal Analysis
  S6,  // Diffraction Analysis
  S7,  // Interferogram Synthesis
};

/// Short code as used in the spec and the pipeline tree, e.g. "S0a".
QString shortName(StageId id);

/// Human-readable name, e.g. "Aperture & Visibility Masking".
QString displayName(StageId id);

} // namespace digitqt::core::pipeline
