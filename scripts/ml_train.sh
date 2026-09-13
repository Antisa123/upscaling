#!/usr/bin/env bash
# M9: trains the blend models compared in docs/ML.md from the captures of
# scripts/ml_dataset.py. CPU only (tools/fg_train, OpenMP); a c8-c16 model
# takes about eight minutes on 12 threads.
#
#   scripts/ml_train.sh            all models
#   scripts/ml_train.sh NAME       one of them
#
#   blend-c8-c16          the reported model, all training paths (chosen from
#                         the seeds of scripts/ml_sweep.sh by ml_select.py and
#                         copied here; an existing file is never retrained)
#   blend-c4-c8           half the width
#   blend-c12-c24         one and a half times the width
#   blend-c8-c16-sponza   Sponza paths only: the procedural scene becomes an
#                         unseen scene, which is the generalisation test
set -euo pipefail
cd "$(dirname "$0")/.."
D=captures/ml/data
W=captures/ml/weights
L=captures/ml/logs
mkdir -p "$W" "$L"
ALL=$(ls $D/sponza-*.bin $D/proc-*.bin | paste -sd,)
SPONZA=$(ls $D/sponza-*.bin | paste -sd,)
VAL=$(ls $D/val-*.bin | paste -sd,)
ONLY="${1:-}"

run() {
    local name=$1
    shift
    if [[ -n "$ONLY" && "$ONLY" != "$name" ]]; then return; fi
    if [[ -f "$W/$name.bin" ]]; then echo "$name: exists, skipped"; return; fi
    build/fg_train train --out "$W/$name.bin" --log "$L/$name.csv" --steps 12000 --batch 16 \
        --lr 0.002 --seed 1 --loss charbonnier "$@" 2>&1 | tee "$L/$name.txt"
}

run blend-c8-c16 --data "$ALL" --val "$VAL" --c0 8 --c1 16
run blend-c4-c8 --data "$ALL" --val "$VAL" --c0 4 --c1 8
run blend-c12-c24 --data "$ALL" --val "$VAL" --c0 12 --c1 24
run blend-c8-c16-sponza --data "$SPONZA" --val "$D/val-sponza-r4-p1.3-30.bin" --c0 8 --c1 16
