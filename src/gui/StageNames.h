#pragma once

#include "core/pipeline/PipelineStageId.h"

#include <QCoreApplication>
#include <QString>

namespace digitqt::gui {

/// Название этапа для интерфейса, в переводе. Core Qt-свободен и отдаёт
/// английское displayName(); QT_TRANSLATE_NOOP здесь -- чтобы lupdate
/// увидел эти строки (они должны совпадать с PipelineStageId.cpp).
inline QString stageDisplayName(digitqt::core::pipeline::StageId id) {
  static const char *const kNames[] = {
      QT_TRANSLATE_NOOP("PipelineStage", "Setup (Image, Aperture, Markers, Fringe Tracing)"),
      QT_TRANSLATE_NOOP("PipelineStage", "Phase Reconstruction"),
      QT_TRANSLATE_NOOP("PipelineStage", "Wavefront Reconstruction"),
      QT_TRANSLATE_NOOP("PipelineStage", "Wavefront Calibration"),
      QT_TRANSLATE_NOOP("PipelineStage", "Polynomial / Modal Analysis"),
      QT_TRANSLATE_NOOP("PipelineStage", "Diffraction Analysis"),
      QT_TRANSLATE_NOOP("PipelineStage", "Interferogram Synthesis"),
  };
  (void)kNames;
  const std::string name = digitqt::core::pipeline::displayName(id);
  return QCoreApplication::translate("PipelineStage", name.c_str());
}

}  // namespace digitqt::gui
