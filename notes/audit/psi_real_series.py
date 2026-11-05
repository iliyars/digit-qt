"""Inspect an IDF series and compare the actual DigitQt PSI modes/apertures.

Requires numpy, scipy, Pillow, matplotlib and a built psi_check. All generated
files go to --output. Original frames/IDF are read only. No ground-truth accuracy
is inferred from residuals on real images.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
from scipy import ndimage, optimize


def disk_fit(image, foreground_level=30):
    # Appropriate only for this dataset's bright circular pupil on dark backing.
    foreground = image > foreground_level
    labels, _ = ndimage.label(foreground)
    counts = np.bincount(labels.ravel())
    counts[0] = 0
    pupil = ndimage.binary_fill_holes(labels == counts.argmax())
    edge = pupil & ~ndimage.binary_erosion(pupil)
    y, x = np.nonzero(edge)
    start = [x.mean(), y.mean(), np.sqrt(pupil.sum()/np.pi)]
    fit = optimize.least_squares(
        lambda p: np.hypot(x-p[0], y-p[1])-p[2], start, loss="soft_l1", f_scale=1)
    return fit.x, pupil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("series", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/psi-real"))
    parser.add_argument("--exe", type=Path, default=Path("build/tools/psi_check/psi_check.exe"))
    parser.add_argument("--inspect-only", action="store_true")
    parser.add_argument("--foreground-level", type=float, default=30)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    lines = [s.strip() for s in args.series.read_text(encoding="ascii").splitlines() if s.strip()]
    if lines[0] != "Interferograms descriptive file":
        raise ValueError("Unexpected IDF header; original data must be checked before processing")
    count, width, height = map(int, lines[1:4])
    names = lines[4:4+count]
    frames = np.stack([np.asarray(Image.open(args.series.parent/name).convert("L"), dtype=float)
                       for name in names])
    if frames.shape != (count, height, width):
        raise ValueError("Declared and actual frame sizes disagree")
    geometry, _ = disk_fit(frames.max(axis=0), args.foreground_level)
    cx, cy, radius = map(float, geometry)
    yy, xx = np.indices((height, width))
    masks = {
        "default": (width/2-.5, height/2-.5, min(width, height)/2-1),
        "pupil": (cx, cy, radius-3),
        "inner": (cx, cy, (radius-3)*.70),
    }
    span = frames.max(axis=0)-frames.min(axis=0)
    summary = dict(series=str(args.series), size=[width, height], frames=names,
                   fitted_pupil=[cx, cy, radius], apertures=masks, image_metrics=[])
    for name, (x, y, r) in masks.items():
        mask = (xx-x)**2+(yy-y)**2 <= r*r
        temporal = frames[:, mask]
        temporal -= temporal.mean(axis=0, keepdims=True)
        singular = np.linalg.svd(temporal, compute_uv=False)
        energy = singular**2
        histogram = np.bincount(frames[:, mask].astype(np.uint8).ravel(), minlength=256)
        upper_mode = 192 + int(histogram[192:].argmax())
        summary["image_metrics"].append(dict(
            aperture=name, pixels=int(mask.sum()),
            dark_fraction=float((frames[:, mask] <= 5).mean()),
            saturated_fraction=float((frames[:, mask] >= 250).mean()),
            upper_histogram_mode=upper_mode,
            upper_mode_fraction=float(histogram[upper_mode]/(mask.sum()*count)),
            upper_mode_band_fraction=float((np.abs(frames[:, mask]-upper_mode) <= 2).mean()),
            span_percentiles=np.percentile(span[mask], [10, 50, 90, 99]).tolist(),
            temporal_rank2_unexplained=float(energy[2:].sum()/energy.sum()),
            temporal_singular_values=singular.tolist()))
    summary["individual_pupil_fits"] = [disk_fit(frame, args.foreground_level)[0].tolist() for frame in frames]
    summary["foreground_level"] = args.foreground_level
    fig, axes = plt.subplots(2, max(count, 3), figsize=(3*max(count, 3), 6), squeeze=False)
    for ax in axes.ravel():
        ax.set_axis_off()
    for i, (frame, name) in enumerate(zip(frames, names)):
        axes[0, i].imshow(frame, cmap="gray", vmin=0, vmax=255)
        axes[0, i].set_title(name)
    axes[1, 0].imshow(frames[0], cmap="gray", vmin=0, vmax=255)
    colors = {"default": "red", "pupil": "lime", "inner": "cyan"}
    for name, (x, y, r) in masks.items():
        axes[1, 0].add_patch(plt.Circle((x, y), r, fill=False, color=colors[name], label=name))
    axes[1, 0].legend(fontsize=8)
    axes[1, 0].set_title("Apertures in source pixels")
    im = axes[1, 1].imshow(span, vmin=0, vmax=255, cmap="magma")
    axes[1, 1].set_title("Temporal intensity span")
    fig.colorbar(im, ax=axes[1, 1], shrink=.65)
    axes[1, 2].set_axis_on()
    for i, frame in enumerate(frames):
        axes[1, 2].plot(frame[round(cy)], label=str(i+1))
    axes[1, 2].set_title("Intensity through pupil center")
    axes[1, 2].set_xlabel("x, px")
    axes[1, 2].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(args.output/"frames-and-apertures.png", dpi=140)
    plt.close(fig)
    print(json.dumps(summary, indent=2), flush=True)
    runs = []
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;" + env.get("PATH", "")
    if not args.inspect_only:
        for aperture, (x, y, r) in masks.items():
            for mode, flags in [("nominal", ["--no-refine"]), ("refine", []), ("tilt", ["--tilt"])]:
                stem = args.output/(aperture+"-"+mode)
                command = [str(args.exe.resolve()), str(args.series.resolve()),
                           f"--center-x={x}", f"--center-y={y}", f"--radius={r}",
                           "--out-map="+str(stem.with_suffix(".map").resolve()), *flags]
                process = subprocess.run(command, capture_output=True, text=True, env=env, timeout=180)
                stem.with_suffix(".txt").write_text(process.stdout+process.stderr, encoding="utf-8")
                record = dict(aperture=aperture, mode=mode, returncode=process.returncode,
                              output=process.stdout, error=process.stderr)
                if process.returncode == 0:
                    orders = np.loadtxt(stem.with_suffix(".map"), skiprows=1)
                    record["valid_pixels"] = int(np.isfinite(orders).sum())
                    record["aperture_coverage"] = record["valid_pixels"] / int(
                        (((xx-x)**2+(yy-y)**2) <= r*r).sum())
                runs.append(record)
                print(aperture, mode, json.dumps(record), flush=True)
    summary["runs"] = runs
    (args.output/"summary.json").write_text(json.dumps(summary, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
