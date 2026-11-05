"""Diagnostic comparison of linear and upper-clipped PSI intensity models.

The upper level is an explicit hypothesis, not automatically calibrated camera
metadata. This script does not correct source frames or attest phase accuracy.
"""
import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
from scipy.optimize import least_squares

from psi_real_series import disk_fit


def design(shifts):
    return np.column_stack([np.ones(len(shifts)), np.cos(shifts), -np.sin(shifts)])


def estimate(frames, mask, start):
    data = frames[:, mask]
    # Dense residuals only for this small five-frame diagnostic dataset.
    def residual(free):
        matrix = design(np.r_[0, free])
        coefficients = np.linalg.lstsq(matrix, data, rcond=None)[0]
        return (data-matrix@coefficients).ravel()
    result = least_squares(residual, start[1:], max_nfev=100,
                           xtol=1e-9, ftol=1e-9, gtol=1e-7)
    shifts = np.r_[0, result.x] % (2*np.pi)
    matrix = design(shifts)
    beta = np.linalg.lstsq(matrix, data, rcond=None)[0]
    inverse = np.linalg.pinv(matrix)
    projection = np.eye(len(shifts))-matrix@inverse
    derivative = -np.sin(shifts)[:, None]*beta[1]-np.cos(shifts)[:, None]*beta[2]
    information = (projection*(derivative@derivative.T))[1:, 1:]
    singular = np.linalg.svd(information, compute_uv=False)
    return shifts, dict(
        pixels=int(mask.sum()), converged=bool(result.success),
        intensity_rms=float(np.sqrt(np.mean(result.fun**2))),
        phase_design_condition=float(np.linalg.cond(matrix)),
        shift_information_ratio=float(singular[-1]/singular[0]),
        median_fringe_amplitude=float(np.median(np.hypot(beta[1], beta[2]))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("series", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/psi-clipping"))
    parser.add_argument("--upper-level", type=float, default=235)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    lines = [s.strip() for s in args.series.read_text().splitlines() if s.strip()]
    if lines[0] != "Interferograms descriptive file":
        raise ValueError("Unexpected IDF header")
    n = int(lines[1])
    frames = np.stack([np.array(Image.open(args.series.parent/name), dtype=float)
                       for name in lines[4:4+n]])
    cx, cy, radius = disk_fit(frames.max(axis=0))[0]
    yy, xx = np.indices(frames.shape[1:])
    pupil = (xx-cx)**2+(yy-cy)**2 <= (radius-3)**2
    span = frames.max(axis=0)-frames.min(axis=0)
    usable = pupil & (span >= .3*np.percentile(span[pupil], 99))
    unclipped = usable & (frames.max(axis=0) < args.upper_level-2)
    saturated = frames >= args.upper_level-2
    start = np.deg2rad([0, 90, 165, 255, 10])
    if n != len(start):
        raise ValueError("This diagnostic currently requires a five-frame series")
    summary = dict(upper_level_hypothesis=args.upper_level,
                   fraction_pupil_with_upper_samples=float(saturated[:, pupil].any(axis=0).mean()),
                   fits={})
    for name, mask in [("all", usable), ("below_upper", unclipped),
                       ("below_upper_left", unclipped & (xx < cx)),
                       ("below_upper_right", unclipped & (xx >= cx))]:
        shifts, metrics = estimate(frames, mask, start)
        metrics["shifts_deg"] = np.rad2deg(shifts).tolist()
        summary["fits"][name] = metrics
        print(name, json.dumps(metrics), flush=True)
    # Keep the full-pupil shifts for a like-for-like intensity-model comparison.
    # A fit after excluding upper samples may be unidentifiable, so do not use
    # it as a ground truth or force it to a plausible-looking phase sequence.
    shifts = np.deg2rad(summary["fits"]["all"]["shifts_deg"])
    matrix = design(shifts)
    # Estimate photometry from only unbounded samples. Saturated samples are
    # then held out and checked against the hypothesized clipping bound.
    linear_rms = np.full(pupil.shape, np.nan)
    clipped_rms = np.full(pupil.shape, np.nan)
    tested, obey_bound = 0, 0
    for bits in range(1, 2**n-1):
        high = np.array([(bits >> i) & 1 for i in range(n)], dtype=bool)
        good = ~high
        if good.sum() < 3 or np.linalg.cond(matrix[good]) > 100:
            continue
        mask = usable & np.all(saturated == high[:, None, None], axis=0)
        if not mask.any():
            continue
        data = frames[:, mask]
        beta = np.linalg.lstsq(matrix[good], data[good], rcond=None)[0]
        raw_prediction = matrix@beta
        prediction = np.minimum(raw_prediction, args.upper_level)
        clipped_rms[mask] = np.sqrt(np.mean((data-prediction)**2, axis=0))
        linear = matrix@np.linalg.lstsq(matrix, data, rcond=None)[0]
        linear_rms[mask] = np.sqrt(np.mean((data-linear)**2, axis=0))
        tested += data.shape[1]
        obey_bound += int(np.all(raw_prediction[high] >= args.upper_level-2, axis=0).sum())
    valid = np.isfinite(clipped_rms)
    summary["held_out_bright_samples"] = dict(
        pixels=tested, fraction_predictions_obey_upper_bound=obey_bound/tested if tested else None,
        linear_intensity_rms=float(np.sqrt(np.mean(linear_rms[valid]**2))) if tested else None,
        unsaturated_fit_clamped_prediction_rms=float(np.sqrt(np.mean(clipped_rms[valid]**2))) if tested else None)
    fig, axes = plt.subplots(2, 3, figsize=(12, 7))
    for ax, data, title in zip(axes[0], [frames[0], usable, unclipped],
                               ["First frame", "Pupil + useful temporal span", "No sample near upper level"]):
        ax.imshow(data, cmap="gray")
        ax.set_title(title)
        ax.set_axis_off()
    for ax, data, title in zip(axes[1, :2], [linear_rms, clipped_rms],
                               ["Linear model RMS, same pixels", "Unsaturated fit + upper clamp RMS"]):
        im = ax.imshow(data, vmin=0, vmax=10, cmap="magma")
        ax.set_title(title)
        ax.set_axis_off()
        fig.colorbar(im, ax=ax, shrink=.7, label="Intensity levels")
    axes[1, 2].hist(frames[:, pupil].ravel(), bins=np.arange(190, 241), color="gray")
    axes[1, 2].axvline(args.upper_level, color="red", label="Hypothesized upper bound")
    axes[1, 2].set_title("Bright-end histogram")
    axes[1, 2].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(args.output/"clipping-comparison.png", dpi=140)
    plt.close(fig)
    (args.output/"summary.json").write_text(json.dumps(summary, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(summary["held_out_bright_samples"]), flush=True)


if __name__ == "__main__":
    main()
