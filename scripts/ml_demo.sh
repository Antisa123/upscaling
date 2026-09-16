#!/usr/bin/env bash
# M9 demonstration on a small example: the whole learned-blend loop in a few
# minutes on one machine -- capture a training set from the renderer, train a
# small network on the CPU, check that the shader computes what the trainer
# trained, and run the renderer with it.
#
# Deliberately small: one training path and one check path at 720p, a
# c4-c8 network (1 875 parameters), 1 500 steps. It shows the method, not the
# result; the measured models and their tables are in docs/ML.md.
#
#   scripts/ml_demo.sh            everything, then prints the interactive command
set -euo pipefail
cd "$(dirname "$0")/.."

OUT=captures/ml/demo
mkdir -p "$OUT"
SCENE=(--scene assets/sponza/Sponza.gltf --scene-fit 12)
[[ -f assets/sponza/Sponza.gltf ]] || SCENE=()
RUN=(build/fsr3lite --width 1280 --height 720 --scale 1.5 --upscaler fsr --scripted --jitter
     --validate-fg --warmup 16 --fixed-dt 0.0333333 --frames 56 "${SCENE[@]}")

echo "== 1/4 capture: training path and check path (renderer, lockstep)"
"${RUN[@]}" --path-radius 3 --path-phase 2.5 --ml-dump "$OUT/train.bin" --ml-patches 8 | grep interpolated
"${RUN[@]}" --path-radius 4 --path-phase 1.3 --ml-dump "$OUT/check.bin" --ml-patches 8 | grep interpolated

echo "== 2/4 train: c4-c8, 1500 steps, CPU"
build/fg_train train --data "$OUT/train.bin" --val "$OUT/check.bin" --c0 4 --c1 8 --steps 1500 \
    --batch 16 --eval-every 500 --out "$OUT/demo.bin" | grep -E "network c0|val|saved"

echo "== 3/4 shader vs trainer: the renderer runs the network and records its own output"
"${RUN[@]}" --path-radius 4 --path-phase 1.3 --fg-ml "$OUT/demo.bin" --ml-dump "$OUT/verify.bin" \
    --ml-patches 8 | grep interpolated
build/fg_train eval --weights "$OUT/demo.bin" --data "$OUT/verify.bin" --margin 28 | tail -2

echo "== 4/4 interactive: M switches heuristic/learned, 0 cycles views 9-11 (11 = weights), H hud"
echo "build/fsr3lite --width 1280 --height 720 --scale 1.5 --upscaler fsr --fg --fg-ml $OUT/demo.bin ${SCENE[*]} --path-radius 3 --path-phase -0.6"
