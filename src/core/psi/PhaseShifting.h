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
  /// То же с готовыми cos(theta), sin(theta) -- когда они уже посчитаны.
  void addCosSin(double cosTheta, double sinTheta, double intensity);
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
  /// Сдвиг каждого кадра в радианах, с которым посчитана wrappedPhase. При
  /// учёте наклона -- сдвиг в центре кадра.
  std::vector<double> shifts;
  /// Только у solveWithTiltShifts(): наклон сдвига каждого кадра, радианы
  /// на пиксель по x и по y (у первого кадра -- нули, наклоны отсчитаны
  /// от него). Сдвиг кадра n в точке (x, y) -- shifts[n] + tiltX[n]*u +
  /// tiltY[n]*v, где u, v -- координаты от центра кадра. Пусто -- сдвиг
  /// один на весь кадр.
  std::vector<double> tiltX, tiltY;
  /// Только у solveWithUnknownShifts(): сколько раз уточнялись сдвиги и
  /// сошёлся ли процесс до PsiOptions::tolerance.
  int iterations = 0;
  bool converged = false;
  /// true, если итерации пришли к зеркальному решению (-phi, -delta) и
  /// оно было заменено на прямое как более близкое к ожидаемым сдвигам
  /// (см. solveWithUnknownShifts()). Повод перепроверить знак карты, если
  /// ожидаемые сдвиги заданы неточно.
  bool mirrored = false;

  /// Диагностика solveWithUnknownShifts() -- насколько найденным сдвигам
  /// можно верить. "Сошлось" само по себе этого не говорит: на кадрах,
  /// где сдвиги не определены, итерации тоже где-нибудь останавливаются.
  struct Diagnostics {
    /// Сколько точек участвовало в оценке сдвигов.
    int shiftPoints = 0;
    /// Сдвиги, найденные по двум половинам этих точек (левой и правой,
    /// верхней и нижней) при одной и той же фазе, -- СКО их расхождения
    /// по кадрам, градусы; из двух разбиений берётся худшее. Около нуля --
    /// сдвиг один на весь кадр и надёжно определён. Большое -- либо полос
    /// слишком мало, чтобы определить сдвиг (меньше одной на точки), либо
    /// сдвиг по полю разный (наклон между кадрами). NaN -- точек не
    /// хватило на половины.
    double halvesDisagreementDeg = 0.0;
    /// СКО отличия кадров от модели a + b*cos(phi + delta) в тех же точках,
    /// в уровнях яркости. Сравнивать с amplitude: у идеальных кадров --
    /// доли уровня (квантование), у реальных растёт от шума, пересвета
    /// (несинусоидальные полосы), мерцания лазера, наклона.
    double residual = 0.0;
    /// Медианная амплитуда полос b в тех же точках, уровни яркости.
    double amplitude = 0.0;
  };
  Diagnostics diagnostics;
};

struct PsiOptions {
  /// Какие точки участвуют в оценке сдвигов -- см.
  /// selectShiftEstimationPoints(): размах яркости по кадрам не меньше
  /// этой доли от 99-го процентиля размаха в апертуре.
  double minFringeSpan = 0.3;
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
 * @brief То же, что computePhase(), но сдвиг кадра -- плоскость: в точке
 * (x, y) он равен shifts[n] + tiltX[n]*u + tiltY[n]*v (u, v -- от центра
 * кадра). tiltX/tiltY -- радианы на пиксель; пустые -- без наклона.
 */
bool computePhaseTilted(const std::vector<Bitmap> &frames, const std::vector<double> &shifts,
                        const std::vector<double> &tiltX, const std::vector<double> &tiltY,
                        const std::function<bool(int, int)> &isVisible, PsiResult &out,
                        std::string &error);

/// Маска точек кадра, по строкам (width*height), 1 -- точка участвует.
using PointMask = std::vector<unsigned char>;

/**
 * @brief Точки, по которым оцениваются сдвиги (шаг 2): в апертуре и с
 * размахом яркости по кадрам (max - min) не меньше minFringeSpan от 99-го
 * процентиля размаха в апертуре. Правило 1 из раздела "Практические
 * результаты" статьи: точки без полос только зашумляют оценку.
 *
 * Почему размах, а не модуляция b/a из шага 1:
 * - размах не зависит от текущей оценки сдвигов, поэтому маска
 *   считается один раз; маска по b/a менялась от итерации к итерации, и
 *   на реальных кадрах итерации ходили по кругу, не сходясь;
 * - b/a велико и там, где полос нет, но темно: при фоне в несколько
 *   уровней шум камеры даёт "модуляцию" 0.3 и выше, и в оценку сдвигов
 *   попадали тёмные углы кадра.
 */
PointMask selectShiftEstimationPoints(const std::vector<Bitmap> &frames,
                                      const std::function<bool(int, int)> &isVisible,
                                      double minFringeSpan);

/**
 * @brief Шаг 2: фаза известна -> реальный сдвиг каждого кадра.
 * Формулы (12)-(15).
 *
 * Зеркало computePhase(): там для каждого ПИКСЕЛЯ подгонка идёт по
 * кадрам (theta = сдвиг кадра), здесь для каждого КАДРА -- по пикселям
 * (theta = фаза в пикселе).
 *
 * @param current Результат computePhase() -- текущая оценка фазы.
 * @param points Точки, по которым оценивать (см.
 *        selectShiftEstimationPoints()); из них берутся те, где фаза
 *        определена.
 * @param shifts Найденные сдвиги в радианах, приведённые так, что
 *        shifts[0] == 0, остальные в [0, 2*pi).
 */
bool estimateShifts(const std::vector<Bitmap> &frames, const PsiResult &current,
                    const PointMask &points, std::vector<double> &shifts, std::string &error);

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
 *        (результат всё равно возвращается: обычно он уже пригоден);
 *        out.diagnostics -- насколько сдвигам можно верить.
 */
bool solveWithUnknownShifts(const std::vector<Bitmap> &frames,
                            const std::vector<double> &expectedShifts,
                            const std::function<bool(int, int)> &isVisible,
                            const PsiOptions &options, PsiResult &out, std::string &error);

/**
 * @brief Как solveWithUnknownShifts(), но сдвиг каждого кадра -- плоскость
 * (наклон между кадрами от вибраций или перекоса фазосдвигателя).
 *
 * Метод: Q. Liu, Y. Wang, F. Ji, J. He, "A three-step least-squares
 * iterative method for tilt phase-shift interferometry", Opt. Express 21,
 * 29505 (2013). Идея: x- и y-части наклона независимы, а в пределах одного
 * столбца x-часть сдвига постоянна (в строке -- y-часть). Поэтому каждая
 * итерация -- три линейные подгонки той же формы, что CosineFit:
 *   1) фаза в каждой точке при известных плоскостях сдвига;
 *   2) сдвиг каждого столбца каждого кадра -> развёртка вдоль x и прямая ->
 *      наклон по x и сдвиг;
 *   3) то же по строкам -> наклон по y.
 * В каждом столбце и строке нужна хотя бы одна полоса.
 *
 * Отступление от статьи: итерации запускаются с трёх стартов -- без
 * наклона и с наклоном, оценённым по сдвигу пика спектра кадра
 * относительно первого (двумя знаками: по спектру его не определить), --
 * и берётся решение с наименьшей невязкой. С нулевого наклона, как в
 * статье, итерации не находят большой наклон (несколько полос), а старт
 * по спектру портит кадры без преобладающей частоты полос (кольца).
 */
bool solveWithTiltShifts(const std::vector<Bitmap> &frames,
                         const std::vector<double> &expectedShifts,
                         const std::function<bool(int, int)> &isVisible,
                         const PsiOptions &options, PsiResult &out, std::string &error);

}  // namespace digitqt::core::psi
