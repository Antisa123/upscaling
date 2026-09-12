#!/usr/bin/env python3
"""Provjera ispravnosti ground-truth dumpa (`fsr3lite --capture-gt <dir>`).

Za svaki trojac okvira provjerava dvije stvari koje moraju vrijediti ako je
među-okvir stvarno renderiran na t-0.5*dt:

  1. `mid_N` je podjednako udaljen od `gt_{N-1}` i `gt_N` (simetrija);
  2. `mid_N` je bliži svakom susjedu nego što su susjedi jedan drugome
     (inače uopće ne leži "između" njih).

Ispisuje i PSNR naivnog 50/50 blenda susjeda naspram pravog među-okvira —
to je donja granica koju generiranje okvira mora nadmašiti.

Koristi samo Pillow (bez numpyja).
"""

import argparse
import csv
import math
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def psnr(a: Image.Image, b: Image.Image) -> float:
    stat = ImageStat.Stat(ImageChops.difference(a, b))
    mse = sum(stat.sum2) / (a.size[0] * a.size[1] * len(a.getbands()))
    return float("inf") if mse == 0 else 10.0 * math.log10(255.0 * 255.0 / mse)


def load(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", nargs="?", default="captures/gt",
                    help="direktorij s gt_*/mid_*/lr_* okvirima")
    # Simetrija nikad nije savršena: među-okvir je renderiran, a ne interpoliran,
    # pa razlika u sadržaju (a ne u vremenu) daje mali razmak između dva PSNR-a.
    ap.add_argument("--tolerance", type=float, default=1.0,
                    help="dopuštena asimetrija u dB")
    args = ap.parse_args()

    root = Path(args.dir)
    manifest = root / "manifest.csv"
    if not manifest.exists():
        print(f"nema {manifest} — je li pokrenut --capture-gt?", file=sys.stderr)
        return 1

    with manifest.open() as fh:
        rows = list(csv.DictReader(fh))

    print(f"{'idx':>4} {'gt-gt':>8} {'mid-gt0':>8} {'mid-gt1':>8} "
          f"{'asim':>7} {'blend':>8}  ocjena")

    failures = 0
    checked = 0
    for row in rows:
        index = int(row["index"])
        prev_path = root / f"gt_{index - 1:04d}.png"
        cur_path = root / f"gt_{index:04d}.png"
        mid_path = root / f"mid_{index:04d}.png"
        if not (prev_path.exists() and cur_path.exists() and mid_path.exists()):
            continue

        prev, cur, mid = load(prev_path), load(cur_path), load(mid_path)
        between = psnr(prev, cur)
        to_prev = psnr(mid, prev)
        to_cur = psnr(mid, cur)
        asym = abs(to_prev - to_cur)
        blend = psnr(mid, ImageChops.blend(prev, cur, 0.5))

        ok = asym <= args.tolerance and min(to_prev, to_cur) > between
        checked += 1
        failures += not ok
        print(f"{index:>4} {between:8.2f} {to_prev:8.2f} {to_cur:8.2f} "
              f"{asym:7.2f} {blend:8.2f}  {'ok' if ok else 'NE VALJA'}")

    if checked == 0:
        print("nijedan trojac nije potpun (prvi okvir nema prethodnika)",
              file=sys.stderr)
        return 1

    print(f"\n{checked - failures}/{checked} trojaca zadovoljava uvjete "
          f"(asimetrija <= {args.tolerance:.2f} dB, među-okvir bliži "
          f"susjedima nego oni jedan drugome)")
    print("stupac 'blend' je PSNR naivnog 50/50 blenda susjeda naspram pravog "
          "među-okvira — to generiranje okvira mora nadmašiti")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
