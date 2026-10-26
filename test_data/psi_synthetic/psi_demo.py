"""Демонстрация PSI в DigitQt: своя серия кадров -> расчёт программой -> сверка с правдой.

1. Генерирует серию кадров I = A + B*cos(phi + delta_n(x, y)) с заданными
   сдвигами, наклонами сдвига, фронтом, шумом (как generate.py, но параметры
   -- из командной строки).
2. Прогоняет её через НАСТОЯЩИЙ конвейер DigitQt (tools/psi_check: S2 PSI ->
   S4 -> S5) в нескольких режимах.
3. Сравнивает найденные сдвиги, наклоны и карту с заложенными, печатает
   таблицу и сохраняет картинку (правда / карта DigitQt / ошибка).

Нужны numpy и matplotlib, собранный psi_check (-DDIGITQT_BUILD_TOOLS=ON).

Примеры (из этой папки):
    python psi_demo.py                                  # сдвиги рис. 4 статьи
    python psi_demo.py --shifts 0,90,180,270            # идеальный шаг
    python psi_demo.py --tilt-x 0,0.3,0.6,0.2,0.5 --tilt-y 0,-0.2,0.1,0.4,-0.3
    python psi_demo.py --noise 3 --beam
    python psi_demo.py --defocus 0.15 --carrier-x 0     # ловушка: мало полос
    python psi_demo.py --shifts 0,10,110,30             # зеркальный знак
    python psi_demo.py --defocus 1 --spherical 0.5 --coma-x 0.3 --astig-y 0.2

Единицы: сдвиги -- градусы; фронт -- волны, коэффициенты при полиномах
базиса Серегина (см. ABERRATIONS, --help) -- S5 DigitQt должен вернуть их же;
наклон сдвига -- волны на ширину (высоту) кадра, как в отчёте DigitQt.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
MODES = {
    "nominal": (["--no-refine"], "без уточнения (номинальные сдвиги)"),
    "refine": ([], "уточнение сдвигов"),
    "tilt": (["--tilt"], "уточнение + наклон между кадрами"),
}


# Аберрации фронта: параметр -> (имя коэффициента S5, формула, полином).
# Ровно базис Серегина из buildSereginHierarchy() (ModalAnalysisStage.cpp):
# u -- вправо, v -- ВНИЗ по строкам кадра, обе в [-1, 1] по апертуре.
ABERRATIONS = {
    "carrier_x": ("tiltX", "u", lambda u, v: u),
    "carrier_y": ("tiltY", "-v", lambda u, v: -v),
    "defocus": ("defocus", "u^2+v^2", lambda u, v: u * u + v * v),
    "astig_x": ("astigX", "1.5(u^2-v^2)", lambda u, v: 1.5 * (u * u - v * v)),
    "astig_y": ("astigY", "-uv", lambda u, v: -u * v),
    "coma_x": ("comaX", "u^3+uv^2-2u/3", lambda u, v: u ** 3 + u * v * v - 2.0 / 3.0 * u),
    "coma_y": ("comaY", "2v/3-v(u^2+v^2)", lambda u, v: 2.0 / 3.0 * v - v * (u * u + v * v)),
    "trefoil_x": ("trefoilX", "3uv^2-u^3", lambda u, v: 3 * u * v * v - u ** 3),
    "trefoil_y": ("trefoilY", "3u^2v-v^3", lambda u, v: 3 * u * u * v - v ** 3),
    "spherical": ("spherical", "(u^2+v^2)^2", lambda u, v: (u * u + v * v) ** 2),
}


def floats(text):
    return [float(v) for v in text.split(",")] if text else []


def find_psi_check():
    """psi_check.exe: из DIGITQT_PSI_CHECK, иначе самый свежий в ROOT/build*/.

    Папка сборки у всех своя (build, build-tests, build-release...), поэтому
    перебираются все. Не нашлось -- путь в build/, чтобы сообщение об ошибке
    подсказало, где его ждут.
    """
    env = os.environ.get("DIGITQT_PSI_CHECK")
    if env:
        return env
    found = []
    for name in os.listdir(ROOT):
        exe = os.path.join(ROOT, name, "tools", "psi_check", "psi_check.exe")
        if name.startswith("build") and os.path.isfile(exe):
            found.append(exe)
    if found:
        return max(found, key=os.path.getmtime)
    return os.path.join(ROOT, "build", "tools", "psi_check", "psi_check.exe")


def find_mingw_bin():
    """Папка с DLL MinGW (libstdc++, Qt не нужен): psi_check без них не стартует."""
    prefix = os.environ.get("MINGW_PREFIX")  # задан в оболочке MSYS2 MINGW64
    candidates = [os.path.join(prefix, "bin")] if prefix else []
    gxx = shutil.which("g++")
    if gxx:
        candidates.append(os.path.dirname(gxx))
    candidates.append(r"C:\msys64\mingw64\bin")  # установка MSYS2 по умолчанию
    return next((c for c in candidates if os.path.isdir(c)), None)


DEFAULTS = {
    "shifts": [0.0, 72.0, 198.0, 266.4, 36.0],  # настоящие сдвиги, град (рис. 4 статьи)
    "tilt_x": None,      # наклон сдвига по x, волн на кадр, по кадрам; None -- нули
    "tilt_y": None,
    "step": 90.0,        # номинальный шаг для DigitQt, град
    # Фронт, волн: коэффициенты при полиномах ABERRATIONS (базис Серегина,
    # как на S5 DigitQt -- он должен вернуть те же числа).
    "carrier_x": 0.5,    # несущая (наклон эталона) -- tiltX
    "carrier_y": 0.0,    # tiltY
    "defocus": 3.0,
    "astig_x": 0.0,
    "astig_y": 0.0,
    "coma_x": 0.0,
    "coma_y": 0.0,
    "trefoil_x": 0.0,
    "trefoil_y": 0.0,
    "spherical": 0.0,
    "noise": 0.0,        # шум камеры, уровни яркости
    "beam": False,       # гауссов пучок (0.5 на краю)
    "a": 110.0,          # фон, уровни
    "b": 90.0,           # амплитуда полос, уровни
    "size": 256,         # размер кадра, px
    "modes": ["nominal", "refine", "tilt"],
    "out": os.path.join(HERE, "demo"),
    "exe": find_psi_check(),
    "seed": 1,
}


def make_args(**params):
    """Параметры демонстрации: DEFAULTS, переопределённые params. Для тетрадки."""
    unknown = set(params) - set(DEFAULTS)
    if unknown:
        raise ValueError("неизвестные параметры: %s" % ", ".join(sorted(unknown)))
    args = argparse.Namespace(**{**DEFAULTS, **params})
    args.shifts = [float(v) for v in args.shifts]
    n = len(args.shifts)
    if n < 3:
        raise ValueError("нужно не меньше 3 кадров, задано %d" % n)
    args.tilt_x = [float(v) for v in args.tilt_x] if args.tilt_x else [0.0] * n
    args.tilt_y = [float(v) for v in args.tilt_y] if args.tilt_y else [0.0] * n
    if len(args.tilt_x) != n or len(args.tilt_y) != n:
        raise ValueError("tilt_x и tilt_y должны иметь по значению на кадр (%d)" % n)
    for m in args.modes:
        if m not in MODES:
            raise ValueError("неизвестный режим %s (есть: %s)" % (m, ", ".join(MODES)))
    return args


def parse_args():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    d = DEFAULTS
    p.add_argument("--shifts", default=",".join("%g" % v for v in d["shifts"]),
                   help="настоящие сдвиги кадров, град (по умолчанию рис. 4 статьи)")
    p.add_argument("--tilt-x", default="", help="наклон сдвига по x, волн на кадр, по кадрам")
    p.add_argument("--tilt-y", default="", help="наклон сдвига по y, волн на кадр, по кадрам")
    p.add_argument("--step", type=float, default=d["step"],
                   help="номинальный шаг для DigitQt, град")
    for name, (term, formula, _) in ABERRATIONS.items():
        p.add_argument("--" + name.replace("_", "-"), type=float, default=d[name],
                       help="%s = %s, волн (S5: %s)" % (name, formula, term))
    p.add_argument("--noise", type=float, default=d["noise"], help="шум камеры, уровни яркости")
    p.add_argument("--beam", action="store_true", help="гауссов пучок (0.5 на краю)")
    p.add_argument("--a", type=float, default=d["a"], help="фон, уровни")
    p.add_argument("--b", type=float, default=d["b"], help="амплитуда полос, уровни")
    p.add_argument("--size", type=int, default=d["size"], help="размер кадра, px")
    p.add_argument("--modes", default=",".join(d["modes"]),
                   help="режимы DigitQt через запятую: " + ", ".join(MODES))
    p.add_argument("--out", default=d["out"], help="папка для серии")
    p.add_argument("--exe", default=d["exe"])
    p.add_argument("--seed", type=int, default=d["seed"])
    p.add_argument("--no-plot", action="store_true")
    params = vars(p.parse_args())
    no_plot = params.pop("no_plot")
    params["shifts"] = floats(params["shifts"])
    params["tilt_x"] = floats(params["tilt_x"])
    params["tilt_y"] = floats(params["tilt_y"])
    params["modes"] = [m for m in params["modes"].split(",") if m]
    try:
        return make_args(**params), no_plot
    except ValueError as e:
        p.error(str(e))


# ---------------------------------------------------------------- генерация

def geometry(size):
    radius = 0.45 * size
    c = (size - 1) / 2.0
    y, x = np.mgrid[0:size, 0:size].astype(float)
    return x, y, c, radius


def true_wavefront(args):
    """Фронт в волнах; NaN вне апертуры."""
    x, y, c, radius = geometry(args.size)
    inside = (x - c) ** 2 + (y - c) ** 2 <= radius ** 2
    # Нормировка как у S5 (computeApertureGeometry()): край апертуры --
    # центры крайних пикселей, а не геометрический круг; иначе
    # коэффициенты S5 разойдутся с заложенными на доли процента x степень.
    xs = x[inside]
    pupil = (xs.max() - xs.min()) / 2.0
    u, v = (x - c) / pupil, (y - c) / pupil
    w = np.zeros_like(u)
    for name, (_, _, polynomial) in ABERRATIONS.items():
        w += getattr(args, name) * polynomial(u, v)
    w[~inside] = np.nan
    return w


def make_frames(args, w):
    x, y, c, radius = geometry(args.size)
    rng = np.random.default_rng(args.seed)
    gain = np.exp(-0.7 * ((x - c) ** 2 + (y - c) ** 2) / radius ** 2) if args.beam else 1.0
    frames = []
    for shift, tx, ty in zip(args.shifts, args.tilt_x, args.tilt_y):
        delta = (np.radians(shift)
                 + 2 * np.pi * (tx * (x - c) + ty * (y - c)) / (args.size - 1))
        value = np.where(np.isnan(w), args.a,
                         args.a + args.b * np.cos(2 * np.pi * np.nan_to_num(w) + delta))
        value = gain * value
        if args.noise:
            value = value + rng.normal(0.0, args.noise, value.shape)
        frames.append(np.clip(np.rint(value), 0, 255).astype(np.uint8))
    return frames


def write_gray_bmp(path, pixels):
    h, w = pixels.shape
    row = (w + 3) & ~3
    palette = b"".join(bytes((i, i, i, 0)) for i in range(256))
    offset = 14 + 40 + len(palette)
    with open(path, "wb") as f:
        f.write(b"BM" + struct.pack("<IHHI", offset + row * h, 0, 0, offset))
        f.write(struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, row * h, 2835, 2835, 256, 256))
        f.write(palette)
        pad = bytes(row - w)
        for line in pixels[::-1]:  # BMP -- снизу вверх
            f.write(line.tobytes() + pad)


def write_series(args, frames):
    if os.path.isdir(args.out):
        shutil.rmtree(args.out)
    os.makedirs(args.out)
    names = []
    for n, frame in enumerate(frames, start=1):
        names.append("frame%d.bmp" % n)
        write_gray_bmp(os.path.join(args.out, names[-1]), frame)
    lines = ["Interferograms descriptive file", "", str(len(names)), str(args.size),
             str(args.size)] + names
    idf = os.path.join(args.out, "series.idf")
    with open(idf, "wb") as f:
        f.write("\r\n".join(lines).encode("ascii") + b"\r\n")
    return idf


# ---------------------------------------------------------------- прогон

def run_digitqt(args, idf, mode):
    flags, _ = MODES[mode]
    map_path = os.path.join(args.out, "map_%s.txt" % mode)
    # Апертура DigitQt чуть шире заложенной: край данных задаёт сам
    # генератор (снаружи полос нет -- отсеет порог модуляции), и S5 нормирует
    # координаты ровно по нему -- коэффициенты сравнимы с заложенными.
    radius = 0.45 * args.size + 2.0
    cmd = [args.exe, idf, "--radius=%g" % radius, "--step=%g" % args.step,
           "--out-map=" + map_path] + flags
    env = dict(os.environ)
    mingw = find_mingw_bin()
    if mingw:
        env["PATH"] = mingw + os.pathsep + env.get("PATH", "")
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if proc.returncode != 0:
        return None, proc.stdout + proc.stderr
    report = {}
    for line in proc.stdout.splitlines():
        key, _, value = line.partition(":")
        report[key.strip()] = value.strip()
    with open(map_path) as f:
        f.readline()
        orders = np.loadtxt(f)
    return (report, orders), proc.stdout


def numbers(text):
    return [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?(?:e-?\d+)?", text)]


def wrap_deg(d):
    return (np.asarray(d) + 180.0) % 360.0 - 180.0


def compare(args, w, report, orders):
    true_shifts = np.array(args.shifts) - args.shifts[0]
    found = np.array(numbers(report["Shifts, deg"]))
    valid = ~np.isnan(orders) & ~np.isnan(w)
    result = {"points": int(valid.sum())}

    # Знак карты: (phi, delta) и (-phi, -delta) неразличимы по кадрам.
    errors = {}
    for sign in (1.0, -1.0):
        e = orders[valid] - sign * w[valid]
        errors[sign] = e - e.mean()
    sign = 1.0 if np.std(errors[1.0]) <= np.std(errors[-1.0]) else -1.0
    result["sign_flipped"] = sign < 0
    result["map_rms"] = float(np.std(errors[sign]))
    result["map_pv"] = float(np.ptp(errors[sign]))
    error_map = np.full(w.shape, np.nan)
    error_map[valid] = errors[sign]
    result["error_map"] = error_map

    reference = true_shifts if sign > 0 else -true_shifts
    result["shift_err"] = float(np.max(np.abs(wrap_deg(found - reference))))
    if "Tilt x, waves" in report:
        tx = np.array(numbers(report["Tilt x, waves"]))
        ty = np.array(numbers(report["Tilt y, waves"]))
        true_tx = sign * (np.array(args.tilt_x) - args.tilt_x[0])
        true_ty = sign * (np.array(args.tilt_y) - args.tilt_y[0])
        result["tilt_err"] = float(max(np.max(np.abs(tx - true_tx)),
                                       np.max(np.abs(ty - true_ty))))

    # Коэффициенты S5: параметр -> (заложено, найдено DigitQt). При
    # перевёрнутом знаке карты найденные сравниваются со знаком минус.
    found_s5 = dict(re.findall(r"(\w+)=(-?\d+\.\d+)", report.get("S5 coefficients", "")))
    result["s5"] = {name: (getattr(args, name), sign * float(found_s5[term]))
                    for name, (term, _, _) in ABERRATIONS.items() if term in found_s5}
    result["s5_err"] = max((abs(t - f) for t, f in result["s5"].values()), default=float("nan"))
    return result


# ---------------------------------------------------------------- вывод

def run_demo(args, log=print):
    """Генерация серии, прогон через DigitQt во всех args.modes, сверка.

    @return (w, rows): заложенный фронт и список (mode, report, cmp, orders).
    """
    if not os.path.isfile(args.exe):
        raise FileNotFoundError("Нет %s -- соберите с -DDIGITQT_BUILD_TOOLS=ON или укажите exe "
                                "(--exe или переменная DIGITQT_PSI_CHECK)" % args.exe)
    w = true_wavefront(args)
    idf = write_series(args, make_frames(args, w))
    log("Серия: %d кадров %dx%d -> %s" % (len(args.shifts), args.size, args.size, idf))
    log("Заложено: сдвиги %s град; наклон x %s, y %s волн на кадр"
        % (args.shifts, args.tilt_x, args.tilt_y))
    log("          фронт PV %.3f волн; шум %g; пучок %s; номинальный шаг %g"
        % (np.nanmax(w) - np.nanmin(w), args.noise, "да" if args.beam else "нет", args.step))
    log("")

    rows = []
    for mode in args.modes:
        run, text = run_digitqt(args, idf, mode)
        if run is None:
            log("[%s] DigitQt не справился:\n%s" % (mode, text))
            continue
        report, orders = run
        cmp = compare(args, w, report, orders)
        rows.append((mode, report, cmp, orders))

        log("== %s" % MODES[mode][1])
        log("   найденные сдвиги, град:  %s" % report["Shifts, deg"])
        if "Tilt x, waves" in report:
            log("   наклон x / y, волн:      %s  |  %s"
                % (report["Tilt x, waves"], report["Tilt y, waves"]))
        log("   невязка: %s;  половины: %s" % (report["Fit residual"],
                                               report["Halves disagree"]))
        log("   ошибка сдвигов: %.3f град%s" % (
            cmp["shift_err"],
            ";  ошибка наклона: %.4f волн" % cmp["tilt_err"] if "tilt_err" in cmp else ""))
        log("   ошибка карты: RMS %.5f волн, PV %.4f волн%s" % (
            cmp["map_rms"], cmp["map_pv"],
            "   !!! ЗНАК КАРТЫ ПЕРЕВЁРНУТ" if cmp["sign_flipped"] else ""))
        if cmp["s5"]:
            # Заложенные аберрации и те, что DigitQt нашёл заметными.
            shown = ["%s %.3f->%.3f" % (name, t, f) for name, (t, f) in cmp["s5"].items()
                     if t != 0.0 or abs(f) >= 0.005]
            log("   S5, волн (заложено->найдено): %s" % ", ".join(shown))
            log("   ошибка S5: %.4f волн (наибольшая по всем 10 термам)" % cmp["s5_err"])
        log("")
    return w, rows


def plot(args, w, rows):
    """Правда и кадр 1 слева, по столбцу на режим: карта DigitQt и её ошибка."""
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, len(rows) + 1, figsize=(3.6 * (len(rows) + 1), 7))
    axes = np.atleast_2d(axes)
    h = axes[0, 0].imshow(w, cmap="viridis")
    axes[0, 0].set_title("заложенный фронт, волн")
    fig.colorbar(h, ax=axes[0, 0], fraction=0.046)
    axes[1, 0].imshow(make_frames(args, w)[0], cmap="gray")
    axes[1, 0].set_title("кадр 1")
    for i, (mode, report, cmp, orders) in enumerate(rows, start=1):
        h = axes[0, i].imshow(orders, cmap="viridis")
        axes[0, i].set_title("DigitQt: %s" % mode)
        fig.colorbar(h, ax=axes[0, i], fraction=0.046)
        h = axes[1, i].imshow(cmp["error_map"], cmap="RdBu_r")
        axes[1, i].set_title("ошибка: RMS %.4f%s" % (cmp["map_rms"],
                                                    ", ЗНАК!" if cmp["sign_flipped"] else ""))
        fig.colorbar(h, ax=axes[1, i], fraction=0.046)
    for ax in axes.flat:
        ax.axis("off")
    fig.tight_layout()
    return fig


def main():
    args, no_plot = parse_args()
    try:
        w, rows = run_demo(args)
    except FileNotFoundError as e:
        sys.exit(str(e))
    if not rows or no_plot:
        return
    import matplotlib
    matplotlib.use("Agg")
    fig = plot(args, w, rows)
    png = os.path.join(args.out, "demo.png")
    fig.savefig(png, dpi=90)
    print("Картинка: %s" % png)


if __name__ == "__main__":
    main()
