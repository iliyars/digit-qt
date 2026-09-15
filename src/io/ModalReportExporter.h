#pragma once

#include <QString>

namespace digitqt::core {
class Measurement;
}

namespace digitqt::core::io {

/**
 * @brief Экспорт результата S5 (Polynomial / Modal Analysis) в читаемый
 * текстовый отчёт.
 *
 * Содержит ровно то же, что показывает страница S5 в ParametersDock
 * (см. ModalAnalysisReportData -- общий источник чисел для экрана и
 * файла, чтобы они не могли разойтись), плюс необработанные X/Y
 * составляющие для астигматизма/комы/трефойла (на экране показаны только
 * модуль+угол) и метаданные подгонки (базис термов, способ решения МНК,
 * длина волны).
 *
 * @param path Путь для сохранения (обычно с расширением .txt).
 * @param measurement Измерение, чей modalAnalysis() экспортируется.
 * @param errorMessage Заполняется при неудаче (в т.ч. если S5 ещё не
 * посчитан).
 */
bool writeModalReport(const QString &path, const digitqt::core::Measurement &measurement,
                      QString &errorMessage);

}  // namespace digitqt::core::io
