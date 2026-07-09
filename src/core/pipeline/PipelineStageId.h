#pragma once

#include <QString>

namespace digitqt::core::pipeline {

/**
 * @brief Canonical processing stages.
 *
 * Follows the application master specification's "5. Processing Pipeline
 * (Canonical)" section, with a deliberate simplification: everything up
 * through fringe tracing -- S0 (Raw Images), S0a (Aperture & Visibility
 * Masking), S0b (Reference Markers), S0c (Geometric Calibration), and S1
 * (Amplitude Digitization / fringe tracing) -- is one continuous piece of
 * work in practice: open the image, draw the aperture, place seed points,
 * run the tracer, all in the same screen/session. So it's a single Setup
 * stage/tree node here, not five. Internally each concern still has its
 * own data/classes on Measurement (ShapeCollection, FringeTracingData,
 * separate undo commands, separate controllers) -- only the pipeline
 * tree/UI granularity is coarser than the spec's stage numbering.
 *
 * This order is also the dependency/invalidation order: changing
 * something at stage X potentially invalidates every stage after it.
 */
enum class StageId {
  Setup,  // S0 + S0a + S0b + S0c + S1: image, aperture masking, reference
          // markers, geometric calibration, fringe tracing
  S2,     // Phase Reconstruction
  S3,     // Phase Unwrapping
  S4,     // Wavefront Reconstruction
  S4b,    // Wavefront Calibration (subtraction)
  S5,     // Polynomial / Modal Analysis
  S6,     // Diffraction Analysis
  S7,     // Interferogram Synthesis
};

/// Short code as used in the spec and the pipeline tree, e.g. "S0a".
QString shortName(StageId id);

/// Human-readable name, e.g. "Aperture & Visibility Masking".
QString displayName(StageId id);

}  // namespace digitqt::core::pipeline
