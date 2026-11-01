# Первоисточники и дополнительное чтение

Курс содержит самостоятельные выводы и учебные реализации. Статьи ниже нужны для
проверки исходных идей и дальнейшего углубления; обозначения и нормировки могут
отличаться. Наличие ссылки не означает дословного воспроизведения алгоритма статьи.

| Занятия | Источник | Для чего читать |
|---|---|---|
| 01 | [University of Arizona: Interference](https://webs.optics.arizona.edu/gsmith/interference.html) | Вывод интенсивности двух волн и физические предпосылки |
| 04 | [SciPy: CubicSpline](https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.CubicSpline.html) | Граничные условия и поведение интерполятора |
| 04 | [SciPy: Extrapolation tips and tricks](https://docs.scipy.org/doc/scipy/tutorial/interpolate/extrapolation_examples.html) | Почему natural spline не означает линейную экстраполяцию |
| 05 | [Takeda, Ina, Kobayashi, 1982](https://doi.org/10.1364/JOSA.72.000156) | Fourier-transform method of fringe-pattern analysis for computer-based topography and interferometry |
| 06 | [Zhong, Weng, 2004](https://opg.optica.org/ao/abstract.cfm?uri=ao-43-26-4993) | Spatial carrier-fringe pattern analysis by means of wavelet transform: wavelet transform profilometry |
| 07 | [Itoh, 1982](https://opg.optica.org/ao/abstract.cfm?uri=ao-21-14-2470) | Analysis of the phase unwrapping algorithm |
| 09 | [Liu et al., 2013 — запись PubMed](https://pubmed.ncbi.nlm.nih.gov/24514503/) | PSI с неизвестными сдвигами и пространственными наклонами; сверяйте конкретную итерационную схему со статьёй |
| 00, 08, 10 | [NumPy: numpy.linalg.lstsq](https://numpy.org/doc/stable/reference/generated/numpy.linalg.lstsq.html) | Ранг, сингулярные числа и решение линейного МНК |
| 09 | [SciPy: least_squares](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.least_squares.html) | Нелинейный МНК, критерии остановки и масштабирование |

Материалы, уже находившиеся в проекте: [раздел PSI](../Phase-shift/),
[полиномиальный анализ](../Polynomials/). Их нужно читать с учётом соглашений
о знаке, единицах и ориентации осей, описанных в уроке 00.

Для следующего уровня полезно отдельно изучить двумерную развёртку с разрезами
и глобальным МНК, статистическую оценку неопределённости, устойчивую регрессию,
оптическую передаточную функцию и модели спекл-интерферометрии. Этот курс даёт
основание для такого чтения, а не исчерпывает все варианты этих методов.
