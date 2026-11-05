"""Independent black-box phase-plugin audit; Python stdlib only, Windows x64.

Run from the repository root after building:
  python notes/audit/phase_plugin_probe.py --output build/phase-plugin-probe.json
Errors compare against the generated phase with ONLY a piston removed.
The fitted slope is diagnostic; it is never used to reduce the reported RMS.
"""
import argparse
import ctypes as C
import json
import math
import os
from pathlib import Path
import statistics


class Bitmap(C.Structure):
    _fields_ = [("width", C.c_int32), ("height", C.c_int32),
                ("pixels", C.POINTER(C.c_uint8))]


class Info(C.Structure):
    _fields_ = [("name", C.c_char_p), ("version", C.c_char_p), ("needs_lines", C.c_int32)]


Visible = C.CFUNCTYPE(C.c_int, C.c_int32, C.c_int32, C.c_void_p)
Create = C.CFUNCTYPE(C.c_void_p)
Destroy = C.CFUNCTYPE(None, C.c_void_p)
Reconstruct = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int32, C.c_int32,
                          C.POINTER(Bitmap), Visible, C.c_void_p, C.c_void_p,
                          C.c_size_t, C.POINTER(C.c_double))
String = C.CFUNCTYPE(C.c_char_p, C.c_void_p)


class VTable(C.Structure):
    _fields_ = [("create", Create), ("destroy", Destroy), ("reconstruct", Reconstruct),
                ("name", String), ("error", String)]


def run(dll_path, case, fx=18, fy=0, size=128):
    dll = C.CDLL(str(dll_path.resolve()))
    entry = dll.dqt_phase_plugin_entry
    entry.argtypes = [C.c_uint32, C.POINTER(Info), C.POINTER(C.POINTER(VTable))]
    entry.restype = C.c_int
    info, table = Info(), C.POINTER(VTable)()
    if not entry(2, C.byref(info), C.byref(table)):
        raise RuntimeError("ABI v2 unavailable")
    api = table.contents
    center = (size - 1) / 2

    def mask(x, y):
        r2 = (x - center) ** 2 + (y - center) ** 2
        if case == "disconnected":
            return ((x - size * .25) ** 2 + (y - center) ** 2 < (size * .17) ** 2 or
                    (x - size * .75) ** 2 + (y - center) ** 2 < (size * .17) ** 2)
        return r2 < (size * .43) ** 2 and (case != "annulus" or r2 > (size * .15) ** 2)

    truth, pixels = [], []
    for y in range(size):
        for x in range(size):
            u, v = (x - center) / size, (y - center) / size
            phase = fx * u + fy * v + .6 * (u * u + v * v)
            truth.append(phase)
            pixels.append(128 if case == "flat" else round(128 + 90 * math.cos(2 * math.pi * phase)))
    data = (C.c_uint8 * len(pixels))(*pixels)
    image = Bitmap(size, size, data)
    output = (C.c_double * len(pixels))()
    visible = Visible(lambda x, y, _: int(mask(x, y)))
    handle = api.create()
    if not handle:
        raise RuntimeError("Plugin allocation failed")
    try:
        success = bool(api.reconstruct(handle, size, size, C.byref(image), visible,
                                       None, None, 0, output))
        error = (api.error(handle) or b"").decode("utf-8")
    finally:
        api.destroy(handle)
    expected = [i for i in range(len(pixels)) if mask(i % size, i // size)]
    valid = [i for i in expected if math.isfinite(output[i])]
    central = [i for i in valid if (i % size - center) ** 2 + (i // size - center) ** 2 < (size * .30) ** 2]
    result = dict(plugin=dll_path.stem, case=case, size=size, fx=fx, fy=fy,
                  success=success, error=error, coverage=len(valid) / len(expected))
    if central and success:
        a, b = [truth[i] for i in central], [output[i] for i in central]
        result["central_pv_orders"] = max(b) - min(b)
        if case != "flat":  # A constant image carries no ground-truth phase.
            errors = [v - t for t, v in zip(a, b)]
            piston = statistics.mean(errors)
            result["rms_orders_piston_only"] = math.sqrt(statistics.mean((e - piston) ** 2 for e in errors))
            ma, mb = statistics.mean(a), statistics.mean(b)
            denom = sum((t - ma) ** 2 for t in a)
            result["slope_diagnostic"] = sum((t - ma) * (v - mb) for t, v in zip(a, b)) / denom
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--runtime", type=Path, default=Path("C:/msys64/mingw64/bin"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    # Keep directory handles alive throughout all DLL calls.
    handles = [os.add_dll_directory(str(args.runtime.resolve())),
               os.add_dll_directory(str((args.build / "src").resolve()))]
    results = []
    for plugin in ("fourier", "wavelet"):
        for case, fx, fy, size in [
            ("vertical", 18, 0, 128), ("horizontal", 0, 18, 128),
            ("diagonal", 12, 12, 128), ("negative_carrier", -18, 0, 128),
            ("odd", 18, 0, 129), ("annulus", 18, 0, 128),
            ("flat", 18, 0, 128), ("disconnected", 18, 0, 128),
        ]:
            result = run(args.build / "src/plugins/phase" / (plugin + ".dll"), case, fx, fy, size)
            results.append(result)
            print(json.dumps(result, ensure_ascii=False), flush=True)
    if args.output:
        args.output.write_text(json.dumps(results, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for handle in handles:
        handle.close()


if __name__ == "__main__":
    main()
