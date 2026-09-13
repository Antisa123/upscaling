#!/usr/bin/env python3
"""Builds the M7 acceptance figure: the interpolated frame next to the real one.

The milestone's criterion is that the interpolated frame exists and is
measurable against ground truth. The measurement is the `fg` groups in
run_metrics.py; this is the picture that goes with it, because a PSNR says how
much is wrong and not where.

Six panels, all at the same instant t - dt/2 unless stated:

  the previous and the current presented frame (t-1 and t), the real frame
  rendered at t - dt/2, the interpolated frame, the 50/50 blend of the two
  inputs, and the module's debug view (red = hidden in the previous frame,
  green = hidden in the current one, blue = vector taken from the inpainting
  pyramid rather than measured).

The real frame comes from --capture-gt, which writes mid_<index>.png with
index = frame - 2. The run is lockstep and the capture does not alter the
pipeline, so a separate run for each debug view lands on the same frame.

Usage:
    scripts/fg_debug.py [--frame 40] [--fixed-dt 0.0333333] [--out captures/framegen]
"""

import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

REPO = Path(__file__).resolve().parent.parent
SPONZA = REPO / "assets/sponza/Sponza.gltf"


def run(binary, args):
    proc = subprocess.run([str(binary), *args], cwd=REPO, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"[run] failed: {' '.join(args)}")
    return proc.stdout


def shot(binary, common, frames, view, out_png):
    run(binary, [*common, "--frames", str(frames), "--debug-view", str(view),
                 "--shot", "--out", str(out_png)])
    if not out_png.exists():
        raise SystemExit(f"[shot] {out_png} was not written")
    return Image.open(out_png).convert("RGB")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--out", default="captures/framegen")
    ap.add_argument("--width", type=int, default=1920)
    ap.add_argument("--height", type=int, default=1080)
    ap.add_argument("--scale", default="1.5")
    ap.add_argument("--frame", type=int, default=40)
    # 30 fps by default: at 60 fps on this orbit the interpolated frame and the
    # blend both look fine at figure size, and the figure is for showing where
    # they differ.
    ap.add_argument("--fixed-dt", default="0.0333333")
    ap.add_argument("--scene", default=str(SPONZA) if SPONZA.exists() else "")
    ap.add_argument("--extra", nargs=argparse.REMAINDER, default=[],
                    help="further application arguments, e.g. --fg-masks 0")
    args = ap.parse_args()

    binary = REPO / args.build_dir / "fsr3lite"
    if not binary.exists():
        raise SystemExit(f"[shot] no binary at {binary}")
    out_dir = REPO / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    common = ["--scripted", "--fixed-dt", args.fixed_dt, "--jitter",
              "--width", str(args.width), "--height", str(args.height),
              "--scale", args.scale, "--upscaler", "fsr", "--fg"]
    if args.scene:
        common += ["--scene", args.scene, "--scene-fit", "12",
                   "--path-radius", "3", "--path-phase", "-0.6"]
    common += args.extra

    # A --frames N run presents frame N-1 last, so the pair around the
    # interpolated frame is (N-2, N-1) and its reference is mid_<N-1-2>.
    n = args.frame
    previous = shot(binary, common, n - 1, 0, out_dir / "prev.png")
    current = shot(binary, common, n, 0, out_dir / "cur.png")
    interpolated = shot(binary, common, n, 9, out_dir / "interp.png")
    debug = shot(binary, common, n, 10, out_dir / "debug.png")

    gt_dir = out_dir / "gt"
    run(binary, [*common, "--frames", str(n), "--capture-gt", str(gt_dir)])
    reference = Image.open(gt_dir / f"mid_{n - 1 - 2:04d}.png").convert("RGB")
    blend = Image.blend(previous, current, 0.5)

    panels = [
        (previous, "Prethodni prikazani okvir (t-1)"),
        (current, "Trenutni prikazani okvir (t)"),
        (reference, "Stvarni render na t - dt/2 (referenca)"),
        (interpolated, "Interpolirani okvir (M7)"),
        (blend, "50/50 blend istog para (baseline)"),
        (debug, "Maske: crveno = skriveno u t-1, zeleno = u t, plavo = popunjeno"),
    ]

    third = (args.width // 3, args.height // 3)
    label_h = 24
    sheet = Image.new("RGB", (third[0] * 3, (third[1] + label_h) * 2), (16, 16, 16))
    draw = ImageDraw.Draw(sheet)
    for i, (image, caption) in enumerate(panels):
        x = (i % 3) * third[0]
        y = (i // 3) * (third[1] + label_h)
        draw.text((x + 6, y + 6), caption, fill=(255, 255, 255))
        sheet.paste(image.resize(third, Image.LANCZOS), (x, y + label_h))
    path = out_dir / "fg_compare.png"
    sheet.save(path)

    # Absolute error against the reference, amplified 4x: where the blend
    # doubles every edge the interpolated frame should be dark.
    def error(image):
        return ImageChops.difference(image, reference).point(lambda v: min(255, 4 * v))
    pair = Image.new("RGB", (third[0] * 2, third[1] + label_h), (16, 16, 16))
    d = ImageDraw.Draw(pair)
    for i, (image, caption) in enumerate(((interpolated, "|interpolirani - referenca| x4"),
                                          (blend, "|blend - referenca| x4"))):
        d.text((i * third[0] + 6, 6), caption, fill=(255, 255, 255))
        pair.paste(error(image).resize(third, Image.LANCZOS), (i * third[0], label_h))
    err_path = out_dir / "fg_error.png"
    pair.save(err_path)

    print(f"[out] {path.relative_to(REPO)}, {err_path.relative_to(REPO)}  "
          f"(okvir {n}, dt {args.fixed_dt})")


if __name__ == "__main__":
    main()
