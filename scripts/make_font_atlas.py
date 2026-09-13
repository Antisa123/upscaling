#!/usr/bin/env python3
"""Rasterises the HUD font into assets/ui_font.png.

The HUD needs text, and text needs glyphs; a PNG atlas generated once from a
monospace TrueType font is the smallest dependency that gives legible ones.
The atlas is committed, so building the project does not need this script or
the font -- it only documents where the image came from.

Layout: 16 columns x 6 rows covering ASCII 32..127, every cell the same size,
white glyph coverage in the alpha channel. DejaVu Sans Mono is under the
Bitstream Vera licence, which permits embedding.
"""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ap = argparse.ArgumentParser()
ap.add_argument("--font", default="/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf")
ap.add_argument("--size", type=int, default=15)
ap.add_argument("--out", default=str(Path(__file__).resolve().parent.parent / "assets/ui_font.png"))
args = ap.parse_args()

font = ImageFont.truetype(args.font, args.size)
ascent, descent = font.getmetrics()
cell_w = int(round(font.getlength("M")))
cell_h = ascent + descent
cols, rows = 16, 6
atlas = Image.new("L", (cols * cell_w, rows * cell_h), 0)
draw = ImageDraw.Draw(atlas)
for code in range(32, 128):
    i = code - 32
    draw.text(((i % cols) * cell_w, (i // cols) * cell_h), chr(code),
              font=font, fill=255)
rgba = Image.merge("RGBA", (Image.new("L", atlas.size, 255),) * 3 + (atlas,))
Path(args.out).parent.mkdir(parents=True, exist_ok=True)
rgba.save(args.out)
print(f"wrote {args.out}: cell {cell_w}x{cell_h}, atlas {atlas.size[0]}x{atlas.size[1]}")
