#pragma once

namespace digitqt::core {

/**
 * @brief Точная последовательная схема подгонки DAPPSIM/Seregin --
 * НЕ совместный МНК (см. ModalCoefficients/JointLeastSquares), а прямая
 * транскрипция реального алгоритма референсного инструмента
 * (DAPPSIM, Includes/Fitting.cpp):
 *
 *   GetTiltSeregin   -> {1, x, -y}                       -> piston, tiltX, tiltY
 *   GetPowerSeregin  -> {1, ρ²}  (после вычитания выше)   -> defocus
 *   GetAstigSeregin  -> {1,x,-y,(3x²-1)/2,-xy,(3y²-1)/2}  -> astig4,astig5,astig6
 *   GetComaSeregin   -> {1,x,-y,comaX,comaY,trefX,trefY}  -> comaResidualX/Y, coma*, trefoil*
 *   GetS3Seregin     -> {1,ρ²,ρ⁴}                         -> s3rho2, s3rho4
 *   GetS5Seregin     -> {1,ρ²,ρ⁴,ρ⁶}                      -> s5rho2, s5rho4, s5rho6
 *   GetS7Seregin     -> {1,ρ²,ρ⁴,ρ⁶,ρ⁸}                   -> s7rho2, s7rho4, s7rho6, s7rho8
 *
 * Каждая стадия вычитает уже накопленную сумму ВСЕХ предыдущих термов
 * (включая "интерсепт", если он был сохранён -- сам интерсепт локального
 * МНК на стадиях 2+ не сохраняется и не переносится дальше, ровно как в
 * оригинале) и подгоняет свои новые термы отдельным МНК на остатке --
 * поэтому термы одной "радиальной семьи" (ρ², ρ⁴, ...) появляются
 * ПОВТОРНО на нескольких стадиях (S3/S5/S7), каждый раз как поправка к
 * тому, что предыдущие стадии не объяснили.
 *
 * Побайтово подтверждено (см. test_data/Terminal.pol -- сырой дамп
 * коэффициентов самого DAPPSIM для test_data/Terminal.bmp) только
 * piston/tiltX/tiltY/defocus: это первые 4 числа .pol-дампа, совпадающие
 * с полями D/Lx/Ly/C в test_data/report.txt. Остальные поля здесь --
 * честная транскрипция алгоритма DAPPSIM, но НЕ подтверждены против
 * конкретных меток B0-B8/C(кома)/FIC того же report.txt: сам этот текстовый
 * отчёт формирует WinFringe, а не DAPPSIM, и исходников WinFringe нет --
 * как именно WinFringe объединяет/подписывает оставшиеся 18 коэффициентов
 * .pol-дампа в B0..B8/C/FIC, доподлинно неизвестно.
 */
struct SequentialSereginResult {
  double piston = 0.0;
  double tiltX = 0.0;
  double tiltY = 0.0;
  double defocus = 0.0;  // "C" в report.txt -- подтверждено побайтово через Terminal.pol

  double astig4 = 0.0;  // коэффициент (3x²-1)/2
  double astig5 = 0.0;  // коэффициент -xy
  double astig6 = 0.0;  // коэффициент (3y²-1)/2

  double comaResidualX = 0.0;  // остаточный линейный член на стадии комы (обычно ≈0)
  double comaResidualY = 0.0;
  double comaX = 0.0;
  double comaY = 0.0;
  double trefoilX = 0.0;
  double trefoilY = 0.0;

  double s3rho2 = 0.0;
  double s3rho4 = 0.0;
  double s5rho2 = 0.0;
  double s5rho4 = 0.0;
  double s5rho6 = 0.0;
  double s7rho2 = 0.0;
  double s7rho4 = 0.0;
  double s7rho6 = 0.0;
  double s7rho8 = 0.0;

  /// Сумма интерсептов локальных МНК стадий 2-7 (GetPowerSeregin ...
  /// GetS7Seregin) -- сам DAPPSIM их не сохраняет ни в один B0-B8-подобный
  /// коэффициент (Fitting.cpp просто не читает Coeff_mnk.Get(0,0) на этих
  /// стадиях), но здесь они всё равно учтены при подсчёте остатка/RMS,
  /// иначе остаток искусственно не сходится к нулю даже на данных, которые
  /// идеально описываются низкочастотной моделью (см. комментарий в
  /// ModalAnalysisStage.cpp::fitSequentialSeregin). Ненулевое значение
  /// здесь -- диагностика: сколько "потерялось" бы, если бы мы (как сам
  /// DAPPSIM) отбросили эти интерсепты совсем.
  double discardedIntercepts = 0.0;

  // RMS всего волнового фронта (не остатка одной стадии, а полной карты
  // минус накопленная сумма термов) на конец каждой стадии.
  double rmsInitial = 0.0;
  double rmsAfterTiltDefocus = 0.0;
  double rmsAfterAstig = 0.0;
  double rmsAfterComa = 0.0;
  double rmsAfterS3 = 0.0;
  double rmsAfterS5 = 0.0;
  double rmsAfterS7 = 0.0;
};

}  // namespace digitqt::core
