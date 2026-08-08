#pragma once

#include "core/pipeline/PipelineStage.h"

namespace digitqt::core::pipeline {

/**
 * @brief S5: Polynomial / Modal Analysis.
 *
 * Отделяет "неинтересную" геометрию измерения (пистон, наклон, дефокус)
 * и классические аберрации формы (астигматизм, кома, трилистник,
 * сферическая аберрация 3-го порядка) от восстановленного волнового
 * фронта  методом наименьших квадратов. Остаток
 * (Measurement::modalAnalysis().residual) — это то, что не описывается
 * этим набором классических термов.
 *
 * Считается одним из двух способов (см. Measurement::modalFitMethod() / core::ModalFitMethod):
 *   - AnalyticZernike — фиксированные аналитические формулы полиномов
 *     Цернике на единичном круге;
 *   - GramSchmidtOnAperture — та же иерархия термов, но ортогонализуется
 *     численно прямо по фактическим точкам апертуры этого снимка; не
 *     зависит от формы апертуры, но результат привязан к конкретным
 *     данным.
 * Оба способа дают одинаковое качество подгонки (RMS/PV остатка), но
 * разные индивидуальные коэффициенты по отдельным термам — см.
 * ModalAnalysisStage.cpp за подробным разбором и ссылкой на то, как это
 * устроено в оригинальном проекте Digit.
 */
class ModalAnalysisStage : public PipelineStage {
public:
  ModalAnalysisStage() : PipelineStage(StageId::S5) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) override;
};

}  // namespace digitqt::core::pipeline
