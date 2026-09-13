#!/usr/bin/env python3
"""M9: compare blend weights on full frames of the validation paths.

Patch validation inside the trainer is biased towards hard pixels and says
little about a whole frame (+0.25 dB on patches was +0.08 dB on the measured
view). Choices between models -- loss, seed, data -- are made here instead, on
full frames of paths that are neither in the training set nor among the
measurement views of scripts/run_metrics.py, so that those views are scored
once, by the model this picked.

    scripts/ml_select.py WEIGHTS.bin [...]    (the heuristic is always included)

Results are cached in captures/ml/select.csv by (weights, view).
"""

import argparse
import csv
import pathlib
import re
import statistics
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ml_dataset as ds  # noqa: E402

CACHE = ds.REPO / "captures" / "ml" / "select.csv"

# The validation captures, plus two 20 fps paths: low frame rates are where
# the first models lost, and the captured validation set has none.
VIEWS = [(name, scale, fps, scene) for split, name, scale, fps, scene in ds.RUNS if split == "val"]
VIEWS += [
    ("val-sponza-r2-p1.0-20", 1.5, 20, ds.sponza(2, 1.0)),
    ("val-proc-t100-p5.0-20", 1.5, 20, ds.procedural(100, 5.0)),
]


def measure(weights, scale, fps, scene):
    cmd = [str(ds.BIN), *ds.arguments(scale, fps, scene)]
    if weights != "heuristic":
        cmd += ["--fg-ml", weights]
    out = subprocess.run(cmd, capture_output=True, text=True, cwd=ds.REPO)
    if out.returncode != 0:
        sys.exit(out.stderr[-2000:])
    psnr = re.search(r"interpolated\s+PSNR\s+([0-9.]+) dB\s+SSIM\s+([0-9.]+)\s+worst frame\s+([0-9.]+)",
                     out.stdout)
    if not psnr:
        sys.exit(f"no result for {weights}:\n{out.stdout[-2000:]}")
    return psnr.groups()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("weights", nargs="*")
    args = parser.parse_args()

    cache = {}
    if CACHE.exists():
        with CACHE.open() as f:
            cache = {(r["weights"], r["view"]): r for r in csv.DictReader(f)}

    names = ["heuristic", *[str(pathlib.Path(w)) for w in args.weights]]
    for weights in names:
        for view, scale, fps, scene in VIEWS:
            if (weights, view) in cache:
                continue
            psnr, ssim, worst = measure(weights, scale, fps, scene)
            cache[(weights, view)] = {"weights": weights, "view": view, "psnr": psnr,
                                      "ssim": ssim, "worst": worst}
            with CACHE.open("w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=["weights", "view", "psnr", "ssim", "worst"])
                writer.writeheader()
                writer.writerows(cache.values())

    views = [v[0] for v in VIEWS]
    print(f"{'weights':44s}" + "".join(f"{v[4:22]:>20s}" for v in views) + f"{'mean gain':>11s}")
    base = {v: float(cache[("heuristic", v)]["psnr"]) for v in views}
    for weights in names:
        cells, gains = [], []
        for v in views:
            r = cache[(weights, v)]
            gain = float(r["psnr"]) - base[v]
            gains.append(gain)
            cells.append(f"{float(r['psnr']):7.2f} {gain:+5.2f} {float(r['worst']):5.1f}")
        label = weights if len(weights) <= 44 else "..." + weights[-41:]
        print(f"{label:44s}" + "".join(f"{c:>20s}" for c in cells) + f"{statistics.fmean(gains):+11.3f}")
    print("cells: PSNR dB, gain over the heuristic, worst frame dB")


if __name__ == "__main__":
    main()
