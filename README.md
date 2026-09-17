# FSR3-lite

Temporalni upscaler i procjena gibanja iz slike, implementirani od nule u C++20
i OpenGL 4.6 compute shaderima. Radni dio završnog rada o kombiniranju
upscalinga i generiranja interpoliranih okvira (plan i opseg: [`PLAN.md`](PLAN.md)).

Ključ postavke je vlastiti renderer: on može renderirati i *ground truth* —
puni native okvir kao referencu za upscaler i pravi među-okvir na t−0.5·dt kao
referencu za interpolaciju. Zato je kvaliteta ovdje brojka (PSNR/SSIM naspram
reference), a ne dojam.

## Demo

**[antisa123.github.io/upscaling/demo](https://antisa123.github.io/upscaling/demo/)**
— render rezolucija naspram upscaled izlaza naspram native reference, na istom
okviru (Sponza, FSR + RCAS, Quality 1.5×: 1280×720 → 1920×1080):

- tri-way pregled istog okvira (sirovi render / upscaled / native), s
  metrikama uz svaku varijantu
- uvećan izrezak (5×) kroz svih šest upscalera jedan pored drugog, za razliku
  koju brojka ne pokaže
- grafovi PSNR/SSIM/FPS po upscaleru i trošak renderiranja po rezoluciji, iz
  stvarnih mjerenja u `captures/metrics/summary.md`

Stranica je statičan `demo/index.html`; slike su snimljene binarnim izlazom
projekta preko `scripts/compare_upscalers.py` (vidi [Mjerenja](#mjerenja)).

## Stanje

| | Modul | Status |
|---|---|---|
| M0–M1 | renderer: glTF, kamera, jitter (Halton 2,3), G-buffer, reverse-Z, motion vektori | ✅ |
| M2 | mjerni harness: skriptirana kamera, GPU timestamp queries, PSNR/SSIM na GPU-u | ✅ |
| M3 | prostorni baseline: nearest, bilinear, bicubic, FSR1 (EASU + RCAS) | ✅ |
| M4 | TAAU: reprojekcija + akumulacija + neighbourhood clamp | ✅ |
| M5 | puni upscaler: dilatacija vektora, depth disokluzija, Lanczos, lockovi, reactive | ✅ |
| M6 | optical flow: piramidalni block matching, filtriranje, detekcija promjene scene | ✅ |
| M7 | generiranje okvira: warp, disokluzija, mješavina | ✅ |
| M8 | inpainting, UI kompozicija, frame pacing, latencija | ✅ |
| M9 | naučena mješavina (mala mreža u compute shaderima, vlastiti trener) | ✅ |

Svi moduli su gotovi. Povijest odluka i rezultata po modulu:
[`docs/NASTAVAK.md`](docs/NASTAVAK.md).

Brojke i ablacije po modulu su u `docs/` (vidi niže); agregirane tablice u
`captures/metrics/summary.md`.

## Ovisnosti

- CMake ≥ 3.20, prevoditelj s C++20
- GPU i driver s OpenGL 4.6 core (testirano na NVIDIA RTX 5070
  i AMD RX 7800 XT)
- SDL2 i libepoxy (traže se preko `pkg-config`)
- Python 3 + Pillow, samo za skripte u `scripts/`

Instalacija (MSYS2, [msys2.org](https://www.msys2.org)) — u terminalu
"MSYS2 MinGW x64":

```sh
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-SDL2 mingw-w64-x86_64-libepoxy \
    mingw-w64-x86_64-python mingw-w64-x86_64-python-pillow
```

`third_party/` (stb_image, stb_image_write, cgltf) je u repozitoriju, ništa se
ne dovlači pri buildu.

## Build

Svaki novi MSYS2 MinGW x64 terminal prvo treba alate na `PATH`:

```sh
export PATH="/c/msys64/mingw64/bin:$PATH"

bash scripts/fetch_assets.sh      # glTF scene — nisu u repozitoriju
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

Shaderi se čitaju s diska u runtimeu i hot-reloadaju se (provjera svakih 0.5 s),
pa izmjena u `shaders/` ne traži rebuild.

## Pokretanje

```sh
build/fsr3lite.exe --scene assets/sponza/Sponza.gltf --upscaler fsr-rcas --scale 1.5
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
export PATH="/c/msys64/mingw64/bin:$PATH"
export PYTHONUTF8=1                            # ispravno kodiranje izlaza
python3 scripts/run_metrics.py                 # sve tablice, jednom naredbom
```

`run_metrics.py` nema ovisnosti izvan standardne biblioteke; Pillow (iz
paketa gore) treba tek skriptama koje slažu slike (`ml_gallery.py`,
`make_font_atlas.py`).

Svako pokretanje je deterministično (skriptirana kamera, fiksni timestep, fiksni
broj okvira), piše po-frame CSV i agregira ga u `captures/metrics/`. Ostale
skripte: `check_gt.py` (provjera ground-truth dumpa), `compare_upscalers.py`
(slika s uvećanim izrezima), `flow_debug.py` i `flow_cuts.py` (optical flow).

U repozitoriju su CSV-ovi i Markdown tablice iz `captures/metrics/`; renderirane
slike nisu — svaka skripta ih ponovno napravi.

`captures/metrics/` je s NVIDIA RTX 5070; `captures/metrics-rx7800xt/` je isto
mjerenje s AMD RX 7800 XT (Sapphire Nitro+, uz AMD Ryzen 7 5700X3D) na
drugom stroju. Tablice u `docs/` koje uspoređuju kartice to i navode.

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
| [`docs/FRAMEGEN.md`](docs/FRAMEGEN.md) | M7: generiranje okvira |
| [`docs/PACING.md`](docs/PACING.md) | M8: inpainting, UI, frame pacing, latencija |
| [`docs/ML.md`](docs/ML.md) | M9: naučena mješavina |
| [`docs/ML_KOMERCIJALNO.md`](docs/ML_KOMERCIJALNO.md) | poglavlje 3: komercijalna i istraživačka rješenja |
| [`docs/NASTAVAK.md`](docs/NASTAVAK.md) | povijest odluka i rezultata po modulu |

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
