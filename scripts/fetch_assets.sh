#!/usr/bin/env bash
# Dovodi glTF scene koje renderer i mjerenja koriste.
#
# Scene su tuđe (Khronos glTF-Sample-Assets) i zato nisu u repozitoriju: 54 MB
# binarnih podataka koje nismo mi napravili i nemamo ih zašto redistribuirati.
# Ova skripta ih stavlja točno tamo gdje ih kod očekuje:
#
#   assets/sponza/Sponza.gltf   — scena za sva mjerenja
#   assets/helmet.glb           — mala scena za brzu provjeru učitavanja
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
assets="$root/assets"
upstream="https://github.com/KhronosGroup/glTF-Sample-Assets.git"

if [[ -f "$assets/sponza/Sponza.gltf" && -f "$assets/helmet.glb" ]]; then
    echo "[assets] scene već postoje u $assets — ništa se ne preuzima"
    exit 0
fi

command -v git >/dev/null 2>&1 || {
    echo "[assets] git nije dostupan" >&2
    exit 1
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Sparse + blobless klon: repozitorij uzoraka je nekoliko GB, a trebaju nam
# dva modela, pa se ostalo nikad ne skida.
echo "[assets] preuzimanje iz KhronosGroup/glTF-Sample-Assets ..."
git clone --depth 1 --filter=blob:none --sparse "$upstream" "$tmp/gltf" >/dev/null
git -C "$tmp/gltf" sparse-checkout set \
    Models/Sponza/glTF \
    Models/DamagedHelmet/glTF-Binary >/dev/null

mkdir -p "$assets/sponza"
cp -r "$tmp/gltf/Models/Sponza/glTF/." "$assets/sponza/"
cp "$tmp/gltf/Models/DamagedHelmet/glTF-Binary/DamagedHelmet.glb" "$assets/helmet.glb"

echo "[assets] gotovo:"
echo "  $(du -sh "$assets/sponza" | cut -f1)	assets/sponza  ($(find "$assets/sponza" -type f | wc -l) datoteka)"
echo "  $(du -h "$assets/helmet.glb" | cut -f1)	assets/helmet.glb"
