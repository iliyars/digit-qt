"""Синтетические серии кадров для ручной проверки PSI в DigitQt.

Запуск: python generate.py   (из этой папки; только стандартная библиотека)

Модель кадра n:  I = A + B*cos(phi(x,y) + delta_n(x,y)), округление до 0..255.
Волновой фронт (в волнах, u,v в [-1,1] по апертуре):
    bowl:  W = DEFOCUS * (u^2 + v^2) + TILT_X * u
    tilt:  W = TILT_ONLY_X * u + TILT_ONLY_Y * v        (прямые полосы)
    flat:  W = FLAT_DEFOCUS * (u^2 + v^2)              (меньше одной полосы)
т.е. "чаша": на краю апертуры фаза на DEFOCUS порядков больше, чем в центре.
Сдвиг кадра -- число delta_n или, у серий с наклоном, плоскость
    delta_n + 2*pi*(tiltX_n * x' + tiltY_n * y'),  x', y' -- от центра кадра,
    в долях ширины (высоты) кадра; tilt -- в волнах на ширину (высоту) кадра,
    в тех же единицах, что DigitQt показывает в строке состояния.
Вне апертуры полос нет (только фон A) -- апертуру в программе можно не
рисовать, такие точки отсеются по модуляции.

В каждой папке: frame1..N.bmp, series.idf (открывать его через
File -> Open Phase-Shifting Series...) и truth.txt -- что заложено и что
должна показать программа.
"""

import math
import os
import random
import struct

SIZE = 512
RADIUS = 0.45 * SIZE          # радиус апертуры в пикселях
A, B = 110.0, 90.0            # фон и размах полос (модуляция B/A = 0.82)
DEFOCUS = 3.0                 # волн: край выше центра на 3 порядка
TILT_X = 0.5                  # волн на радиус апертуры

TILT_ONLY_X = 4.0   # волн на радиус: ~8 прямых полос поперёк апертуры
TILT_ONLY_Y = 1.5
FLAT_DEFOCUS = 0.15  # волн: почти плоская деталь без несущей

FIG4 = [0.0, 72.0, 198.0, 266.4, 36.0]
FIG8 = [0.0, 10.0, 110.0, 30.0]
# Наклон сдвига между кадрами (вибрация, перекос пьезо), волн на кадр.
TILT_BETWEEN_X = [0.0, 0.3, 0.6, 0.2, 0.5]
TILT_BETWEEN_Y = [0.0, -0.2, 0.1, 0.4, -0.3]

# имя папки: (сдвиги в градусах, шум в уровнях яркости, фронт,
#             наклоны сдвига x/y или None, гауссов пучок, что проверять)
SERIES = {
    "A_article_fig4": (FIG4, 0.0, "bowl", None, False,
                       "Неравные сдвиги рис. 4 статьи. С уточнением сдвиги находятся "
                       "точно, невязка ~0,2 уровня, остаток S5 ~0,0004 волны. Без "
                       "уточнения (номинальные 90): невязка ~13 уровней, остаток S5 "
                       "~0,03 волны -- рябь на удвоенной частоте полос."),
    "B_article_fig8": (FIG8, 0.0, "bowl", None, False,
                       "Сдвиги не по порядку (рис. 8). Зеркальное решение ближе к "
                       "номинальным 90, чем настоящее, и программа приходит к нему: "
                       "сдвиги 0/350/250/330, невязка мала, БЕЗ предупреждения -- а "
                       "чаша ПЕРЕВЁРНУТА (дефокус S5 около -3 вместо +3). Известное "
                       "ограничение, лекция 6.3: по кадрам знак не узнать."),
    "C_ideal_90": ([0.0, 90.0, 180.0, 270.0], 0.0, "bowl", None, False,
                   "Идеальные 90 градусов: с уточнением и без -- одинаково."),
    "D_fig4_noisy": (FIG4, 3.0, "bowl", None, False,
                     "Как A, плюс шум камеры 3 уровня: сдвиги с точностью "
                     "в сотые доли градуса, невязка ~2 уровня."),
    # Как в самой статье: только наклон + постоянная, прямые полосы.
    "E_fig4_tilt_only": (FIG4, 0.0, "tilt", None, False,
                         "Как в статье: прямые полосы. RMS остатка S5 (λ, без двойного "
                         "хода): с номинальными 90 ~0,0298, с найденными ~0,0004 "
                         "(статья: 0,02978 и 0,00031)."),
    "F_fig8_tilt_only": (FIG8, 0.0, "tilt", None, False,
                         "Как B на прямых полосах."),
    "G_tilt_between_frames": (FIG4, 0.0, "tilt", (TILT_BETWEEN_X, TILT_BETWEEN_Y), False,
                              "Сдвиг каждого кадра -- плоскость. Без 'Compensate tilt': "
                              "предупреждение о расхождении половин и большая невязка. "
                              "С 'Compensate tilt': наклоны находятся (строка состояния, "
                              "волн на кадр), невязка -- доли уровня."),
    "H_few_fringes": (FIG4, 2.0, "flat", None, True,
                      "Ловушка: меньше одной полосы на детали, неравномерный пучок, шум. "
                      "Сдвиги определить нельзя: найденные (около 0/21/158/272/12) "
                      "далеки от настоящих, дефокус S5 ~0,20 вместо 0,15. Половины "
                      "при этом совпадают -- тревогу поднимает только невязка (~16 "
                      "уровней, треть амплитуды). Лекция 6.1, тетрадка гл. 12а."),
}


def wavefront_waves(x, y, kind):
    c = (SIZE - 1) / 2.0
    u, v = (x - c) / RADIUS, (y - c) / RADIUS
    if u * u + v * v > 1.0:
        return None
    if kind == "tilt":
        return TILT_ONLY_X * u + TILT_ONLY_Y * v
    if kind == "flat":
        return FLAT_DEFOCUS * (u * u + v * v)
    return DEFOCUS * (u * u + v * v) + TILT_X * u


def beam(x, y):
    """Гауссов пучок: 1 в центре, ~0.5 на краю апертуры."""
    c = (SIZE - 1) / 2.0
    r2 = ((x - c) ** 2 + (y - c) ** 2) / (RADIUS * RADIUS)
    return math.exp(-0.7 * r2)


def write_gray_bmp(path, pixels):
    """8-битный BMP с серой палитрой; pixels[y][x], y=0 -- верхняя строка."""
    row_size = (SIZE + 3) & ~3
    palette = b"".join(bytes((i, i, i, 0)) for i in range(256))
    offset = 14 + 40 + len(palette)
    file_size = offset + row_size * SIZE
    with open(path, "wb") as f:
        f.write(b"BM" + struct.pack("<IHHI", file_size, 0, 0, offset))
        f.write(struct.pack("<IiiHHIIiiII", 40, SIZE, SIZE, 1, 8, 0, row_size * SIZE,
                            2835, 2835, 256, 256))
        f.write(palette)
        pad = bytes(row_size - SIZE)
        for y in range(SIZE - 1, -1, -1):  # BMP хранит строки снизу вверх
            f.write(bytes(pixels[y]) + pad)


def write_idf(path, frame_names):
    """Файл-описание серии, как у программ Серегина (см. src/formats/IdfFile.h)."""
    lines = ["Interferograms descriptive file", "", str(len(frame_names)), str(SIZE),
             str(SIZE)] + frame_names
    with open(path, "wb") as f:
        f.write("\r\n".join(lines).encode("ascii") + b"\r\n")


def write_truth(path, name, shifts_deg, noise, kind, tilts, has_beam, expect):
    def fmt(values):
        return " / ".join("%g" % v for v in values)

    fronts = {
        "bowl": "чаша: W = %g*(u^2+v^2) + %g*u волн" % (DEFOCUS, TILT_X),
        "tilt": "прямые полосы: W = %g*u + %g*v волн" % (TILT_ONLY_X, TILT_ONLY_Y),
        "flat": "почти плоская: W = %g*(u^2+v^2) волн" % FLAT_DEFOCUS,
    }
    lines = [
        "Серия %s" % name,
        "",
        "Настоящие сдвиги, град: %s" % fmt(shifts_deg),
        "Номинальный шаг для программы: 90 град",
        "Фронт: %s (u, v в [-1, 1] по апертуре радиусом %g px)" % (fronts[kind], RADIUS),
        "Шум камеры: %g уровня" % noise,
        "Фон/размах: A = %g, B = %g%s" % (A, B, ", гауссов пучок (0.5 на краю)" if has_beam
                                           else ""),
    ]
    if tilts:
        lines.append("Наклон сдвига x, волн на кадр: %s" % fmt(tilts[0]))
        lines.append("Наклон сдвига y, волн на кадр: %s" % fmt(tilts[1]))
    lines += ["", "Что проверить:", expect, ""]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    rng = random.Random(1)
    kinds = {kind for _, _, kind, _, _, _ in SERIES.values()}
    phases = {kind: [[wavefront_waves(x, y, kind) for x in range(SIZE)] for y in range(SIZE)]
              for kind in kinds}
    c = (SIZE - 1) / 2.0
    for name, (shifts_deg, noise, kind, tilts, has_beam, expect) in SERIES.items():
        phase = phases[kind]
        folder = os.path.join(here, name)
        os.makedirs(folder, exist_ok=True)
        names = []
        for n, shift in enumerate(shifts_deg):
            d = math.radians(shift)
            tx, ty = (tilts[0][n], tilts[1][n]) if tilts else (0.0, 0.0)
            pixels = []
            for y in range(SIZE):
                row = []
                for x in range(SIZE):
                    w = phase[y][x]
                    gain = beam(x, y) if has_beam else 1.0
                    if w is None:
                        value = A * gain
                    else:
                        delta = d + 2 * math.pi * (tx * (x - c) + ty * (y - c)) / (SIZE - 1)
                        value = gain * (A + B * math.cos(2 * math.pi * w + delta))
                    if noise:
                        value += rng.gauss(0.0, noise)
                    row.append(max(0, min(255, int(round(value)))))
                pixels.append(row)
            names.append("frame%d.bmp" % (n + 1))
            write_gray_bmp(os.path.join(folder, names[-1]), pixels)
        write_idf(os.path.join(folder, "series.idf"), names)
        write_truth(os.path.join(folder, "truth.txt"), name, shifts_deg, noise, kind, tilts,
                    has_beam, expect)
        print(name, "->", len(shifts_deg), "frames")


if __name__ == "__main__":
    main()
