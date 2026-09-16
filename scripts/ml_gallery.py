#!/usr/bin/env python3
"""M9: gallery of the learned blend against the heuristic, for docs/ML.md.

Frames are chosen from the stored fg-ml measurements, not by eye: for each
view the heuristic's worst frame, the frame where the network gains most and
the frame where it loses most. Those frames are re-rendered with --fg-shots
(reference, heuristic, learned, weight view), and from each one two 256 px
crops are cut -- where the network helps most and where it hurts most, by
block error against the reference. Showing only the first kind would be a
brochure, not a measurement.

Output: captures/ml/gallery/<view>-frame<N>.png and a legend. Pillow only.

    scripts/ml_gallery.py [--weights captures/ml/weights/blend-c8-c16.bin]
"""

import argparse
import csv
import math
import pathlib
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_metrics as rm  # noqa: E402

REPO = rm.REPO
OUT = REPO / "captures" / "ml" / "gallery"
CROP = 256
BLOCK = 32
# (view, run_metrics row without the -heur/-net suffix)
VIEWS = [("sponza-1080p-q", "fg-ml-1080p-q"), ("sponza-20fps", "fg-ml-speed-20"),
         ("proceduralna", "fg-ml-proc")]
CANDIDATES = [("heuristika", (128, 128, 128)), ("game warp t-1", (13, 140, 13)),
              ("game warp t", (115, 255, 89)), ("flow warp t-1", (13, 51, 204)),
              ("flow warp t", (89, 204, 255)), ("trenutni okvir", (255, 38, 25)),
              ("prethodni okvir", (255, 153, 25))]


def font(size):
    for path in ("/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if pathlib.Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def per_frame(name):
    with open(REPO / "captures" / "metrics" / f"{name}.csv", newline="") as f:
        return {int(r["frame"]): float(r["psnr_fg"]) for r in csv.DictReader(f)}


def pick_frames(row):
    heur, net = per_frame(f"{row}-heur"), per_frame(f"{row}-net")
    frames = sorted(set(heur) & set(net))
    worst = min(frames, key=lambda f: heur[f])
    best_gain = max(frames, key=lambda f: net[f] - heur[f])
    worst_gain = min(frames, key=lambda f: net[f] - heur[f])
    return {f: (heur[f], net[f]) for f in dict.fromkeys([worst, best_gain, worst_gain])}


def run_row(row, weights, frames, shots):
    entry = next(r for r in rm.RUNS if r[1] == f"{row}-heur")
    group, extra = entry[0], entry[3]
    cmd = [str(REPO / "build" / "fsr3lite"), *rm.COMMON, "--frames", "120", *rm.GROUPS[group][0],
           *extra, "--fg-ml", str(weights), "--fg-shots", str(shots),
           "--fg-shot-frames", ",".join(str(f) for f in frames)]
    result = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(result.stderr[-2000:])


def psnr(a, b):
    diff = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    hist = diff.histogram()
    sq = sum(count * (value % 256) ** 2 for value, count in enumerate(hist))
    mse = sq / (3 * a.width * a.height)
    return 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)


def block_error(img, ref):
    diff = ImageChops.difference(img.convert("RGB"), ref.convert("RGB")).convert("L")
    return diff.resize((img.width // BLOCK, img.height // BLOCK), Image.BOX)


def crop_boxes(heur, net, ref):
    eh, en = block_error(heur, ref), block_error(net, ref)
    w, h = eh.size
    span = CROP // BLOCK
    best, worst = None, None
    # Sum over a crop-sized window of blocks; step of half a crop.
    for by in range(0, h - span + 1, span // 2):
        for bx in range(0, w - span + 1, span // 2):
            box = (bx, by, bx + span, by + span)
            gain = sum(eh.crop(box).getdata()) - sum(en.crop(box).getdata())
            if best is None or gain > best[0]:
                best = (gain, box)
            if worst is None or gain < worst[0]:
                worst = (gain, box)
    to_px = lambda b: (b[0] * BLOCK, b[1] * BLOCK, b[2] * BLOCK, b[3] * BLOCK)
    return [("mreža najviše pomaže", to_px(best[1])), ("mreža najviše šteti", to_px(worst[1]))]


def amplified_error(img, ref):
    return ImageChops.difference(img.convert("RGB"), ref.convert("RGB")).point(lambda v: min(255, v * 6))


def figure(view, frame, scores, shots, path):
    load = lambda kind: Image.open(shots / f"frame{frame}_{kind}.png").convert("RGB")
    ref, heur, net, weights = load("reference"), load("heuristic"), load("learned"), load("weights")
    columns = [("referenca", lambda b: ref.crop(b)), ("heuristika", lambda b: heur.crop(b)),
               ("naučena mješavina", lambda b: net.crop(b)), ("težine", lambda b: weights.crop(b)),
               ("pogreška heuristike ×6", lambda b: amplified_error(heur.crop(b), ref.crop(b))),
               ("pogreška mreže ×6", lambda b: amplified_error(net.crop(b), ref.crop(b)))]
    boxes = crop_boxes(heur, net, ref)
    pad, header, row_label = 8, 64, 26
    width = pad + len(columns) * (CROP + pad)
    height = header + len(boxes) * (row_label + CROP + pad)
    sheet = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(sheet)
    title, small = font(18), font(13)
    heur_psnr, net_psnr = scores
    draw.text((pad, 6), f"{view}, okvir {frame}: heuristika {heur_psnr:.2f} dB, mreža {net_psnr:.2f} dB "
                        f"({net_psnr - heur_psnr:+.2f} dB, cijeli okvir)", fill="black", font=title)
    for c, (label, _) in enumerate(columns):
        draw.text((pad + c * (CROP + pad), 38), label, fill="black", font=small)
    y = header
    for label, box in boxes:
        crop_h, crop_n = psnr(heur.crop(box), ref.crop(box)), psnr(net.crop(box), ref.crop(box))
        draw.text((pad, y + 4), f"{label}: izrez {box[0]},{box[1]}  heuristika {crop_h:.2f} dB, "
                                f"mreža {crop_n:.2f} dB", fill="black", font=small)
        y += row_label
        for c, (_, make) in enumerate(columns):
            sheet.paste(make(box), (pad + c * (CROP + pad), y))
        y += CROP + pad
    sheet.save(path)


def legend(path):
    sheet = Image.new("RGB", (330, 30 + 26 * len(CANDIDATES)), "white")
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 6), "Boje prikaza težina", fill="black", font=font(16))
    for i, (label, color) in enumerate(CANDIDATES):
        draw.rectangle((8, 32 + 26 * i, 38, 52 + 26 * i), fill=color)
        draw.text((48, 34 + 26 * i), label, fill="black", font=font(14))
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--weights", default=str(REPO / "captures/ml/weights/blend-c8-c16.bin"))
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    legend(OUT / "legend.png")
    for view, row in VIEWS:
        frames = pick_frames(row)
        shots = OUT / "shots" / view
        run_row(row, args.weights, list(frames), shots)
        for frame, scores in frames.items():
            path = OUT / f"{view}-frame{frame}.png"
            figure(view, frame, scores, shots, path)
            print(f"{path.relative_to(REPO)}: heuristic {scores[0]:.2f} dB, network {scores[1]:.2f} dB")


if __name__ == "__main__":
    main()
