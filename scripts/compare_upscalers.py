#!/usr/bin/env python3
"""Renders the same frame through every upscaler and builds a crop comparison.

Numbers in a table say which filter is closer to the reference; they do not
say *how* it is wrong. This produces the figure that does: one row per mode,
each a magnified crop of the same region, with the native render as the last
row for reference.

Usage:
    scripts/compare_upscalers.py --scene assets/sponza/Sponza.gltf \
        --crop 0.42 0.35 0.18 0.18 --out captures/compare
"""

import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parent.parent
SPONZA = REPO / "assets/sponza/Sponza.gltf"
MODES = ["nearest", "bilinear", "bicubic", "fsr1", "taau", "fsr"]
# The temporal modes need the sub-pixel offset back on: it is the only thing
# they have to accumulate, and the shared argument list turns it off so the
# spatial modes are not charged for an offset they cannot undo.
TEMPORAL = {"taau", "taau-rcas", "fsr", "fsr-rcas"}
LABELS = {"nearest": "Nearest", "bilinear": "Bilinear", "bicubic": "Bicubic (Catmull-Rom)",
          "fsr1": "FSR1 EASU+RCAS", "taau": "TAAU (M4)", "taau-rcas": "TAAU + RCAS (M4)",
          "fsr": "FSR (M5)", "fsr-rcas": "FSR + RCAS (M5)",
          "native": "Native (referenca)"}


def render(binary, out_png, extra, common):
    cmd = [str(binary), *common, *extra, "--shot", "--out", str(out_png)]
    proc = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"[shot] failed: {' '.join(cmd[1:])}")
    return Image.open(out_png).convert("RGB")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--out", default="captures/compare")
    ap.add_argument("--width", type=int, default=1920)
    ap.add_argument("--height", type=int, default=1080)
    ap.add_argument("--scale", default="2.0")
    ap.add_argument("--frame", type=int, default=80)
    ap.add_argument("--scene", default=str(SPONZA) if SPONZA.exists() else "")
    ap.add_argument("--scene-fit", default="12")
    ap.add_argument("--path-radius", default="3")
    ap.add_argument("--path-phase", default="-0.6")
    ap.add_argument("--crop", nargs=4, type=float, default=[0.40, 0.30, 0.16, 0.16],
                    metavar=("X", "Y", "W", "H"),
                    help="crop rectangle as fractions of the image")
    ap.add_argument("--zoom", type=int, default=4)
    args = ap.parse_args()

    binary = REPO / args.build_dir / "fsr3lite"
    if not binary.exists():
        raise SystemExit(f"[shot] no binary at {binary}")
    out_dir = REPO / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    common = ["--scripted", "--fixed-dt", "0.016667", "--no-jitter",
              "--width", str(args.width), "--height", str(args.height),
              "--frames", str(args.frame)]
    if args.scene:
        common += ["--scene", args.scene, "--scene-fit", args.scene_fit,
                   "--path-radius", args.path_radius, "--path-phase", args.path_phase]

    shots = {}
    for mode in MODES:
        extra = ["--scale", args.scale, "--upscaler", mode]
        if mode in TEMPORAL:
            extra.append("--jitter")
        shots[mode] = render(binary, out_dir / f"{mode}.png", extra, common)
    shots["native"] = render(binary, out_dir / "native.png", ["--scale", "1.0"], common)

    fx, fy, fw, fh = args.crop
    w, h = shots["native"].size
    box = (int(fx * w), int(fy * h), int((fx + fw) * w), int((fy + fh) * h))
    cw, ch = (box[2] - box[0]) * args.zoom, (box[3] - box[1]) * args.zoom

    order = MODES + ["native"]
    label_h = 22
    grid = Image.new("RGB", (cw, (ch + label_h) * len(order)), (16, 16, 16))
    draw = ImageDraw.Draw(grid)
    for i, key in enumerate(order):
        crop = shots[key].crop(box).resize((cw, ch), Image.NEAREST)
        y = i * (ch + label_h)
        draw.text((6, y + 5), LABELS[key], fill=(255, 255, 255))
        grid.paste(crop, (0, y + label_h))

    grid_path = out_dir / "grid.png"
    grid.save(grid_path)
    print(f"[out] {grid_path.relative_to(REPO)}  (crop {box}, zoom {args.zoom}x)")


if __name__ == "__main__":
    main()
