#!/usr/bin/env python3
"""M9: capture the learned blend's training and validation sets.

Each run is a lockstep --validate-fg capture with --ml-dump: every measured
frame contributes patches of the network's features and candidates together
with the real midpoint frame as the label (see src/ml/blend_net.h).

The split is by camera path, not by frame. The measurement views of
scripts/run_metrics.py -- Sponza at radius 3 from phase -0.6, and the
procedural scene from t = 0 at phase 0 -- are never captured: the orbit angle
is phase + 0.35 t, and every training run below stays out of the measured
angle range and its surroundings. Consecutive frames of one path are nearly
the same image, so a random frame split would score the network on frames it
has effectively seen.

Files go to captures/ml/data/ (git-ignored; about 200 MB per run).

    scripts/ml_dataset.py [--split train|val] [--only NAME ...] [--force]
"""

import argparse
import csv
import pathlib
import re
import subprocess
import sys
import time

REPO = pathlib.Path(__file__).resolve().parents[1]
BIN = REPO / "build" / "fsr3lite"
OUT = REPO / "captures" / "ml" / "data"
SPONZA = REPO / "assets/sponza/Sponza.gltf"

COMMON = ["--width", "1920", "--height", "1080", "--upscaler", "fsr", "--scripted", "--jitter",
          "--validate-fg", "--warmup", "16", "--ml-patches", "4"]
DT = {120: "0.0083333", 60: "0.0166667", 30: "0.0333333", 20: "0.05"}
MEASURED = {120: 90, 60: 60, 30: 40, 20: 30}


def sponza(radius, phase):
    return ["--scene", str(SPONZA), "--scene-fit", "12",
            "--path-radius", str(radius), "--path-phase", str(phase)]


def procedural(time_offset, phase):
    return ["--time-offset", str(time_offset), "--path-phase", str(phase)]


# (split, name, render scale, fps, scene arguments)
RUNS = [
    ("train", "sponza-r3-p2.5-60", 1.5, 60, sponza(3, 2.5)),
    ("train", "sponza-r3-p3.0-30", 1.5, 30, sponza(3, 3.0)),
    ("train", "sponza-r3-p3.5-120", 1.5, 120, sponza(3, 3.5)),
    ("train", "sponza-r5-p2.5-60", 1.5, 60, sponza(5, 2.5)),
    ("train", "sponza-r5-p3.5-20", 1.5, 20, sponza(5, 3.5)),
    ("train", "sponza-r5-p1.0-60", 1.5, 60, sponza(5, 1.0)),
    ("train", "sponza-r4-p2.0-30", 1.5, 30, sponza(4, 2.0)),
    ("train", "sponza-r2-p3.0-60", 1.5, 60, sponza(2, 3.0)),
    ("train", "sponza-r3-p2.5-60-perf", 2.0, 60, sponza(3, 2.5)),
    ("train", "sponza-r5-p2.0-60-native", 1.0, 60, sponza(5, 2.0)),
    ("train", "proc-t10-p1.0-60", 1.5, 60, procedural(10, 1.0)),
    ("train", "proc-t25-p3.0-30", 1.5, 30, procedural(25, 3.0)),
    ("train", "proc-t40-p3.0-60", 1.5, 60, procedural(40, 3.0)),
    ("train", "proc-t45-p2.0-20", 1.5, 20, procedural(45, 2.0)),
    # Added after the first full-frame measurement: the model lost at 20 and
    # 30 fps, where the first set had two paths out of fourteen.
    ("train", "sponza-r3-p2.5-20", 1.5, 20, sponza(3, 2.5)),
    ("train", "sponza-r4-p3.0-20", 1.5, 20, sponza(4, 3.0)),
    ("train", "sponza-r2-p2.0-30", 1.5, 30, sponza(2, 2.0)),
    ("train", "proc-t15-p3.0-20", 1.5, 20, procedural(15, 3.0)),
    ("val", "val-sponza-r4-p1.3-30", 1.5, 30, sponza(4, 1.3)),
    ("val", "val-proc-t80-p4.0-60", 1.5, 60, procedural(80, 4.0)),
]


def arguments(scale, fps, scene):
    return [*COMMON, "--scale", str(scale), "--fixed-dt", DT[fps],
            "--frames", str(16 + MEASURED[fps]), *scene]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--split", choices=["train", "val"])
    parser.add_argument("--only", nargs="*")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    manifest = OUT / "manifest.csv"
    rows = {}
    if manifest.exists():
        with manifest.open() as f:
            rows = {r["name"]: r for r in csv.DictReader(f)}

    for split, name, scale, fps, scene in RUNS:
        if args.split and split != args.split:
            continue
        if args.only and name not in args.only:
            continue
        path = OUT / f"{name}.bin"
        if path.exists() and not args.force:
            print(f"{name}: exists, skipped")
            continue
        cmd = [str(BIN), *arguments(scale, fps, scene), "--ml-dump", str(path)]
        start = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO)
        if result.returncode != 0:
            print(result.stdout[-2000:], result.stderr[-2000:], file=sys.stderr)
            sys.exit(f"{name}: failed")
        psnr = re.search(r"interpolated\s+PSNR\s+([0-9.]+)", result.stdout)
        blend = re.search(r"50/50 blend\s+PSNR\s+([0-9.]+)", result.stdout)
        size = path.stat().st_size
        rows[name] = {"name": name, "split": split, "scale": scale, "fps": fps,
                      "psnr_heuristic": psnr.group(1) if psnr else "",
                      "psnr_blend": blend.group(1) if blend else "",
                      "megabytes": f"{size / 1e6:.0f}", "args": " ".join(cmd[1:-2])}
        print(f"{name}: heuristic {rows[name]['psnr_heuristic']} dB, blend "
              f"{rows[name]['psnr_blend']} dB, {size / 1e6:.0f} MB, {time.time() - start:.0f} s")

    with manifest.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["name", "split", "scale", "fps", "psnr_heuristic",
                                               "psnr_blend", "megabytes", "args"])
        writer.writeheader()
        for _, name, *_ in RUNS:
            if name in rows:
                writer.writerow(rows[name])


if __name__ == "__main__":
    main()
