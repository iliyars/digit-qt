"""Known-truth PSI sweep over temporal spacing, amplitude and camera noise.

Runs the actual CLI with known shifts. Error removes only one phase piston;
coverage is recorded separately. This is not an instrument accuracy calibration.
"""
import argparse
import json
import os
import re
from pathlib import Path
import subprocess

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/psi-conditioning"))
    parser.add_argument("--exe", type=Path, default=Path("build/tools/psi_check/psi_check.exe"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;"+env.get("PATH", "")
    size, radius, count = 64, 29, 5
    y, x = np.indices((size, size))
    u, v = (x-31.5)/radius, (y-31.5)/radius
    pupil = u*u+v*v <= 1
    truth = 3*u+v+.7*(u*u+v*v)
    steps, noises, amplitudes = [.5, 5, 15, 45, 90], [0, 1, 3], [80, 8]
    results = []
    for amplitude in amplitudes:
        for noise in noises:
            for step in steps:
                folder = args.output/f"amplitude-{amplitude}-noise-{noise}-step-{step}"
                folder.mkdir(exist_ok=True)
                shifts = np.deg2rad(step*np.arange(count))
                matrix = np.column_stack([np.ones(count), np.cos(shifts), -np.sin(shifts)])
                rng = np.random.default_rng(721)
                for n, shift in enumerate(shifts):
                    intensity = 120+amplitude*np.cos(2*np.pi*truth+shift)
                    intensity += rng.normal(0, noise, intensity.shape)
                    frame = np.rint(np.clip(intensity, 0, 255)).astype(np.uint8)
                    Image.fromarray(frame).save(folder/f"frame{n}.bmp")
                idf = folder/"series.idf"
                idf.write_text("Interferograms descriptive file\n\n5\n64\n64\n"+
                    "\n".join(f"frame{n}.bmp" for n in range(count))+"\n", encoding="ascii")
                map_path = folder/"phase.map"
                process = subprocess.run([str(args.exe.resolve()), str(idf.resolve()),
                    "--no-refine", f"--step={step}", f"--radius={radius}",
                    "--out-map="+str(map_path.resolve())], env=env,
                    text=True, capture_output=True, timeout=120)
                (folder/"run.txt").write_text(process.stdout+process.stderr, encoding="utf-8")
                record = dict(step_deg=step, amplitude=amplitude, noise_sigma=noise,
                              design_condition_2=float(np.linalg.cond(matrix)),
                              returncode=process.returncode, error=process.stderr)
                for key, pattern in [
                    ("conditional_noise_p95", r"Conditional noise:\s+p95 ([\d.eE+-]+)"),
                    ("noisy_point_fraction", r"Noisy points:\s+([\d.eE+-]+)%")]:
                    match = re.search(pattern, process.stdout)
                    if match:
                        record[key] = float(match.group(1)) / (100 if "fraction" in key else 1)
                if process.returncode == 0:
                    orders = np.loadtxt(map_path, skiprows=1)
                    valid = pupil & np.isfinite(orders)
                    error = orders[valid]-truth[valid]
                    error -= error.mean()
                    record.update(coverage=float(valid.sum()/pupil.sum()),
                                  phase_rms_orders=float(np.sqrt(np.mean(error**2))))
                results.append(record)
                print(json.dumps(record), flush=True)
    (args.output/"summary.json").write_text(json.dumps(results, indent=2)+"\n", encoding="utf-8")
    fig, axes = plt.subplots(1, 3, figsize=(13, 4))
    for ax, amplitude in zip(axes[:2], amplitudes):
        for noise in noises:
            rows = [r for r in results if r["amplitude"] == amplitude and r["noise_sigma"] == noise]
            ax.loglog([r["step_deg"] for r in rows],
                      [r.get("phase_rms_orders", np.nan) for r in rows], "o-", label=f"noise sigma={noise}")
        ax.set_title(f"Fringe amplitude {amplitude} / 255")
        ax.set_xlabel("Known temporal step, degrees")
        ax.set_ylabel("Unwrapped phase error RMS, cycles")
        ax.grid(True, which="both", alpha=.25)
        ax.legend(fontsize=8)
    rows = results[:len(steps)]
    axes[2].loglog(steps, [r["design_condition_2"] for r in rows], "o-")
    axes[2].axhline(1e4, color="red", linestyle="--", label="Numerical rejection limit")
    axes[2].set_title("Condition of the temporal design")
    axes[2].set_xlabel("Known temporal step, degrees")
    axes[2].set_ylabel("Condition number (2-norm)")
    axes[2].grid(True, which="both", alpha=.25)
    axes[2].legend(fontsize=8)
    fig.suptitle("5 frames, 8-bit quantization; no fitted sign, scale or tilt")
    fig.tight_layout()
    fig.savefig(args.output/"conditioning-sweep.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    main()
