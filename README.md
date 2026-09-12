# FSR3-lite

Temporalni upscaler i procjena gibanja iz slike, implementirani od nule u C++20
i OpenGL 4.6 compute shaderima. Radni dio diplomskog rada o kombiniranju
upscalinga i generiranja interpoliranih okvira (plan i opseg: [`PLAN.md`](PLAN.md)).

Ključ postavke je vlastiti renderer: on može renderirati i *ground truth* —
puni native okvir kao referencu za upscaler i pravi među-okvir na t−0.5·dt kao
referencu za interpolaciju. Zato je kvaliteta ovdje brojka (PSNR/SSIM naspram
reference), a ne dojam.

## Stanje

| | Modul | Status |
|---|---|---|
| M0–M1 | renderer: glTF, kamera, jitter (Halton 2,3), G-buffer, reverse-Z, motion vektori | ✅ |
| M2 | mjerni harness: skriptirana kamera, GPU timestamp queries, PSNR/SSIM na GPU-u | ✅ |
| M3 | prostorni baseline: nearest, bilinear, bicubic, FSR1 (EASU + RCAS) | ✅ |
| M4 | TAAU: reprojekcija + akumulacija + neighbourhood clamp | ✅ |
| M5 | puni upscaler: dilatacija vektora, depth disokluzija, Lanczos, lockovi, reactive | ✅ |
| M6 | optical flow: piramidalni block matching, filtriranje, detekcija promjene scene | ✅ |
| M7–M9 | generiranje okvira, frame pacing, naučeni modul | — |

Brojke i ablacije po modulu su u `docs/` (vidi niže); agregirane tablice u
`captures/metrics/summary.md`.

## Ovisnosti

- CMake ≥ 3.20, prevoditelj s C++20
- GPU i driver s OpenGL 4.6 core (razvijano na Radeon RX 580 / Mesa)
- SDL2 i libepoxy (traže se preko `pkg-config`)
- Python 3 + Pillow, samo za skripte u `scripts/`

Arch: `pacman -S cmake sdl2 libepoxy python-pillow` ·
Debian/Ubuntu: `apt install cmake libsdl2-dev libepoxy-dev python3-pil`

`third_party/` (stb_image, stb_image_write, cgltf) je u repozitoriju, ništa se
ne dovlači pri buildu.

## Build

```sh
scripts/fetch_assets.sh          # glTF scene — nisu u repozitoriju
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

Shaderi se čitaju s diska u runtimeu i hot-reloadaju se (provjera svakih 0.5 s),
pa izmjena u `shaders/` ne traži rebuild.

## Pokretanje

```sh
build/fsr3lite --scene assets/sponza/Sponza.gltf --upscaler fsr-rcas --scale 1.5
```

Bez `--scene` renderira se ugrađena proceduralna scena. `--scale N` je faktor
upscalinga: `1.0` NativeAA, `1.5` Quality (default), `1.7` Balanced, `2.0`
Performance, `3.0` Ultra Performance. Izlaz je `--width`×`--height`, po defaultu
1920×1080, pa render rezolucija izlazi iz faktora.

Režimi za `--upscaler`: `nearest`, `bilinear`, `bicubic`, `fsr1`, `taau`,
`taau-rcas`, `fsr`, `fsr-rcas`. Bez zastavice se G-buffer prikazuje direktno.
`--optical-flow` uključuje procjenu gibanja (po defaultu je isključena da ne
ulazi u mjerenja upscalera).

Tipke: `1`–`9` debug prikazi · `F1`–`F5` faktor skaliranja (NativeAA → Ultra
Performance) · `J` jitter ·
`P` skriptirana kamera · `F` pattern filter · `V` vsync · `SPACE` pauza ·
`F12` screenshot · `ESC` izlaz.

Nepoznata ili nepotpuna zastavica je greška, ne ignorira se — mjerenje koje
tiho ne mjeri ono što mu piše u naredbi je gore od pada.

## Mjerenja

```sh
python -m venv .venv && .venv/bin/pip install -r scripts/requirements.txt
.venv/bin/python scripts/run_metrics.py        # sve tablice, jednom naredbom
```

Svako pokretanje je deterministično (skriptirana kamera, fiksni timestep, fiksni
broj okvira), piše po-frame CSV i agregira ga u `captures/metrics/`. Ostale
skripte: `check_gt.py` (provjera ground-truth dumpa), `compare_upscalers.py`
(slika s uvećanim izrezima), `flow_debug.py` i `flow_cuts.py` (optical flow).

U repozitoriju su CSV-ovi i Markdown tablice iz `captures/metrics/`; renderirane
slike nisu — svaka skripta ih ponovno napravi.

## Dokumentacija

| | |
|---|---|
| [`PLAN.md`](PLAN.md) | opseg rada, arhitektura, milestones, rizici |
| [`docs/METRICS.md`](docs/METRICS.md) | kako se generira tablica metrika |
| [`docs/GROUND_TRUTH.md`](docs/GROUND_TRUTH.md) | kako se snima i provjerava referenca |
| [`docs/UPSCALING.md`](docs/UPSCALING.md) | M3: prostorni baseline |
| [`docs/TAAU.md`](docs/TAAU.md) | M4: minimalni temporalni upscaler |
| [`docs/FSR.md`](docs/FSR.md) | M5: puni upscaler |
| [`docs/OPTICALFLOW.md`](docs/OPTICALFLOW.md) | M6: procjena gibanja iz slike |

## Struktura

```
src/app/          prozor, ulaz, petlja, argumenti, mjerenje vremena
src/renderer/     glTF učitavanje, kamera + jitter, G-buffer
src/upscale/      Modul B — prostorni i temporalni upscaleri
src/opticalflow/  Modul C — piramidalni block matching
src/metrics/      PSNR/SSIM na GPU-u, dump okvira
src/gfx/          shaderi (s hot reloadom), teksture, GPU timeri
shaders/          GLSL 460 compute + fullscreen passevi
scripts/          mjerenja i provjere (Python)
third_party/      stb_image, stb_image_write, cgltf
```

## Tuđi rad

- `third_party/stb_image.h`, `stb_image_write.h` — Sean Barrett, MIT / public
  domain (licenca je u zaglavlju datoteke)
- `third_party/cgltf.h` — Johannes Kuhlmann i suradnici, MIT
- Scene koje dovodi `scripts/fetch_assets.sh` nisu u repozitoriju i dolaze iz
  Khronosovog [glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets).
  Uvjeti su njihovi, ne naši: **Sponza** © 2016 Crytek, CRYENGINE Limited
  License Agreement; **Damaged Helmet** © 2018 ctxwing, CC BY 4.0 (ranija
  verzija © 2016 theblueturtle_, CC BY-NC 4.0). Prije bilo kakve daljnje
  upotrebe scena provjeri uvjete u izvornom repozitoriju.
- AMD FidelityFX dokumentacija se čita kao referenca, vidi
  [`docs/reference/README.md`](docs/reference/README.md); passevi u `src/` i
  `shaders/` su implementirani iz opisa, a ne prepisani.
