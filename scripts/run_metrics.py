#!/usr/bin/env python3
"""Generates the metrics tables for the thesis with a single command.

Each configuration in RUNS is executed as a separate deterministic capture
(scripted camera, fixed timestep, fixed frame count), writing a per-frame CSV.
This script then aggregates those per-frame rows into summary tables, in both
CSV (for plotting) and Markdown (for pasting into the thesis).

Runs are organised into groups. A group fixes what is being validated - and
therefore which columns the per-frame CSV carries - so motion-vector rows and
upscaler rows do not have to pretend to share a table.

Usage:
    scripts/run_metrics.py [--build-dir build] [--out captures/metrics]
                           [--frames 120] [--only NAME ...] [--group NAME ...]
"""

import argparse
import csv
import statistics
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SPONZA = REPO / "assets/sponza/Sponza.gltf"

# Shared by every run so the numbers are comparable: same camera path, same
# timestep, same number of frames. Anything that differs between rows of the
# table belongs in RUNS, not here.
COMMON = ["--scripted", "--fixed-dt", "0.016667", "--no-jitter"]

TIMING_COLUMNS = [
    ("cpu_ms", "CPU ms", "mean"),
    ("gpu_ms", "GPU ms", "mean"),
]

# Endpoint error is heavy-tailed: on the measurement orbit a handful of frames
# sweep past a column at three hundred pixels per frame, and those frames set
# the mean for the whole capture. Reporting the mean alone would describe four
# frames and not the other hundred, so the same column is aggregated three
# ways. The fractions are reported as percentages because "0.66" in a column
# headed "<1 px" reads as a distance.
FLOW_COLUMNS = [
    ("epe_px", "EPE sred. (px)", "mean"),
    ("epe_px", "EPE medijan (px)", "median"),
    ("epe_px", "EPE p95 (px)", "p95"),
    ("epe_within1", "unutar 1 px (%)", "pct"),
    ("epe_within2", "unutar 2 px (%)", "pct"),
]

# A group is (extra arguments for every row, extra columns, table title).
GROUPS = {
    "mv": (
        ["--validate-mv"],
        [
            ("psnr_reproj", "PSNR reproj. (dB)", "mean"),
            ("ssim_reproj", "SSIM reproj.", "mean"),
            ("psnr_direct", "PSNR bez reproj. (dB)", "mean"),
            ("ssim_direct", "SSIM bez reproj.", "mean"),
        ],
        "Motion vectori: reprojekcija prethodnog okvira",
    ),
    "native": (
        [],
        [],
        "Cijena renderiranja bez upscalera (referentne brojke za ubrzanje)",
    ),
    "upscale": (
        ["--validate-upscale"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "Prostorni upscaleri (M3)",
    ),
    "rcas": (
        ["--validate-upscale"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "RCAS: ablacija ostrine (Sponza, Quality 1.5x)",
    ),
    # --jitter is what makes the temporal rows temporal: COMMON turns jitter
    # off for every other group, and without a sub-pixel offset per frame the
    # accumulation has nothing new to average and TAAU degenerates into a
    # repeatedly applied spatial filter.
    "taau": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "Temporalni upscaler (M4): TAAU",
    ),
    "taau-ablation": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "TAAU: ablacije (Sponza, Quality 1.5x)",
    ),
    # A temporal upscaler has no single quality number: it converges towards a
    # supersample when the camera is still and falls back towards a spatial
    # filter when the image moves faster than the history can be resampled
    # without loss. One row per camera speed is the honest way to report that,
    # and it is the only table in which the spatial baseline ever wins.
    "taau-speed": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "TAAU: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)",
    ),
    # M5. Same columns as the M4 groups so the two tables can be read against
    # each other line by line.
    "fsr": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "Puni upscaler (M5): dilatacija, depth clip, lockovi, Lanczos",
    ),
    "fsr-ablation": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "FSR: ablacije (Sponza, Quality 1.5x, 60 fps orbita)",
    ),
    "fsr-speed": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "FSR: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)",
    ),
    # The same ablations with the camera standing still. Locks and kernel width
    # only pay for themselves where the history is long enough to protect, and
    # the 60 fps orbit shortens it to about one frame -- measuring them only
    # there would report that they do nothing.
    "fsr-ablation-still": (
        ["--validate-upscale", "--jitter"],
        [
            ("psnr_upscale", "PSNR vs. native (dB)", "mean"),
            ("ssim_upscale", "SSIM vs. native", "mean"),
            ("tstab_upscale", "Stabilnost (dB)", "mean"),
            ("tstab_ref", "Stabilnost ref. (dB)", "mean"),
        ],
        "FSR: ablacije s mirnom kamerom (Sponza, Quality 1.5x)",
    ),
    # M6. The flow groups share no column with the upscaler groups: what is
    # measured here is the distance between the estimated field and the
    # rasteriser's own motion vectors. Those vectors are exact ground truth for
    # as long as the only thing moving is the camera, which on this scene is
    # always -- so the module gets a reference it never has in a real engine,
    # and the tables below are the closest thing to an absolute accuracy the
    # project can produce.
    "flow": (
        ["--validate-flow", "--jitter"],
        FLOW_COLUMNS,
        "Optical flow (M6): tocnost po razlucivosti i cijena",
    ),
    "flow-ablation": (
        ["--validate-flow", "--jitter"],
        FLOW_COLUMNS,
        "Optical flow: ablacije i pretrage parametara (Sponza, 60 fps orbita)",
    ),
    # Block matching has a hard reach: radius texels per level, doubled at each
    # of the levels above. Past it the estimator does not degrade gracefully,
    # it loses the block. One row per camera speed is the only honest way to
    # report where that boundary is on real content.
    "flow-speed": (
        ["--validate-flow", "--jitter"],
        FLOW_COLUMNS,
        "Optical flow: tocnost u ovisnosti o brzini kamere (Sponza, Quality 1.5x)",
    ),
}

FHD = ["--width", "1920", "--height", "1080"]
UHD = ["--width", "3840", "--height", "2160"]
# Sponza is loaded at radius 12 and the orbit is pulled in to radius 3 with a
# starting angle that keeps the camera looking down the arcade for the whole
# capture. The default orbit sits against a wall there, and a wall of blurry
# mip-mapped stone makes every upscaler look equally good (and equally
# pointless to measure).
SPONZA_ARGS = ["--scene", str(SPONZA), "--scene-fit", "12",
               "--path-radius", "3", "--path-phase", "-0.6"]

# (group, name, description, extra arguments). New upscalers/interpolators are
# added here as they land; the table shape does not change.
RUNS = [
    ("mv", "native-1080p", "Native 1920x1080", [*FHD, "--scale", "1.0"]),
    ("mv", "native-540p", "Native 960x540", ["--width", "960", "--height", "540", "--scale", "1.0"]),
    ("mv", "quality-1080p", "1080p, render 1.5x manji", [*FHD, "--scale", "1.5"]),
    ("mv", "performance-1080p", "1080p, render 2.0x manji", [*FHD, "--scale", "2.0"]),
    ("mv", "motion-slow", "960x540, sporo gibanje (dt/4)", ["--width", "960", "--height", "540", "--scale", "1.0", "--fixed-dt", "0.004167"]),
    ("mv", "motion-fast", "960x540, brzo gibanje (dt*2)", ["--width", "960", "--height", "540", "--scale", "1.0", "--fixed-dt", "0.033334"]),
    ("mv", "aliased", "960x540, bez filtriranja uzorka", ["--width", "960", "--height", "540", "--scale", "1.0", "--no-filter-pattern"]),
]

# What a frame costs with no upscaler at all. The upscale rows are only
# meaningful next to these: a spatial upscaler's cost is fixed at output
# resolution, so it pays for itself only when the render it replaces was
# expensive enough.
if SPONZA.exists():
    RUNS += [
        ("native", "cost-1080p-native", "Sponza: native 1920x1080", [*FHD, "--scale", "1.0", *SPONZA_ARGS]),
        ("native", "cost-1080p-q", "Sponza: samo render 1280x720", [*FHD, "--scale", "1.5", *SPONZA_ARGS]),
        ("native", "cost-1080p-p", "Sponza: samo render 960x540", [*FHD, "--scale", "2.0", *SPONZA_ARGS]),
        ("native", "cost-4k-native", "Sponza: native 3840x2160", [*UHD, "--scale", "1.0", *SPONZA_ARGS]),
        ("native", "cost-4k-p", "Sponza: samo render 1920x1080", [*UHD, "--scale", "2.0", *SPONZA_ARGS]),
    ]

# Every spatial baseline at both FSR preset scales, on both scenes. The
# reference is always the same instant rendered natively at 1080p.
for _scale, _label in (("1.5", "Quality 1.5x"), ("2.0", "Performance 2.0x")):
    for _mode, _name in (("nearest", "Nearest"), ("bilinear", "Bilinear"),
                         ("bicubic", "Bicubic"), ("fsr1", "FSR1 EASU+RCAS")):
        RUNS.append((
            "upscale", f"up-{_mode}-{_scale.replace('.', '')}",
            f"{_name}, {_label}",
            [*FHD, "--scale", _scale, "--upscaler", _mode],
        ))
        if SPONZA.exists():
            RUNS.append((
                "upscale", f"up-sponza-{_mode}-{_scale.replace('.', '')}",
                f"Sponza: {_name}, {_label}",
                [*FHD, "--scale", _scale, "--upscaler", _mode, *SPONZA_ARGS],
            ))

# How much fidelity RCAS costs. `--sharpness` is in stops of attenuation:
# 0 is maximum sharpening, and around 8 the negative lobe is scaled away to
# nothing, so that row is EASU on its own.
if SPONZA.exists():
    for _s in ("0", "0.25", "0.5", "1", "2", "4", "8"):
        _tag = "EASU bez RCAS-a" if _s == "8" else f"sharpness {_s}"
        RUNS.append((
            "rcas", f"rcas-{_s.replace('.', '')}", f"FSR1, {_tag}",
            [*FHD, "--scale", "1.5", "--upscaler", "fsr1", "--sharpness", _s, *SPONZA_ARGS],
        ))

# 4K is where the trade-off actually shows: the render cost scales with the
# input resolution, the upscaler's cost with the output.
if SPONZA.exists():
    for _mode, _name in (("nearest", "Nearest"), ("bilinear", "Bilinear"),
                         ("bicubic", "Bicubic"), ("fsr1", "FSR1 EASU+RCAS")):
        RUNS.append((
            "upscale", f"up4k-sponza-{_mode}-20",
            f"Sponza 4K: {_name}, Performance 2.0x",
            [*UHD, "--scale", "2.0", "--upscaler", _mode, *SPONZA_ARGS],
        ))


# M4. The temporal rows are the same two scenes and the same two presets as the
# spatial table, so the two can be read side by side.
for _scale, _label in (("1.5", "Quality 1.5x"), ("2.0", "Performance 2.0x")):
    for _mode, _name in (("taau", "TAAU"), ("taau-rcas", "TAAU + RCAS")):
        RUNS.append((
            "taau", f"up-{_mode}-{_scale.replace('.', '')}",
            f"{_name}, {_label}",
            [*FHD, "--scale", _scale, "--upscaler", _mode],
        ))
        if SPONZA.exists():
            RUNS.append((
                "taau", f"up-sponza-{_mode}-{_scale.replace('.', '')}",
                f"Sponza: {_name}, {_label}",
                [*FHD, "--scale", _scale, "--upscaler", _mode, *SPONZA_ARGS],
            ))

# One row per knob, everything else at its default, so each line of the table
# answers exactly one question. The camera is the same orbit as everywhere
# else; a temporal upscaler is only interesting in motion, and the still-camera
# numbers are reported separately in docs/TAAU.md.
if SPONZA.exists():
    _TAAU_BASE = [*FHD, "--scale", "1.5", "--upscaler", "taau", *SPONZA_ARGS]
    _ABLATIONS = [
        ("no-jitter", "bez jittera (nema sto akumulirati)", ["--no-jitter"]),
        *[(f"frames-{v}", f"akumulacija max {v} okvira", ["--taau-frames", v])
          for v in ("1", "2", "4", "8", "16", "32")],
        *[(f"gamma-{v.replace('.', '')}", f"clamp {v} sigma", ["--taau-gamma", v])
          for v in ("0.5", "1", "1.5", "2", "4")],
        *[(f"kernel-{v}", f"rekonstrukcija 1/sigma^2 = {v}", ["--taau-kernel", v])
          for v in ("2", "4", "8", "16")],
        *[(f"motion-{v.replace('.', '')}", f"skracivanje povijesti {v}/px", ["--taau-motion", v])
          for v in ("0", "0.4", "0.8", "1.6", "3.2")],
        *[(f"mip-{v.replace('.', '').replace('-', 'm')}", f"mip bias {v}", ["--mip-bias", v])
          for v in ("0", "-0.585", "-1.585")],
        # RCAS on top of TAAU, which is a different question from RCAS on top
        # of EASU: here the sharpener is putting back Nyquist that the history
        # resample provably removed, so the optimum sits several stops softer
        # than the one the "rcas" group finds for FSR1.
        *[(f"rcas-{v.replace('.', '')}", f"TAAU + RCAS, ostrina {v}",
           ["--upscaler", "taau-rcas", "--sharpness", v])
          for v in ("0.4", "0.8", "1.0", "1.2", "1.5", "2.0")],
    ]
    RUNS.append(("taau-ablation", "taau-default", "TAAU, sve zadano", _TAAU_BASE))
    for _tag, _desc, _args in _ABLATIONS:
        RUNS.append(("taau-ablation", f"taau-{_tag}", _desc, [*_TAAU_BASE, *_args]))

    # 1e-7 s is a frozen camera in everything but name: the scripted path is a
    # function of accumulated time, so a small enough step makes the motion
    # vectors vanish without adding a separate code path for "paused".
    for _dt, _fps in (("0.0000001", "mirna kamera"), ("0.0083333", "120 fps"),
                      ("0.0166667", "60 fps"), ("0.0333333", "30 fps")):
        for _mode, _name in (("bicubic", "bicubic"), ("taau", "TAAU"),
                             ("taau-rcas", "TAAU + RCAS")):
            # The spatial baseline is measured unjittered: jitter is a cost for
            # it (a sub-pixel offset it cannot undo) and only a benefit for the
            # accumulator, so leaving it on would flatter TAAU for free.
            _extra = [] if _mode != "bicubic" else ["--no-jitter"]
            RUNS.append((
                "taau-speed", f"speed-{_mode}-{_dt.replace('.', '')}",
                f"{_name}, {_fps}",
                [*FHD, "--scale", "1.5", "--upscaler", _mode, *SPONZA_ARGS,
                 "--fixed-dt", _dt, *_extra],
            ))


# M5. All five scaling presets, because "sva 4 rezima skaliranja rade" is the
# acceptance criterion -- plus NativeAA, where the upscaler is a pure temporal
# antialiaser and the ratio is 1:1.
_SCALES = (("1.0", "NativeAA 1.0x"), ("1.5", "Quality 1.5x"), ("1.7", "Balanced 1.7x"),
           ("2.0", "Performance 2.0x"), ("3.0", "Ultra Performance 3.0x"))
for _scale, _label in _SCALES:
    for _mode, _name in (("fsr", "FSR"), ("fsr-rcas", "FSR + RCAS")):
        if SPONZA.exists():
            RUNS.append((
                "fsr", f"up-sponza-{_mode}-{_scale.replace('.', '')}",
                f"Sponza: {_name}, {_label}",
                [*FHD, "--scale", _scale, "--upscaler", _mode, *SPONZA_ARGS],
            ))
if SPONZA.exists():
    for _mode, _name in (("fsr", "FSR"), ("fsr-rcas", "FSR + RCAS")):
        RUNS.append((
            "fsr", f"up-sponza-4k-{_mode}",
            f"Sponza 4K: {_name}, Performance 2.0x",
            [*UHD, "--scale", "2.0", "--upscaler", _mode, *SPONZA_ARGS],
        ))

# One row per mechanism, everything else at its default. The first three rows
# are the ones the milestone is about: each turns off exactly one of the things
# M5 added to M4.
if SPONZA.exists():
    _FSR_BASE = [*FHD, "--scale", "1.5", "--upscaler", "fsr", *SPONZA_ARGS]
    _FSR_ABLATIONS = [
        ("no-dilate", "bez dilatacije vektora", ["--fsr-dilate", "0"]),
        ("no-disocclusion", "bez depth clipa (samo clamp boje)", ["--fsr-disocclusion", "0"]),
        ("no-locks", "bez lockova", ["--fsr-lock-life", "0"]),
        *[(f"lock-relax-{v}", f"lock siri clamp {v}x", ["--fsr-lock-relax", v])
          for v in ("1", "2", "4", "8")],
        *[(f"lock-contrast-{v.replace('.', '')}", f"lock prag kontrasta {v}",
           ["--fsr-lock-contrast", v]) for v in ("0.2", "0.35", "0.5", "0.7")],
        *[(f"lock-life-{v}", f"lock traje {v} okvira", ["--fsr-lock-life", v])
          for v in ("2", "4", "8")],
        *[(f"lanczos-{v.replace('.', '')}", f"Lanczos u gibanju {v}", ["--fsr-lanczos", v])
          for v in ("0.75", "1.0", "1.25", "1.5")],
        *[(f"lanczos-still-{v.replace('.', '')}", f"Lanczos na miru {v}",
           ["--fsr-lanczos-still", v]) for v in ("1.0", "1.5", "2.0", "2.5")],
        *[(f"depth-{v.replace('.', '')}", f"tolerancija dubine {v}", ["--fsr-depth", v])
          for v in ("0.005", "0.02", "0.1")],
        *[(f"reactive-{v.replace('.', '')}", f"reactive maska {v}", ["--fsr-reactive", v])
          for v in ("0", "0.3", "1.0")],
    ]
    RUNS.append(("fsr-ablation", "fsr-default", "FSR, sve zadano", _FSR_BASE))
    for _tag, _desc, _args in _FSR_ABLATIONS:
        RUNS.append(("fsr-ablation", f"fsr-{_tag}", _desc, [*_FSR_BASE, *_args]))

    _STILL = ["--fixed-dt", "0.0000001"]
    _FSR_STILL_ABLATIONS = [
        ("default", "FSR, sve zadano", []),
        ("no-dilate", "bez dilatacije vektora", ["--fsr-dilate", "0"]),
        ("no-disocclusion", "bez depth clipa (samo clamp boje)", ["--fsr-disocclusion", "0"]),
        ("no-locks", "bez lockova", ["--fsr-lock-life", "0"]),
        *[(f"lock-relax-{v}", f"lock siri clamp {v}x", ["--fsr-lock-relax", v])
          for v in ("1", "2", "4", "8")],
        *[(f"lock-contrast-{v.replace('.', '')}", f"lock prag kontrasta {v}",
           ["--fsr-lock-contrast", v]) for v in ("0.2", "0.35", "0.5", "0.7")],
        *[(f"lanczos-still-{v.replace('.', '')}", f"Lanczos na miru {v}",
           ["--fsr-lanczos-still", v]) for v in ("1.0", "1.5", "2.0", "2.5")],
        ("taau", "M4 TAAU, za usporedbu", ["--upscaler", "taau"]),
    ]
    for _tag, _desc, _args in _FSR_STILL_ABLATIONS:
        RUNS.append(("fsr-ablation-still", f"fsrs-{_tag}", _desc,
                     [*_FSR_BASE, *_STILL, *_args]))

    # The same four camera speeds as the M4 table, with the M4 upscaler kept as
    # the baseline row: the point of M5 is the delta against it, not against a
    # spatial filter it already beat.
    for _dt, _fps in (("0.0000001", "mirna kamera"), ("0.0083333", "120 fps"),
                      ("0.0166667", "60 fps"), ("0.0333333", "30 fps")):
        for _mode, _name in (("taau", "TAAU (M4)"), ("fsr", "FSR (M5)"),
                             ("fsr-rcas", "FSR + RCAS")):
            RUNS.append((
                "fsr-speed", f"fspeed-{_mode}-{_dt.replace('.', '')}",
                f"{_name}, {_fps}",
                [*FHD, "--scale", "1.5", "--upscaler", _mode, *SPONZA_ARGS,
                 "--fixed-dt", _dt],
            ))

# M6. Every flow row is 1080p with FSR in the pipeline unless it says
# otherwise: the estimator runs on the presented image, so the upscaler is part
# of its input and measuring it on a natively rendered frame would measure a
# configuration the project does not ship.
if SPONZA.exists():
    _FLOW_BASE = [*FHD, "--scale", "1.5", "--upscaler", "fsr", *SPONZA_ARGS]

    RUNS += [
        ("flow", "flow-1080p-q", "1080p, render 1280x720, FSR", _FLOW_BASE),
        ("flow", "flow-1080p-native", "1080p native (bez upscalinga)",
         [*FHD, "--scale", "1.0", "--upscaler", "fsr", *SPONZA_ARGS]),
        ("flow", "flow-1080p-p", "1080p, render 960x540, FSR",
         [*FHD, "--scale", "2.0", "--upscaler", "fsr", *SPONZA_ARGS]),
        ("flow", "flow-720p-q", "1280x720, render 854x480, FSR",
         ["--width", "1280", "--height", "720", "--scale", "1.5",
          "--upscaler", "fsr", *SPONZA_ARGS]),
        ("flow", "flow-4k-p", "4K, render 1920x1080, FSR",
         [*UHD, "--scale", "2.0", "--upscaler", "fsr", *SPONZA_ARGS]),
    ]

    # One row per knob, everything else at its default, so each line answers
    # exactly one question. The three mechanism rows at the top are the ones
    # the milestone is about: each removes one of the three passes' reasons to
    # exist.
    _FLOW_ABLATIONS = [
        ("no-temporal", "bez kandidata iz proslog okvira", ["--of-temporal", "0"]),
        ("no-filter", "bez medijan filtra", ["--of-filter", "0"]),
        ("no-upscale", "bez izbora kandidata pri prosirenju", ["--of-upscale", "0"]),
        ("no-scene-change", "bez detekcije reza", ["--of-scene-change", "0"]),
        *[(f"levels-{v}", f"{v} razina piramide", ["--of-levels", v])
          for v in ("3", "4", "5", "6", "7")],
        *[(f"radius-{v}", f"radijus pretrage {v} texela", ["--of-radius", v])
          for v in ("2", "4", "6", "8")],
        *[(f"smooth-{v.replace('.', '')}", f"glatkoca {v}/texel", ["--of-smoothness", v])
          for v in ("0", "0.0002", "0.0005", "0.002", "0.01")],
        *[(f"novelty-{v.replace('.', '')}", f"novelty {v}", ["--of-novelty", v])
          for v in ("0", "0.0005", "0.001", "0.004", "0.01", "1")],
        *[(f"statistic-{v}", f"rez: statistika {n}", ["--of-scene-statistic", v])
          for v, n in (("0", "maksimum"), ("1", "srednja"), ("2", "medijan"))],
    ]
    RUNS.append(("flow-ablation", "flow-default", "Sve zadano", _FLOW_BASE))
    for _tag, _desc, _args in _FLOW_ABLATIONS:
        RUNS.append(("flow-ablation", f"flow-{_tag}", _desc, [*_FLOW_BASE, *_args]))

    # The window is fixed in scene time, not in frames. A fixed frame count at
    # three timesteps measures three different stretches of the camera path,
    # and since the path is not uniformly difficult -- one close pass past a
    # column dominates it -- that alone can make the slower rate look more
    # accurate. Half a second of warm-up, one second measured, at every rate.
    for _dt, _fps in (("0.0000001", "mirna kamera"), ("0.0083333", "120 fps"),
                      ("0.0166667", "60 fps"), ("0.0333333", "30 fps")):
        _step = float(_dt)
        _warm = 16 if _step < 1e-4 else round(0.5 / _step)
        _total = _warm + (60 if _step < 1e-4 else round(1.0 / _step))
        RUNS.append((
            "flow-speed", f"flow-speed-{_dt.replace('.', '')}", _fps,
            [*_FLOW_BASE, "--fixed-dt", _dt,
             "--warmup", str(_warm), "--frames", str(_total)],
        ))


def run_one(binary, group, name, extra, frames, out_dir):
    csv_path = out_dir / f"{name}.csv"
    # COMMON first so a run-specific --fixed-dt overrides the shared one:
    # later arguments win in the app's left-to-right parser. The shared
    # --frames sits with it for the same reason -- a row that fixes its window
    # in scene time needs a different frame count at every timestep.
    cmd = [str(binary), *COMMON, "--frames", str(frames),
           *GROUPS[group][0], *extra, "--csv", str(csv_path)]
    print(f"[run] {name}: {' '.join(cmd[1:])}", flush=True)
    proc = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"[run] {name} failed with exit code {proc.returncode}")
    if not csv_path.exists():
        raise SystemExit(f"[run] {name} produced no CSV at {csv_path}")
    return csv_path


# The aggregate is part of the key a result is stored under, so that one csv
# column can appear in a table three times (mean, median, p95) without the
# three overwriting each other.
def column_key(key, agg):
    return key if agg == "mean" else f"{key}_{agg}"


def percentile(values, q):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(q * len(ordered)))]


def reduce_column(values, agg):
    if agg == "mean":
        return statistics.fmean(values)
    if agg == "pct":  # a per-frame fraction, reported as a percentage
        return 100.0 * statistics.fmean(values)
    if agg == "median":
        return statistics.median(values)
    if agg == "p95":
        return percentile(values, 0.95)
    raise SystemExit(f"[agg] unknown aggregate '{agg}'")


def aggregate(csv_path, columns):
    with csv_path.open() as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit(f"[agg] {csv_path} has no data rows")
    out = {"frames": len(rows)}
    for key, _, agg in columns:
        values = [float(r[key]) for r in rows if r.get(key)]
        if not values:
            raise SystemExit(f"[agg] {csv_path} has no column '{key}'")
        out[column_key(key, agg)] = reduce_column(values, agg)
        if key.endswith("_ms"):
            out[key + "_p95"] = percentile(values, 0.95)
    out["fps"] = 1000.0 / out["gpu_ms"] if out["gpu_ms"] > 0 else float("inf")
    return out


def format_table(results, columns):
    header = ["Konfiguracija", "Frameovi", *[c[1] for c in columns], "GPU p95 (ms)", "FPS (GPU)"]
    lines = ["| " + " | ".join(header) + " |",
             "|" + "|".join(["---"] * len(header)) + "|"]
    for r in results:
        cells = [r["description"], str(r["frames"])]
        for key, _, agg in columns:
            value = r[column_key(key, agg)]
            if key.startswith("ssim"):
                cells.append(f"{value:.4f}")
            elif agg == "pct":
                cells.append(f"{value:.1f}")
            else:
                cells.append(f"{value:.2f}")
        cells.append(f"{r['gpu_ms_p95']:.3f}")
        cells.append(f"{r['fps']:.0f}")
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--out", default="captures/metrics")
    ap.add_argument("--frames", type=int, default=120)
    ap.add_argument("--only", nargs="*", default=None, help="run only these configuration names")
    ap.add_argument("--group", nargs="*", default=None, choices=list(GROUPS),
                    help="run only these groups")
    args = ap.parse_args()

    binary = REPO / args.build_dir / "fsr3lite"
    if not binary.exists():
        raise SystemExit(f"[run] no binary at {binary}; build first (cmake --build {args.build_dir})")

    out_dir = REPO / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    selected = [r for r in RUNS
                if (args.only is None or r[1] in args.only)
                and (args.group is None or r[0] in args.group)]
    if not selected:
        raise SystemExit(f"[run] no configuration matched {args.only or args.group}")

    selected_names = {r[1] for r in selected}
    for group, name, description, extra in selected:
        run_one(binary, group, name, extra, args.frames, out_dir)

    # The summary is rebuilt from every per-run CSV on disk, not only from the
    # configurations this invocation executed. Without that, `--group x`
    # silently replaces the whole table with one section: the runs it did not
    # repeat are still on disk, but they vanish from summary.md and the next
    # reader has no way to tell a missing group from a group that was never
    # measured. Re-aggregating is free -- the CSVs are already there.
    by_group = {}
    missing = []
    for group, name, description, extra in RUNS:
        path = out_dir / f"{name}.csv"
        if not path.exists():
            if name in selected_names:
                raise SystemExit(f"[agg] {name} was run but produced no CSV")
            missing.append(name)
            continue
        stats = aggregate(path, TIMING_COLUMNS + GROUPS[group][1])
        stats["name"] = name
        stats["description"] = description
        by_group.setdefault(group, []).append(stats)
    if missing:
        print(f"[agg] {len(missing)} configuration(s) never measured, omitted from the summary: "
              f"{', '.join(missing[:6])}{' ...' if len(missing) > 6 else ''}", flush=True)

    summary_csv = out_dir / "summary.csv"
    with summary_csv.open("w", newline="") as f:
        writer = csv.writer(f)
        # One flat CSV for plotting: the union of all metric columns, with
        # blanks where a group does not measure something.
        all_keys = []
        for group in by_group:
            for key, _, agg in TIMING_COLUMNS + GROUPS[group][1]:
                if column_key(key, agg) not in all_keys:
                    all_keys.append(column_key(key, agg))
        writer.writerow(["group", "config", "description", "frames", "gpu_ms_p95", "fps", *all_keys])
        for group, results in by_group.items():
            for r in results:
                writer.writerow([group, r["name"], r["description"], r["frames"],
                                 f"{r['gpu_ms_p95']:.3f}", f"{r['fps']:.1f}",
                                 *[f"{r[k]:.4f}" if k in r else "" for k in all_keys]])

    sections = []
    for group, results in by_group.items():
        columns = TIMING_COLUMNS + GROUPS[group][1]
        sections.append(f"## {GROUPS[group][2]}\n\n" + format_table(results, columns))
    document = "# Metrike\n\n" + "\n\n".join(sections) + "\n"

    summary_md = out_dir / "summary.md"
    summary_md.write_text(document)

    print()
    print(document)
    print(f"[out] {summary_csv.relative_to(REPO)}")
    print(f"[out] {summary_md.relative_to(REPO)}")


if __name__ == "__main__":
    main()
