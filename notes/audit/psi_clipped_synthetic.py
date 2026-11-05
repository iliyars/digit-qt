"""Known-phase control experiment: camera clipping without motion or tilt.

Runs the actual psi_check on unclipped and deliberately upper-clipped data.
All frames, maps and logs are generated under --output; no real frames change.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/psi-clipped-synthetic"))
    parser.add_argument("--exe", type=Path, default=Path("build/tools/psi_check/psi_check.exe"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    size, radius, count = 128, 60, 6
    y, x = np.indices((size, size))
    u, v = (x-63.5)/radius, (y-63.5)/radius
    mask = u*u+v*v <= 1
    phase = 2*np.pi*(3*u+v+.7*(u*u+v*v))
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;"+env.get("PATH", "")
    results = []
    for name, background, amplitude, ceiling in [
        ("clean", 120, 100, 255),
        ("clip_235", 170, 100, 235),
        ("clip_255", 190, 100, 255),
    ]:
        folder = args.output/name
        folder.mkdir(exist_ok=True)
        frames = []
        for n in range(count):
            raw = background+amplitude*np.cos(phase+n*np.pi/3)
            frame = np.rint(np.clip(raw, 0, ceiling)).astype(np.uint8)
            frames.append(frame)
            Image.fromarray(frame).save(folder/f"frame{n}.bmp")
        idf = folder/"series.idf"
        idf.write_text("Interferograms descriptive file\n\n6\n128\n128\n"+
                       "\n".join(f"frame{n}.bmp" for n in range(count))+"\n", encoding="ascii")
        for mode, flags in [("known", ["--no-refine"]), ("refine", [])]:
            map_path = folder/(mode+".map")
            process = subprocess.run([str(args.exe.resolve()), str(idf.resolve()),
                "--step=60", "--radius=60", "--out-map="+str(map_path.resolve()), *flags],
                env=env, text=True, capture_output=True, timeout=120)
            (folder/(mode+".txt")).write_text(process.stdout+process.stderr, encoding="utf-8")
            record = dict(case=name, mode=mode, returncode=process.returncode,
                          output=process.stdout, error=process.stderr)
            if process.returncode == 0:
                orders = np.loadtxt(map_path, skiprows=1)
                valid = mask & np.isfinite(orders)
                error = orders[valid]-phase[valid]/(2*np.pi)
                error -= error.mean()  # Only the unwrapping integer/piston gauge.
                record.update(coverage=float(valid.sum()/mask.sum()),
                    phase_rms_orders=float(np.sqrt(np.mean(error**2))),
                    fraction_samples_at_ceiling=float((np.stack(frames)[:, mask]==ceiling).mean()))
            results.append(record)
            print(json.dumps(record), flush=True)
    (args.output/"summary.json").write_text(json.dumps(results, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
