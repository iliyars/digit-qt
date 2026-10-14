#pragma once

#include "core/Bitmap.h"
#include "core/PhaseMap.h"

#include <functional>
#include <string>
#include <vector>

/**
 * Фазово-сдвиговая интерферометрия (PSI): фаза по серии кадров одной и
 * той же интерферограммы, снятых с разным сдвигом фазы опорной волны.
 *
 * Реализация по статье: М. А. Парпин, Д. А. Серегин, "Алгоритм калибровки
 * и расчёта фазы при работе на интерферометре фазового сдвига", Вопросы
 * радиоэлектроники, сер. Техника телевидения, 2012, вып. 1 (лежит в
 * notes/). Номера формул в комментариях -- оттуда.
 *
 * Модель одного пикселя в кадре n:
 *
 *   I = a + b*cos(phi + delta_n)
 *     = a + c*cos(delta_n) - s*sin(delta_n),   c = b*cos(phi), s = b*sin(phi)
 *
 * Вторая строка линейна по (a, c, s) при известном delta_n -- и, по
 * симметрии формулы, линейна по (a, b*cos(delta_n), b*sin(delta_n)) при
 * известном phi. Поэтому оба шага алгоритма -- одна и та же подгонка
 * I ~ a + c*cos(theta) - s*sin(theta) методом наименьших квадратов (см.
 * CosineFit), только с разным смыслом theta.
 */
namespace digitqt::core::psi {

/**
 * @brief МНК-подгонка I ~ a + c*cos(theta) - s*sin(theta) по набору пар
 * (theta, I): копит нормальные уравнения 3x3, потом решает их.
 *
 * После solve() угол, "спрятанный" в (c, s), равен atan2(s, c), а размах
 * косинуса -- sqrt(c*c + s*s).
 */
class CosineFit {
public:
  void add(double theta, double intensity);
  int count() const { return m_count; }

  /// @return false, если система вырождена (меньше трёх различных theta).
  bool solve(double &a, double &c, double &s) const;

private:
  double m_matrix[3][3] = {};
  double m_rhs[3] = {};
  int m_count = 0;
};

struct PsiResult {
  /// Фаза в радианах, свёрнутая в (-pi, pi]; NaN вне апертуры. (PhaseMap
  /// здесь -- просто сетка double с NaN; "порядки полосы" из его описания
  /// появятся только после развёртки.)
  PhaseMap wrappedPhase;
  /// Модуляция b/a, формула (8): 1 -- полосы во весь размах, 0 -- полос нет.
  PhaseMap modulation;
  /// Сдвиг каждого кадра в радианах, с которым посчитана wrappedPhase.
  std::vector<double> shifts;
  /// Только у solveWithUnknownShifts(): сколько раз уточнялись сдвиги и
  /// сошёлся ли процесс до PsiOptions::tolerance.
  int iterations = 0;
  bool converged = false;
  /// true, если итерации пришли к зеркальному решению (-phi, -delta) и
  /// оно было заменено на прямое как более близкое к ожидаемым сдвигам
  /// (см. solveWithUnknownShifts()). Повод перепроверить знак карты, если
  /// ожидаемые сдвиги заданы неточно.
  bool mirrored = false;
};

struct PsiOptions {
  /// Точки с модуляцией ниже порога не участвуют в оценке сдвигов.
  double minModulation = 0.3;
  /// Порог из (16): итерации прекращаются, когда сдвиги за шаг меняются
  /// меньше чем на эту величину (СКО по кадрам, радианы). По умолчанию
  /// 0.001 градуса -- в статье 0.0005 градуса потребовали 39 итераций.
  double tolerance = 0.001 * 3.14159265358979323846 / 180.0;
  int maxIterations = 200;
};

/**
 * @brief Шаг 1: сдвиги известны -> фаза и модуляция в каждой точке.
 * Формулы (6)-(8).
 *
 * @param frames Не меньше трёх кадров одного размера.
 * @param shifts Сдвиг каждого кадра в радианах (shifts.size() == frames.size()).
 * @param isVisible Апертура; точки вне её остаются NaN.
 * @param out Заполняется при успехе.
 * @param error Заполняется при неудаче.
 */
bool computePhase(const std::vector<Bitmap> &frames, const std::vector<double> &shifts,
                  const std::function<bool(int, int)> &isVisible, PsiResult &out,
                  std::string &error);

/**
 * @brief Шаг 2: фаза известна -> реальный сдвиг каждого кадра.
 * Формулы (12)-(15).
 *
 * Зеркало computePhase(): там для каждого ПИКСЕЛЯ подгонка идёт по
 * кадрам (theta = сдвиг кадра), здесь для каждого КАДРА -- по пикселям
 * (theta = фаза в пикселе).
 *
 * В расчёте участвуют только точки, где фаза определена (не NaN) и
 * модуляция не ниже minModulation -- правило 1 из раздела "Практические
 * результаты": точки без полос только зашумляют оценку.
 *
 * @param current Результат computePhase() -- текущая оценка фазы.
 * @param minModulation Порог модуляции, 0..1.
 * @param shifts Найденные сдвиги в радианах, приведённые так, что
 *        shifts[0] == 0, остальные в [0, 2*pi).
 */
bool estimateShifts(const std::vector<Bitmap> &frames, const PsiResult &current,
                    double minModulation, std::vector<double> &shifts, std::string &error);

/**
 * @brief Весь алгоритм статьи (рис. 3): шаг 1 и шаг 2 по очереди, пока
 * сдвиги не перестанут меняться.
 *
 * Для кадров, снятых с вибрациями, когда реальные сдвиги неизвестны и
 * могут сильно отличаться от ожидаемых (в статье: ожидались 90 градусов,
 * на деле 0/10/110/30).
 *
 * @param expectedShifts Ожидаемые ("паспортные") сдвиги в радианах --
 *        начальное приближение. Задают и знак результата: решение с
 *        одновременно обращёнными знаками фазы и сдвигов уравнениям
 *        удовлетворяет так же хорошо, и из двух выбирается то, чьи сдвиги
 *        ближе к ожидаемым (см. PsiResult::mirrored). Если реальные сдвиги
 *        ушли от ожидаемых дальше, чем зеркальные, знак карты будет
 *        неверным -- по самим кадрам этого не узнать.
 * @param out При успехе -- фаза и модуляция с последними сдвигами;
 *        out.converged == false, если за maxIterations порог не достигнут
 *        (результат всё равно возвращается: обычно он уже пригоден).
 */
bool solveWithUnknownShifts(const std::vector<Bitmap> &frames,
                            const std::vector<double> &expectedShifts,
                            const std::function<bool(int, int)> &isVisible,
                            const PsiOptions &options, PsiResult &out, std::string &error);

}  // namespace digitqt::core::psi
