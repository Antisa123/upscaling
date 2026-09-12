#!/usr/bin/env python3
"""Builds the M6 acceptance figure: estimated flow next to the exact vectors.

The milestone's criterion is visual -- the HSV debug view has to look right --
and "looks right" only means something against a reference in the same encoding
at the same instant. The rasteriser's own motion vectors are that reference:
while the only thing moving is the camera they are exact, and debug view 7
draws them on the flow's block grid and with the flow's colour scale, so the
two images differ only where the estimator is wrong.

Four panels: the presented frame, the estimated flow, the reference vectors,
and the match error of the winning block (green = matched well, red = no good
match found anywhere in the search window).

Usage:
    scripts/flow_debug.py [--frame 80] [--flow-scale 120] [--out captures/flow]
"""

import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parent.parent
SPONZA = REPO / "assets/sponza/Sponza.gltf"

# (debug view, output file, caption)
PANELS = [
    (0, "m0.png", "Prikazani okvir (1080p, render 720p, FSR)"),
    (6, "m6.png", "Procijenjeni tok (HSV: nijansa = smjer, zasicenost = iznos)"),
    (7, "m7.png", "Vektori gibanja iz rasterizacije, ista skala (referenca)"),
    (8, "m8.png", "Pogreska podudaranja (zeleno = dobar pogodak, crveno = nema)"),
]


def shot(binary, view, out_png, common, flow_scale):
    cmd = [str(binary), *common, "--debug-view", str(view),
           "--flow-scale", str(flow_scale), "--shot", "--out", str(out_png)]
    proc = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"[shot] failed: {' '.join(cmd[1:])}")
    if not out_png.exists():
        raise SystemExit(f"[shot] {out_png} was not written; --shot needs --out")
    return Image.open(out_png).convert("RGB")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--out", default="captures/flow")
    ap.add_argument("--width", type=int, default=1920)
    ap.add_argument("--height", type=int, default=1080)
    ap.add_argument("--scale", default="1.5")
    ap.add_argument("--frame", type=int, default=80)
    ap.add_argument("--fixed-dt", default="0.016667")
    # Display pixels mapped to full saturation. The application default of 24
    # is set for the render-resolution motion vector views and saturates to
    # flat red on this orbit, where the image moves by tens to hundreds of
    # pixels per frame: the figure then shows that something is moving and
    # nothing else. 40 keeps the whole frame inside the colour wheel at 60 fps;
    # a faster capture wants a larger number.
    ap.add_argument("--flow-scale", default="40")
    ap.add_argument("--scene", default=str(SPONZA) if SPONZA.exists() else "")
    args = ap.parse_args()

    binary = REPO / args.build_dir / "fsr3lite"
    if not binary.exists():
        raise SystemExit(f"[shot] no binary at {binary}")
    out_dir = REPO / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    common = ["--scripted", "--fixed-dt", args.fixed_dt, "--jitter",
              "--width", str(args.width), "--height", str(args.height),
              "--scale", args.scale, "--upscaler", "fsr", "--validate-flow",
              "--frames", str(args.frame)]
    if args.scene:
        common += ["--scene", args.scene, "--scene-fit", "12",
                   "--path-radius", "3", "--path-phase", "-0.6"]

    images = [(shot(binary, view, out_dir / name, common, args.flow_scale), caption)
              for view, name, caption in PANELS]

    half = (args.width // 2, args.height // 2)
    label_h = 28
    sheet = Image.new("RGB", (half[0] * 2, (half[1] + label_h) * 2), (16, 16, 16))
    draw = ImageDraw.Draw(sheet)
    for i, (image, caption) in enumerate(images):
        x = (i % 2) * half[0]
        y = (i // 2) * (half[1] + label_h)
        draw.text((x + 8, y + 8), caption, fill=(255, 255, 255))
        sheet.paste(image.resize(half, Image.LANCZOS), (x, y + label_h))

    path = out_dir / "hsv_compare.png"
    sheet.save(path)
    print(f"[out] {path.relative_to(REPO)}  (okvir {args.frame}, flow-scale {args.flow_scale})")


if __name__ == "__main__":
    main()
