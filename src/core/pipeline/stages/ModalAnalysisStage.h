#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {

/**
 * @brief S5: Polynomial / Modal Analysis.
 *
 * Отделяет "неинтересную" геометрию измерения (пистон, наклон, дефокус)
 * и классические аберрации формы (астигматизм, кома, трилистник,
 * сферическая аберрация 3-го порядка) от восстановленного волнового
 * фронта (S4) методом наименьших квадратов. Остаток
 * (Measurement::modalAnalysis().residual) — это то, что не описывается
 * этим набором классических термов.
 *
 * См. ModalAnalysisResult.h за полным разбором базиса и ссылкой на то,
 * как это устроено в оригинальном проекте Digit.
 */
class ModalAnalysisStage : public PipelineStage {
public:
  ModalAnalysisStage() : PipelineStage(StageId::S5) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) override;
};

}  // namespace digitqt::core::pipeline
