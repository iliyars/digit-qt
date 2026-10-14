"""Синтетические серии кадров для ручной проверки PSI в DigitQt.

Запуск: python generate.py   (из этой папки; только стандартная библиотека)

Модель кадра n:  I = A + B*cos(phi(x,y) + delta_n), округление до 0..255.
Волновой фронт (в волнах, u,v в [-1,1] по апертуре):
    W = DEFOCUS * (u^2 + v^2) + TILT_X * u
т.е. "чаша": на краю апертуры фаза на DEFOCUS порядков больше, чем в центре.
Вне апертуры полос нет (только фон A) -- апертуру в программе можно не
рисовать, такие точки отсеются по модуляции.
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

SERIES = {
    # имя папки: (сдвиги в градусах, шум в уровнях яркости, фронт)
    "A_article_fig4": ([0.0, 72.0, 198.0, 266.4, 36.0], 0.0, "bowl"),
    "B_article_fig8": ([0.0, 10.0, 110.0, 30.0], 0.0, "bowl"),
    "C_ideal_90": ([0.0, 90.0, 180.0, 270.0], 0.0, "bowl"),
    "D_fig4_noisy": ([0.0, 72.0, 198.0, 266.4, 36.0], 3.0, "bowl"),
    # Как в самой статье: только наклон + постоянная, прямые полосы.
    "E_fig4_tilt_only": ([0.0, 72.0, 198.0, 266.4, 36.0], 0.0, "tilt"),
    "F_fig8_tilt_only": ([0.0, 10.0, 110.0, 30.0], 0.0, "tilt"),
}

TILT_ONLY_X = 4.0   # волн на радиус: ~8 прямых полос поперёк апертуры
TILT_ONLY_Y = 1.5


def wavefront_waves(x, y, kind):
    c = (SIZE - 1) / 2.0
    u, v = (x - c) / RADIUS, (y - c) / RADIUS
    if u * u + v * v > 1.0:
        return None
    if kind == "tilt":
        return TILT_ONLY_X * u + TILT_ONLY_Y * v
    return DEFOCUS * (u * u + v * v) + TILT_X * u


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


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    rng = random.Random(1)
    phases = {kind: [[wavefront_waves(x, y, kind) for x in range(SIZE)] for y in range(SIZE)]
              for kind in ("bowl", "tilt")}
    for name, (shifts_deg, noise, kind) in SERIES.items():
        phase = phases[kind]
        folder = os.path.join(here, name)
        os.makedirs(folder, exist_ok=True)
        for n, shift in enumerate(shifts_deg, start=1):
            d = math.radians(shift)
            pixels = []
            for y in range(SIZE):
                row = []
                for x in range(SIZE):
                    w = phase[y][x]
                    value = A if w is None else A + B * math.cos(2 * math.pi * w + d)
                    if noise:
                        value += rng.gauss(0.0, noise)
                    row.append(max(0, min(255, int(round(value)))))
                pixels.append(row)
            write_gray_bmp(os.path.join(folder, "frame%d.bmp" % n), pixels)
        print(name, "->", len(shifts_deg), "frames")


if __name__ == "__main__":
    main()
