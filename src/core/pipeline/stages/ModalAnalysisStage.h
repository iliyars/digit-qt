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
 *   - AnalyticZernike — базис термов взят напрямую из референсного
 *     инструмента (DAPPSIM, Includes/Fitting.cpp, метод Seregin:
 *     GetTiltSeregin/GetPowerSeregin/GetAstigSeregin/GetComaSeregin/
 *     GetS3Seregin), а не из "учебниковых" нормированных полиномов
 *     Цернике -- коэффициенты выходят в тех же единицах, что и у
 *     референсного инструмента;
 *   - GramSchmidtOnAperture — тот же (по форме) базис термов, но
 *     ортогонализуется численно прямо по фактическим точкам апертуры
 *     этого снимка; не зависит от формы апертуры, но результат привязан
 *     к конкретным данным.
 * Оба способа дают одинаковое качество подгонки (RMS/PV остатка), но
 * разные индивидуальные коэффициенты по отдельным термам — см.
 * ModalAnalysisStage.cpp за подробным разбором соответствия термам
 * DAPPSIM.
 */
class ModalAnalysisStage : public PipelineStage {
public:
  ModalAnalysisStage() : PipelineStage(StageId::S5) {}

protected:
  bool doCompute(digitqt::core::Measurement &measurement, QString &errorMessage) override;
};

}  // namespace digitqt::core::pipeline
