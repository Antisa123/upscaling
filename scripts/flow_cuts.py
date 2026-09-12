#!/usr/bin/env python3
"""Measures how well the scene-change detector separates cuts from fast motion.

The detector compares nine per-section luminance histograms between this frame
and the last and calls the frame a cut when a statistic over the nine section
distances exceeds a threshold. Which statistic -- the worst section, the mean
or the median -- is not obvious a priori, and getting it wrong is expensive in
both directions: a missed cut feeds the estimator two unrelated images, and a
false positive throws away a perfectly good field for a frame. So all three are
written to the CSV every frame and the choice is made from this table.

The capture forces a cut every N frames with --cut-every, which teleports the
camera half an orbit and marks the frame. The two numbers that matter per
statistic are then the smallest distance a real cut produced and the largest
distance continuous camera motion produced: the detector is usable exactly
when the first is above the second, and the threshold belongs between them.

Usage:
    scripts/flow_cuts.py [--build-dir build] [--out captures/metrics/cuts]
                         [--threshold 0.25]
"""

import argparse
import csv
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SPONZA = REPO / "assets/sponza/Sponza.gltf"

COMMON = ["--scripted", "--fixed-dt", "0.016667", "--jitter", "--validate-flow",
          "--width", "1920", "--height", "1080", "--scale", "1.5",
          "--upscaler", "fsr", "--scene", str(SPONZA), "--scene-fit", "12"]

STATISTICS = [("sc_max", "maksimum"), ("sc_mean", "srednja"), ("sc_median", "medijan")]

# Three orbits, not one. The measurement orbit is the hard case -- it stays
# inside the same stone atrium, so half an orbit away looks a great deal like
# where it started and a cut there is genuinely faint -- while the wide orbit
# is the easy case the number would look best on. Reporting only one of them
# would be choosing the answer.
ORBITS = [
    ("orbit-r3", "mjerna orbita (r=3, faza -0.6)", ["--path-radius", "3", "--path-phase", "-0.6"]),
    ("orbit-r5", "siroka orbita (r=5)", ["--path-radius", "5"]),
    ("orbit-r11", "vanjska orbita (r=11)", ["--path-radius", "11"]),
]


def run(binary, name, extra, frames, cut_every, out_dir):
    csv_path = out_dir / f"{name}.csv"
    cmd = [str(binary), *COMMON, *extra, "--cut-every", str(cut_every),
           "--frames", str(frames), "--warmup", "8", "--csv", str(csv_path)]
    print(f"[run] {name}: {' '.join(cmd[1:])}", flush=True)
    proc = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"[run] {name} failed with exit code {proc.returncode}")
    with csv_path.open() as f:
        return list(csv.DictReader(f))


def separation(rows, key):
    cuts = [float(r[key]) for r in rows if r["cut"] == "1"]
    rest = [float(r[key]) for r in rows if r["cut"] != "1"]
    if not cuts or not rest:
        raise SystemExit(f"[agg] no cut frames in the capture; is --cut-every too large?")
    return min(cuts), max(rest), len(cuts), len(rest)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--out", default="captures/metrics/cuts")
    ap.add_argument("--frames", type=int, default=128)
    ap.add_argument("--cut-every", type=int, default=8)
    ap.add_argument("--threshold", type=float, default=0.25)
    args = ap.parse_args()

    binary = REPO / args.build_dir / "fsr3lite"
    if not binary.exists():
        raise SystemExit(f"[run] no binary at {binary}; build first")
    if not SPONZA.exists():
        raise SystemExit(f"[run] {SPONZA} is missing; this measurement needs Sponza")
    out_dir = REPO / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    lines = [f"# Detekcija promjene scene (prag {args.threshold})", "",
             "Najmanja udaljenost koju je proizveo pravi rez naspram najvece koju je "
             "proizvelo obicno gibanje kamere. Detektor je upotrebljiv kad je prvi "
             "broj veci od drugog; prag pripada izmedu njih.", "",
             "| Orbita | Statistika | min(rez) | max(gibanje) | margina | "
             "detektirano rezova | laznih pozitiva |",
             "|---|---|---|---|---|---|---|"]
    for name, label, extra in ORBITS:
        rows = run(binary, name, extra, args.frames, args.cut_every, out_dir)
        for key, stat in STATISTICS:
            lo, hi, ncut, nrest = separation(rows, key)
            hit = sum(1 for r in rows if r["cut"] == "1" and float(r[key]) > args.threshold)
            fp = sum(1 for r in rows if r["cut"] != "1" and float(r[key]) > args.threshold)
            lines.append(f"| {label} | {stat} | {lo:.3f} | {hi:.3f} | {lo - hi:+.3f} | "
                         f"{hit}/{ncut} | {fp}/{nrest} |")

    document = "\n".join(lines) + "\n"
    path = out_dir / "cuts.md"
    path.write_text(document)
    print()
    print(document)
    print(f"[out] {path.relative_to(REPO)}")


if __name__ == "__main__":
    main()
