#!/usr/bin/env bash
# M9: loss x seed sweep for the c8-c16 model on all 18 training paths. The
# first round showed that one training run says little: networks of 4, 8 and
# 12 base channels landed 0.26 dB apart on the same view, more than the gain
# being measured. Models go to captures/ml/exp/ and are compared by
# scripts/ml_select.py on full frames of the validation paths.
set -euo pipefail
cd "$(dirname "$0")/.."
D=captures/ml/data
E=captures/ml/exp
mkdir -p "$E"
ALL=$(ls $D/sponza-*.bin $D/proc-*.bin | paste -sd,)
VAL=$(ls $D/val-*.bin | paste -sd,)
for loss in mse charbonnier; do
    for seed in 1 2 3; do
        name="c8-c16-$loss-s$seed"
        [[ -f "$E/$name.bin" ]] && continue
        build/fg_train train --data "$ALL" --val "$VAL" --c0 8 --c1 16 --steps 12000 --batch 16 \
            --lr 0.002 --seed "$seed" --loss "$loss" --out "$E/$name.bin" --log "$E/$name.csv" \
            > "$E/$name.txt" 2>&1
        grep -E "val  12000" "$E/$name.txt" | sed "s/^/$name /"
    done
done
