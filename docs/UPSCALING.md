# M3 — prostorni upscaleri (baseline)

Prva stepenica prema FSR3: tri referentna prostorna upscalera i FSR1
(EASU + RCAS), bez ikakve vremenske komponente. Svrha nije da budu dobri,
nego da postave brojke ispod kojih TAAU (M4) i puni upscaler (M5) ne smiju
pasti, i da se na njima vidi zašto je vremenska komponenta uopće potrebna.

## Pokretanje

```
./build/fsr3lite --upscaler fsr1 --scale 1.5 --validate-upscale --frames 120
```

| Zastavica | Značenje |
|---|---|
| `--upscaler <nearest\|bilinear\|bicubic\|fsr1>` | koji upscaler ide između G-buffera i ekrana (temporalni načini `taau`, `taau-rcas` su u `docs/TAAU.md`) |
| `--sharpness <f>` | RCAS oštrina u stopovima; 0 = najjače, ≈8 = RCAS praktički isključen. Bez zastavice: 0.25 za FSR1, 1.2 za TAAU (`docs/TAAU.md`) |
| `--validate-upscale` | svaki okvir renderira i nativnu referencu u punoj razlučivosti i mjeri PSNR/SSIM |
| `--path-radius`, `--path-phase` | polumjer i početni kut skriptirane orbite, u jedinicama scene |

`--validate-upscale` piše `psnr_upscale,ssim_upscale` u CSV (`--csv`) i ispisuje
sažetak na kraju pokretanja.

## Prostor boja: zašto tone mapping ide *prije* upscalera

FSR1 je definiran nad display-referred vrijednostima u `[0, 1]`. RCAS-ov
limiter računa koliko "prostora" ima do bijele pretpostavljajući da je vrh
skale 1.0, a EASU-ova detekcija rubova pretpostavlja perceptivni razmak
između vrijednosti. Renderer radi u linearnom HDR-u, pa bi izravno
propuštanje G-buffera kroz EASU dalo pogrešne smjerove rubova i RCAS koji
ne oštri ništa.

Zato je lanac:

```
linearni HDR (G-buffer)
  -> tonemap.comp          (isti metricsToDisplay iz metrics_common.glsl)
  -> upscaler              (EASU -> RCAS, ili resample)
  -> present.frag, uToneMap = 0
```

Nusprodukt je da je izlaz upscalera već u prostoru u kojem se PSNR i SSIM
konvencionalno izvještavaju, pa se referenca tone-mapa istim shaderom i
usporedba ide s `tonemap = false`. Ništa se ne tone-mapa dvaput.

Iz istog razloga `--upscaler` gasi jitter: čisto prostorni upscaler nema
povijest u koju bi se sub-pikselni pomak akumulirao, pa bi jitter bio samo
šum na ulazu.

## Usporedna slika

```
scripts/compare_upscalers.py --scale 2.0 --frame 80 --crop 0.47 0.28 0.11 0.11 --zoom 5
```

Renderira isti okvir kroz sva četiri upscalera plus nativni render i slaže
uvećane izreze jedan ispod drugoga u `captures/compare/grid.png`. Tablica
kaže koji je filtar bliži referenci; ova slika kaže *kako* griješi.

## Scena za mjerenje

Sponza se učitava s `--scene-fit 12`, a orbita se steže na `--path-radius 3`
uz `--path-phase -0.6`. Zadana orbita polumjera 5 na toj skali završi
priljubljena uz zid, a zid zamućenog mipmapiranog kamena čini svaki upscaler
jednako dobrim (i jednako besmislenim za mjeriti): PSNR tamo skače na 42–49 dB
i razlike se izgube. S ovim kadrom kamera cijelo vrijeme gleda niz arkadu —
tanka geometrija, stupovi, lanci, natpisi — što je sadržaj na kojem se
rekonstrukcija ruba uopće vidi.

## Implementacije

Sve tri su vlastite izvedbe iz objavljenog opisa algoritma, ne prijepis
`ffx_fsr1.h` — v. pravilo u `PLAN.md` ("referentne implementacije: čitaju se,
ne kopiraju").

**`upscale_resample.comp`** — nearest (`texelFetch`), bilinear (hardverski
filtar) i bicubic. Bicubic je Catmull-Rom (Mitchell-Netravali s B=0, C=1/2),
16 tapova, interpolirajući i blago izoštravajući. Njegov overshoot na rubu
je upravo artefakt zbog kojeg EASU postoji.

**`fsr1_easu.comp`** — 12 tapova (4×4 bez kutova). Četiri preklapajuća
5-tap križa, težinska po bilinearnim težinama kvadranta, daju smjer i
jačinu ruba. Kernel se zatim rotira u smjer ruba i rasteže okomito na njega;
prozorska funkcija ga ograničava, a deringing ga na kraju stegne u raspon
četiri najbliža tapa.

**`fsr1_rcas.comp`** — 3×3 križ, po kanalu min/max prstena, negativna
latica ograničena preostalim prostorom do crne i do bijele, skalirana s
`exp2(-sharpness)`, i tvrdi limit `0.25 - 1/16`.

## Rezultati

Puna tablica: `captures/metrics/summary.md` (`scripts/run_metrics.py
--group upscale rcas`). Mjereno na RX 580 (Mesa), 120 frameova, skriptirana
kamera, fiksni korak, bez jittera.

> Brojke u ovom dokumentu su regenerirane nakon M4. Referenca je od tada
> **supersamplirana** (`--gt-ss 2`) i render prolaz koristi negativni LOD bias
> tekstura; oboje je objašnjeno u `docs/METRICS.md`. Apsolutne vrijednosti su
> zato više nego u ranijim verzijama, a poredak filtara se ponegdje promijenio
> — prvenstveno zato što aliasirana referenca sustavno nagrađuje zamućenje.

### Sponza, 1080p

| Upscaler | Quality 1,5× PSNR / SSIM | Performance 2,0× PSNR / SSIM |
|---|---|---|
| Nearest | 32,15 dB / 0,8966 | 31,59 dB / 0,8794 |
| Bilinear | 35,25 dB / 0,9333 | 33,11 dB / 0,8895 |
| Bicubic | 35,37 dB / 0,9420 | 33,13 dB / 0,8997 |
| FSR1 (EASU+RCAS, `--sharpness 0.25`) | 33,09 dB / 0,9117 | 31,92 dB / 0,8785 |
| **EASU sam** (`--sharpness 8`) | **35,42 dB / 0,9406** | — |

Sponza u 4K (Performance 2,0×): nearest 33,81 / 0,9193, bilinear 35,55 /
0,9333, bicubic **35,71 / 0,9416**, FSR1 34,30 / 0,9232.

### Što se iz toga čita

**EASU sam je najbolji prostorni filtar po PSNR-u, bicubic po SSIM-u.**
S isključenim RCAS-om EASU daje 35,42 dB naspram bicubicovih 35,37, ali
bicubic ima bolji SSIM (0,9420 naspram 0,9406). Razlika je unutar šuma i
poštenije ju je čitati kao „izjednačeno na sceni koja EASU-u ne ide na ruku”:
Sponza ima mipmapirane teksture, dakle malo aliasinga za popraviti, a
rekonstrukcija ruba po smjeru plaća se tamo gdje ruba nema. Bicubic je zato u
`docs/TAAU.md` uzet kao prostorni baseline za M4.

**RCAS košta vjernost, monotono.** Ablacija oštrine (grupa `rcas` u tablici,
Sponza Quality 1,5×):

| `--sharpness` | PSNR | SSIM |
|---|---|---|
| 0 (najjače) | 30,89 dB | 0,8675 |
| 0,25 (zadano za FSR1) | 33,09 dB | 0,9117 |
| 0,5 | 34,08 dB | 0,9274 |
| 1 | 34,88 dB | 0,9374 |
| 2 | 35,28 dB | 0,9407 |
| 4 | 35,40 dB | 0,9408 |
| 8 (RCAS praktički isključen) | 35,42 dB | 0,9406 |

I PSNR i SSIM rastu monotono kako se oštrenje gasi, dakle optimum je
degenerirani slučaj u kojem RCAS ne radi ništa: EASU je već izvukao sve što u
jednom okviru postoji. Oštrina je ovdje perceptivni parametar, ne parametar
vjernosti, i u radu je tako treba i izvještavati — s ablacijom, ne s jednom
brojkom.

Suprotan rezultat vrijedi za RCAS nad TAAU izlazom, gdje postoji pravi
optimum iznad isključenog filtra; usporedba i objašnjenje su u
`docs/TAAU.md`.

**PSNR i oko se ne slažu.** Na proceduralnoj sceni (namjerno aliasirana, bez
mipmapa) bilinear ima najbolji PSNR od sva četiri filtra (31,11 dB naspram
29,98 dB za FSR1 pri 1,5×). To nije znak da je bilinear bolji, nego da PSNR
nagrađuje zamućivanje. (Taj je efekt od M4 djelomično uklonjen
supersampliranom referencom — vidi `docs/METRICS.md` — ali ne nestaje: filtar
koji zamuti aliasirani ulaz i dalje ispadne bliži u srednjoj kvadratnoj
pogrešci od filtra koji ga izoštri.) Usporedna slika
(`scripts/compare_upscalers.py`, izlaz `captures/compare/grid.png`) pokazuje
suprotan poredak od PSNR-a: nearest stepeničasti rub, bilinear zamućen,
bicubic oštriji ali sa stepenicama na luku, FSR1 najbliži nativnom renderu.

### Cijena

| Pass | 1080p izlaz | 4K izlaz |
|---|---|---|
| Nativni render (cijeli okvir) | 1,01 ms | 3,48 ms |
| G-buffer na 1/2 površine | 0,38 ms | 1,19 ms |
| Tone mapping | 0,09 ms | 0,20 ms |
| EASU | 0,33 ms | 1,29 ms |
| RCAS | 0,20 ms | 0,81 ms |
| Present | 0,19 ms | 0,57 ms |
| **Ukupno FSR1 pri 2,0×** | **1,14 ms** | **3,98 ms** |

Ovo je neugodan, ali iskren rezultat: na ovoj sceni **FSR1 ne ubrzava ništa**.
Cijena upscalera je fiksna i vezana za *izlaznu* razlučivost, a ušteda je
vezana za *ulaznu*. Sponza se na RX 580 renderira u ~1 ms pri 1080p, pa
ušteda od 0,63 ms ne može platiti 0,52 ms EASU+RCAS-a plus tone mapping i
skuplji Present. Bilinear pri 2,0× jest brži od nativnog (0,62 ms naspram
1,01 ms), jer gotovo ništa ne košta.

Dva zaključka za rad:

1. Mjerenje ubrzanja traži scenu čija je cijena renderiranja usporediva s
   pravim opterećenjem, a ne demo koja se vrti na 1000 FPS. Ovo je stvarno
   ograničenje mjerne postavke i tako će biti i navedeno.
2. Naša EASU izvedba (1,29 ms pri 4K na RX 580) je u fp32, skalarna, s 12
   `texelFetch` poziva. Referentna koristi pakirani fp16 i `gather4`. To je
   konkretan cilj optimizacije za M5, ne nedostatak algoritma.

### Zašto ovo motivira M4

Nijedan prostorni filtar ne može rekonstruirati informaciju koje u ulaznom
okviru nema — razlika između njih je samo koju vrstu pogreške rade. Tek
akumulacija kroz vrijeme donosi nove uzorke, i zato FSR2/FSR3 nisu prostorni
upscaleri. Brojke iz ove tablice su donja granica koju TAAU (M4) mora
nadmašiti.
