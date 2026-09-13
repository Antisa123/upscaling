#!/usr/bin/env python3
"""M8: frame pacing and latency measurements.

Runs the application in realtime (wall-clock time step, --run-seconds) over a
grid of synthetic load levels and presentation modes, reads the presenter's
per-image log (--present-csv) and reports what actually reached the screen:

  * presented frame rate and the distribution of present intervals
    (mean, standard deviation, 1st / 50th / 99th percentile),
  * the two alternating intervals separately (generated -> real and
    real -> generated): pacing is about whether those two are equal,
  * latency from input sampling to the real frame on screen, and to the
    first image that shows anything of that input (the generated frame),
  * how long the presenter waited on the GPU before each swap.

Latency here ends at the swap. What the compositor and the display add after
it is invisible to the application and is not included -- see docs/PACING.md.

Pure standard library + Pillow for the figures. No numpy.

    python3 scripts/run_pacing.py                 # everything
    python3 scripts/run_pacing.py --loads 0,12    # fewer load levels
    python3 scripts/run_pacing.py --report-only   # re-analyse existing logs
"""

import argparse
import csv
import math
import statistics
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SPONZA = REPO / "assets/sponza/Sponza.gltf"
BASE = ["--width", "1920", "--height", "1080", "--scale", "1.5", "--upscaler", "fsr",
        "--scene", str(SPONZA), "--scene-fit", "12", "--path-radius", "3",
        "--path-phase", "-0.6", "--scripted"]

# Presentation modes: (tag, label, extra arguments).
MODES = [
    ("nofg", "bez FG", []),
    ("immediate", "FG, odmah", ["--fg", "--pacing", "immediate"]),
    ("paced", "FG, ravnomjerno", ["--fg", "--pacing", "paced"]),
]
# Extra rows that are not part of the load grid.
EXTRA = [
    # The two render-side alternatives to staging, at the load where pacing
    # matters (docs/PACING.md, "Zašto staging").
    ("paced-flush-L12", "FG ravnomjerno + glFlush po fazi", 12, ["--fg", "--pacing", "paced", "--gpu-flush", "1"]),
    ("paced-finish-L12", "FG ravnomjerno + glFinish po fazi", 12, ["--fg", "--pacing", "paced", "--gpu-flush", "2"]),
    # vsync on: the compositor's refresh rate is the ceiling.
    ("nofg-vsync-L12", "bez FG, vsync", 12, ["--vsync"]),
    ("paced-vsync-L12", "FG ravnomjerno, vsync", 12, ["--fg", "--pacing", "paced", "--vsync"]),
    ("nofg-vsync-L24", "bez FG, vsync", 24, ["--vsync"]),
    ("paced-vsync-L24", "FG ravnomjerno, vsync", 24, ["--fg", "--pacing", "paced", "--vsync"]),
]

WARMUP_FRAMES = 30


def percentile(sorted_values, p):
    if not sorted_values:
        return float("nan")
    k = (len(sorted_values) - 1) * p
    lo, hi = math.floor(k), math.ceil(k)
    if lo == hi:
        return sorted_values[lo]
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (k - lo)


def mean(values):
    return statistics.fmean(values) if values else float("nan")


def stdev(values):
    return statistics.pstdev(values) if len(values) > 1 else float("nan")


def analyse(path):
    rows = [r for r in csv.DictReader(open(path)) if int(r["frame"]) >= WARMUP_FRAMES]
    if len(rows) < 3:
        return None
    t = [float(r["present_ms"]) for r in rows]
    kinds = [r["interpolated"] == "1" for r in rows]
    intervals = [b - a for a, b in zip(t, t[1:])]
    gen_to_real = [b - a for a, b, ka, kb in zip(t, t[1:], kinds, kinds[1:]) if ka and not kb]
    real_to_gen = [b - a for a, b, ka, kb in zip(t, t[1:], kinds, kinds[1:]) if not ka and kb]
    real = [r for r in rows if r["interpolated"] == "0"]
    gen = [r for r in rows if r["interpolated"] == "1"]
    lat_real = sorted(float(r["present_ms"]) - float(r["sample_ms"]) for r in real)
    lat_gen = sorted(float(r["present_ms"]) - float(r["sample_ms"]) for r in gen)
    span_s = (t[-1] - t[0]) / 1000.0
    s = sorted(intervals)
    return {
        "presents": len(rows),
        "presented_fps": (len(rows) - 1) / span_s if span_s > 0 else float("nan"),
        "rendered_fps": (len(real) - 1) / span_s if span_s > 0 else float("nan"),
        "interval_mean": mean(intervals),
        "interval_std": stdev(intervals),
        "interval_p1": percentile(s, 0.01),
        "interval_p50": percentile(s, 0.50),
        "interval_p99": percentile(s, 0.99),
        "gen_to_real": mean(gen_to_real),
        "real_to_gen": mean(real_to_gen),
        "latency_real": mean(lat_real),
        "latency_real_p99": percentile(lat_real, 0.99),
        "latency_gen": mean(lat_gen),
        "gpu_wait": mean([float(r["gpu_wait_ms"]) for r in rows]),
        "caught_up": sum(int(r["caught_up"]) for r in rows),
        "intervals": intervals,
        "kinds": kinds,
    }


def run(binary, out_dir, tag, load, extra, seconds):
    present_csv = out_dir / f"{tag}.csv"
    log = out_dir / f"{tag}.log"
    cmd = [str(binary), *BASE, "--run-seconds", str(seconds), "--load", str(load),
           "--present-csv", str(present_csv), *extra]
    print(f"[pacing] {tag}: {' '.join(extra) or '(no FG)'}  load x{load}", flush=True)
    with open(log, "w") as f:
        result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=REPO)
    if result.returncode != 0:
        print(f"[pacing] {tag} failed, see {log}", file=sys.stderr)
    return present_csv


def gpu_total(log_path):
    try:
        for line in open(log_path):
            if line.startswith("[gpu] TOTAL"):
                return float(line.split()[2])
    except OSError:
        pass
    return float("nan")


# --- figures ----------------------------------------------------------------

COLORS = {"nofg": (90, 90, 90), "immediate": (220, 80, 60), "paced": (40, 150, 70)}


def load_font(size):
    from PIL import ImageFont
    for name in ("/usr/share/fonts/TTF/DejaVuSans.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def histogram_figure(results, load, path, bin_ms=0.5, max_ms=25.0):
    from PIL import Image, ImageDraw
    W, H, M = 1000, 700, 60
    panel_h = (H - 2 * M) // len(MODES)
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)
    font, small = load_font(18), load_font(13)
    d.text((M, 15), f"Razdioba intervala prikaza, sintetsko opterećenje x{load}", fill="black", font=font)
    bins = int(max_ms / bin_ms)
    for i, (tag, label, _) in enumerate(MODES):
        r = results.get(f"{tag}-L{load}")
        top = M + i * panel_h
        bottom = top + panel_h - 30
        d.rectangle([M, top, W - M, bottom], outline=(200, 200, 200))
        if not r:
            continue
        counts = [0] * bins
        for v in r["intervals"]:
            counts[min(int(v / bin_ms), bins - 1)] += 1
        peak = max(counts) or 1
        bw = (W - 2 * M) / bins
        for b, c in enumerate(counts):
            h = (bottom - top - 4) * c / peak
            d.rectangle([M + b * bw, bottom - h, M + (b + 1) * bw - 1, bottom], fill=COLORS[tag])
        d.text((M + 8, top + 6),
               f"{label}: {r['presented_fps']:.1f} fps, std {r['interval_std']:.2f} ms, "
               f"p1 {r['interval_p1']:.2f} / p99 {r['interval_p99']:.2f} ms",
               fill="black", font=small)
        for ms in range(0, int(max_ms) + 1, 5):
            x = M + ms / bin_ms * bw
            d.line([x, bottom, x, bottom + 5], fill="black")
            d.text((x - 8, bottom + 7), f"{ms}", fill="black", font=small)
    d.text((W // 2 - 60, H - 25), "interval (ms)", fill="black", font=small)
    img.save(path)


def timeline_figure(results, load, path, count=80, max_ms=25.0):
    from PIL import Image, ImageDraw
    W, H, M = 1000, 700, 60
    panel_h = (H - 2 * M) // len(MODES)
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)
    font, small = load_font(18), load_font(13)
    d.text((M, 15), f"Uzastopni intervali prikaza (zeleno: generirani -> stvarni, "
                    f"crveno: stvarni -> generirani), opterećenje x{load}", fill="black", font=small)
    for i, (tag, label, _) in enumerate(MODES):
        r = results.get(f"{tag}-L{load}")
        top = M + i * panel_h
        bottom = top + panel_h - 30
        d.rectangle([M, top, W - M, bottom], outline=(200, 200, 200))
        if not r:
            continue
        iv = r["intervals"][:count]
        kinds = r["kinds"]
        bw = (W - 2 * M) / count
        for j, v in enumerate(iv):
            h = (bottom - top - 20) * min(v / max_ms, 1.0)
            # Interval j ends at present j+1; colour by the kind of the pair.
            color = (40, 150, 70) if kinds[j] and not kinds[j + 1] else \
                    (220, 80, 60) if not kinds[j] and kinds[j + 1] else (90, 90, 90)
            d.rectangle([M + j * bw + 1, bottom - h, M + (j + 1) * bw - 1, bottom], fill=color)
        d.text((M + 8, top + 4), f"{label}", fill="black", font=small)
    img.save(path)


def fps_figure(results, loads, path):
    from PIL import Image, ImageDraw
    W, H, M = 1000, 600, 70
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)
    font, small = load_font(18), load_font(13)
    d.text((M, 15), "Prikazani FPS u ovisnosti o opterećenju renderiranja", fill="black", font=font)
    ymax = max((r["presented_fps"] for r in results.values() if r), default=1.0) * 1.1
    xmax = max(loads) or 1
    def pt(load, fps):
        return (M + (W - 2 * M) * load / xmax, H - M - (H - 2 * M) * fps / ymax)
    d.line([M, H - M, W - M, H - M], fill="black")
    d.line([M, M, M, H - M], fill="black")
    for fps in range(0, int(ymax) + 1, 50):
        y = pt(0, fps)[1]
        d.line([M - 5, y, W - M, y], fill=(230, 230, 230))
        d.text((M - 40, y - 8), f"{fps}", fill="black", font=small)
    for load in loads:
        x = pt(load, 0)[0]
        d.text((x - 8, H - M + 8), f"x{load}", fill="black", font=small)
    legend_y = M
    for tag, label, _ in MODES:
        pts = [pt(l, results[f"{tag}-L{l}"]["presented_fps"]) for l in loads if results.get(f"{tag}-L{l}")]
        if len(pts) > 1:
            d.line(pts, fill=COLORS[tag], width=3)
        for p in pts:
            d.ellipse([p[0] - 4, p[1] - 4, p[0] + 4, p[1] + 4], fill=COLORS[tag])
        d.line([W - 260, legend_y + 8, W - 230, legend_y + 8], fill=COLORS[tag], width=3)
        d.text((W - 222, legend_y), label, fill="black", font=small)
        legend_y += 22
    d.text((W // 2 - 120, H - 30), "sintetsko opterećenje (dodatni G-buffer prolazi)", fill="black", font=small)
    img.save(path)


# --- report -----------------------------------------------------------------

COLUMNS = [
    ("presented_fps", "prikazano fps", "{:.1f}"),
    ("rendered_fps", "renderirano fps", "{:.1f}"),
    ("interval_mean", "interval ms", "{:.2f}"),
    ("interval_std", "std ms", "{:.2f}"),
    ("interval_p1", "p1 ms", "{:.2f}"),
    ("interval_p50", "p50 ms", "{:.2f}"),
    ("interval_p99", "p99 ms", "{:.2f}"),
    ("gen_to_real", "gen→stv ms", "{:.2f}"),
    ("real_to_gen", "stv→gen ms", "{:.2f}"),
    ("latency_real", "latencija stv ms", "{:.2f}"),
    ("latency_real_p99", "lat. stv p99", "{:.2f}"),
    ("latency_gen", "latencija gen ms", "{:.2f}"),
    ("gpu_wait", "čekanje GPU ms", "{:.2f}"),
    ("gpu_total", "GPU ukupno ms", "{:.2f}"),
]


def fmt(value, pattern):
    return "—" if value is None or (isinstance(value, float) and math.isnan(value)) else pattern.format(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=str(REPO / "build"))
    parser.add_argument("--out", default=str(REPO / "captures/pacing"))
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--loads", default="0,4,8,12,16,24")
    parser.add_argument("--no-extra", action="store_true")
    parser.add_argument("--report-only", action="store_true")
    args = parser.parse_args()

    binary = Path(args.build_dir) / "fsr3lite"
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    loads = [int(x) for x in args.loads.split(",")]

    plan = []  # (tag, label, load, extra)
    for load in loads:
        for tag, label, extra in MODES:
            plan.append((f"{tag}-L{load}", label, load, extra))
    if not args.no_extra:
        plan += [(tag, label, load, extra) for tag, label, load, extra in EXTRA]

    results = {}
    for tag, label, load, extra in plan:
        if not args.report_only:
            run(binary, out_dir, tag, load, extra, args.seconds)
        path = out_dir / f"{tag}.csv"
        r = analyse(path) if path.exists() else None
        if r:
            r["gpu_total"] = gpu_total(out_dir / f"{tag}.log")
        results[tag] = r

    with open(out_dir / "summary.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["run", "label", "load", *[c[0] for c in COLUMNS], "caught_up"])
        for tag, label, load, _ in plan:
            r = results.get(tag)
            if r:
                w.writerow([tag, label, load, *[f"{r[c[0]]:.4f}" for c in COLUMNS], r["caught_up"]])

    with open(out_dir / "summary.md", "w") as f:
        f.write("| run | način | opt. | " + " | ".join(c[1] for c in COLUMNS) + " |\n")
        f.write("|---|---|---|" + "---:|" * len(COLUMNS) + "\n")
        for tag, label, load, _ in plan:
            r = results.get(tag)
            if not r:
                continue
            f.write(f"| {tag} | {label} | x{load} | " +
                    " | ".join(fmt(r[c[0]], c[2]) for c in COLUMNS) + " |\n")
    print((out_dir / "summary.md").read_text())

    try:
        for load in loads:
            histogram_figure(results, load, out_dir / f"histogram-L{load}.png")
            timeline_figure(results, load, out_dir / f"timeline-L{load}.png")
        fps_figure(results, loads, out_dir / "fps-vs-load.png")
    except ImportError:
        print("[pacing] Pillow not available, figures skipped", file=sys.stderr)


if __name__ == "__main__":
    main()
