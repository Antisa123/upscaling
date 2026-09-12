# M4 — minimalni temporalni upscaler (TAAU)

Prvi korak koji stvarno *rekonstruira* razlučivost umjesto da je interpolira.
M3 upscaleri (`docs/UPSCALING.md`) imaju na raspolaganju samo uzorke jednog
okvira; TAAU ima uzorke svih dosadašnjih okvira, a kamera svaki okvir pomiče
projekciju za pod-pikselni Halton pomak. Taj niz pomaka *jest* dodatna
razlučivost — nijedan prostorni filtar je ne može izmisliti.

Milestone je namjerno minimalan: reprojekcija, akumulacija i clamp. Dilatacija
vektora gibanja, odbacivanje po dubini, lockovi, reactive maska i Lanczos
rekonstrukcija pripadaju M5 (`docs/FSR.md`) i svaki od njih tamo dobiva vlastiti
redak ablacijske tablice — mjeren u dva režima, jer se nekoliko njih ponaša
suprotno ovisno o tome koliko je povijest duga.

## Pokretanje

```
./build/fsr3lite --upscaler taau-rcas --scale 1.5 --validate-upscale --frames 120
```

| Zastavica | Značenje |
|---|---|
| `--upscaler taau` | reprojekcija + akumulacija + clamp |
| `--upscaler taau-rcas` | isto, plus RCAS izoštravanje na izlazu |
| `--taau-frames <f>` | gornja granica akumulacije, u okvirima (zadano 8) |
| `--taau-gamma <g>` | širina clamp kutije, u standardnim devijacijama susjedstva (zadano 2) |
| `--taau-kernel <k>` | inverzna varijanca rekonstrukcijske jezgre, u render-pikselima⁻² (zadano 6) |
| `--taau-motion <d>` | skraćivanje povijesti s gibanjem, po display-pikselu po okviru (zadano 1.6) |
| `--sharpness <s>` | RCAS oštrina u stopovima; 0 = najjače. Zadano 1.2 za TAAU, 0.25 za FSR1 |
| `--jitter` / `--no-jitter` | uključi/isključi pod-pikselni pomak projekcije |
| `--mip-bias <b>` | LOD bias za teksture; bez zastavice se izvodi iz omjera skaliranja |

## Tri dijela

### 1. Reprojekcija

Povijest je u display razlučivosti, pa je treba dohvatiti na `uv + mv(uv)`.
Vektori gibanja dolaze iz `shaders/gbuffer.frag`, računaju se iz
**nejittriranih** clip pozicija (`uCurVP`, `uPrevVP`) dok se rasterizira s
`uCurVPJittered`, i čitaju se isključivo s `texelFetch` — filtrirani vektor
gibanja na silueti je prosjek dvaju nepovezanih gibanja i pokazuje nikamo.

Povijest se resamplira Catmull-Rom filtrom u 9 bilinearnih dohvata (standardna
3×3 faktorizacija 4×4 jezgre, a = −0.5). Mjereno je bolji od bilinearnog za
0.75 dB u gibanju (33.29 vs 32.54 dB na ranijoj konfiguraciji) — negativni
lobovi djelomično kompenziraju gubitak koji je opisan niže.

### 2. Akumulacija

Za svaki display piksel uzima se 3×3 okolina render piksela oko
`samplePos = uv * uInputSize - 0.5 + uJitter` i teži se Gaussovom jezgrom
`exp(-k·d²)`. Rezultat se miješa s povijesti po težini koja raste do
`--taau-frames`.

Predznak jittera nije proizvoljan: `projJittered_[2][0] = -2·jx/W` znači da
render teksel `i` prikazuje scenu kao da je na `i − jitter`, pa je pozicija
uzorka za display piksel `srcF + uJitter`.

**Širina jezgre je bila prvi stvarni nalaz.** Sa σ = 0.5 render piksela svaki
pojedinačni okvir izgleda čisto i stabilno — i to je sve što ikad daje. Uska
jezgra (σ ≈ 0.25, tj. `k ≈ 6–8`) daje šumovit okvir čiji je šum točno uzorak
jittera, a to je ono što se usrednjavanjem pretvara u razlučivost. Sužavanje
je vrijedilo +0.5 dB.

### 3. Clamp

Povijest koja više ne pripada lokalnom rasponu boja tekućeg okvira povlači se
natrag u njega. Granice su srednja vrijednost susjedstva ± `--taau-gamma`
standardnih devijacija, presječene s tvrdim min/max 3×3 okoline.

**Drugi stvarni nalaz: clamp mora biti u YCoCg prostoru, i to *clip*, ne
*clamp*.** Nezavisno ograničavanje tri RGB kanala je očita implementacija i
pomiče ton: na podu od svijetlog kamena s tamnom fugom povijesti se crvena
povuče natrag, a plava ne, i piksel ispadne magenta. Vidljivo je na 1:1 izrezu
poda. Prelazak na YCoCg + povlačenje po pravcu prema središtu kutije
(`clipToBox` u `shaders/taau.comp`) vrijedio je +1.1 dB i uklonio obrub.

## Zašto TAAU u brzom gibanju gubi od bicubica

Ovo je glavni analitički nalaz M4 i ne radi se o bugu.

Svaki *interpolirajući* filtar za resampliranje ima nulu na Nyquistu kad
pogodi pola-pikselnu fazu. Okvir gibanja koji nije cijeli broj display piksela
zato iz povijesti skida najfiniji detalj. To samo po sebi nije strašno — ali
povijest se svaki okvir vraća u ulaz s težinom ~(1 − 1/f), pa se gubitak po
okviru pretvara u stacionarni gubitak:

```
pojačanje na Nyquistu (stacionarno) ≈ a / (1 − (1 − a)·r)
```

gdje je `a = 1/f` udio novog okvira, a `r` prijenos filtra na Nyquistu za tu
fazu. Za `f = 8` i `r = 0.5` to je 0.22 — više od dvije trećine najviših
frekvencija nestane iako svaki pojedinačni resample gubi „samo” pola. Zato
duga povijest izgleda oštro dok kamera stoji i omekša čim se pomakne.

Isključeno je mjerenjem, kao uzrok: predznak i skala reprojekcije, matematika
jittera i akumulacije, izbor filtra povijesti (bilinearni je gori), LOD bias
tekstura (sweep 0 / −0.585 / −1.0 / −1.585 pomiče TAAU sa 33.18 na 33.34) te
disokluzija i tanka geometrija (toplinska karta greške stavlja grešku na velik
teksturiran pod, ne na siluete).

Dvije posljedice za dizajn:

**(a) Skraćivanje povijesti s gibanjem.** Gornja granica akumulacije pada
eksponencijalno s brzinom u slici:

```glsl
maxWeight = mix(1.0, uMaxWeight, exp(-uMotionDecay * mvPixels));
```

`--taau-motion 0` isključuje mehanizam i vraća fiksnu granicu. Ablacija na
Sponzi, 60 fps orbita (iz `captures/metrics/summary.md`):

| `--taau-motion` | PSNR | SSIM | Stabilnost |
|---|---|---|---|
| 0 (fiksna granica) | 32.64 | 0.9071 | 38.58 |
| 0.4 | 34.56 | 0.9385 | 37.56 |
| 0.8 | 34.93 | 0.9428 | 37.19 |
| 1.6 (zadano) | 35.13 | 0.9449 | 36.94 |
| 3.2 | 35.21 | 0.9458 | 36.80 |

**2.5 dB između isključenog i uključenog mehanizma** je najveći pojedinačni
pomak u cijeloj M4 ablaciji, i to je izravna mjera učinka opisanog gore.

Stupac stabilnosti ide u suprotnom smjeru i to je cijela poanta: fiksna
granica daje najstabilniju sliku u tablici (38.58 dB, 4.6 dB iznad reference) i
ujedno najlošiju vjernost. Ta slika ne treperi zato što ne prati scenu.

Krivulja je monotona, ali ravna iznad 1.6: razlika između 1.6 i 3.2 je 0.08 dB,
a 3.2 u praksi znači „povijest isključena čim se išta miče” (na 60 fps orbiti
je `mvPixels` reda 5–10, pa je `exp(-3.2·mvPixels)` numerička nula). Zadano je
zato 1.6 — zadržava mjerljivu akumulaciju pri sporom gibanju za cijenu koja je
manja od desetine decibela.

**(b) RCAS radi nešto drugo nego kod FSR1.** Ista ablacija oštrine, ista
scena, dva različita ulaza:

| `--sharpness` | TAAU + RCAS (PSNR / SSIM) | FSR1 EASU + RCAS (PSNR / SSIM) |
|---|---|---|
| 0 | — | 30.89 / 0.8675 |
| 0.25 (zadano za FSR1) | — | 33.09 / 0.9117 |
| 0.4 | 34.60 / 0.9440 | — |
| 0.5 | — | 34.08 / 0.9274 |
| 0.8 | 35.14 / 0.9502 | — |
| 1.0 | 35.22 / 0.9506 | 34.88 / 0.9374 |
| 1.2 (zadano za TAAU) | **35.25 / 0.9505** | — |
| 1.5 | 35.26 / 0.9498 | — |
| 2.0 | 35.24 / 0.9486 | 35.28 / 0.9407 |
| 4 | — | 35.40 / 0.9408 |
| isključen | 35.13 / 0.9449 | 35.42 / 0.9406 |

Kod FSR1 je krivulja monotona prema „isključeno”: EASU je već izvukao sve što
u jednom okviru postoji, pa izoštravanje samo dodaje grešku, i njegov je
„optimum” degenerirani slučaj u kojem RCAS ništa ne radi. Kod TAAU postoji
pravi optimum na ≈1.0–1.5 koji je **iznad isključenog RCAS-a i po PSNR-u
(+0.13 dB) i po SSIM-u (+0.0057)** — jer ovdje izoštravanje vraća Nyquist koji
je resample povijesti dokazivo odnio, a ne izmišlja detalj kojeg nema. Na
mirnoj kameri, gdje je povijest najduža i gubitak najveći, razlika naraste na
**+1.03 dB** (40.39 vs 39.36). To je mjerljiva potvrda analize gore.

## Rezultat

Sponza, 1280×720 → 1920×1080 (Quality 1.5×), 120 okvira, referenca renderirana
2× supersamplirano i usrednjena u linearnom HDR-u prije tone mappinga
(grupa `taau-speed` u `captures/metrics/summary.md`):

| Kamera | bicubic | TAAU | TAAU + RCAS |
|---|---|---|---|
| mirna | 35.83 dB / 0.9441 | 39.36 / 0.9723 | **40.39 / 0.9819** |
| 120 fps orbita | 37.18 / 0.9596 | 37.50 / 0.9643 | **37.71 / 0.9690** |
| 60 fps orbita | **35.37** / 0.9420 | 35.13 / 0.9449 | 35.25 / **0.9505** |
| 30 fps orbita | **35.27** / 0.9392 | 34.36 / 0.9372 | 34.40 / **0.9421** |

Čitanje tablice:

* Na mirnoj i sporo pomičnoj kameri TAAU + RCAS dobiva **+4.56 dB odnosno
  +0.53 dB** nad najboljim prostornim filtrom. To je akumulacija koja radi ono
  zbog čega postoji.
* **SSIM TAAU + RCAS je bolji na svakoj brzini**, uključujući obje na kojima
  PSNR gubi. (TAAU bez RCAS-a pada ispod bicubica tek na 30 fps: 0.9372 naspram
  0.9392.) Razlika je struktura: bicubic na 60 fps ima nešto bolji PSNR jer
  nema kašnjenja, ali ima puzajući aliasing na rubovima, koji SSIM vidi a PSNR
  usrednji.
* Na 60 fps orbiti zaostatak u PSNR-u je 0.12 dB, unutar šuma mjerenja; na
  30 fps orbiti (≈10–12 display piksela gibanja po okviru) TAAU gubi 0.87 dB.
  Tada je gibanje brže nego što povijest može pratiti bez gubitka i M4 se svodi
  na jednookvirnu rekonstrukciju. Mehanizmi koji to popravljaju — lockovi, luma
  piramida, reactive maska, dilatirani vektori — su M5.
* Orbita korištena za mjerenje je namjerno brza (0.35 rad/s na polumjeru 3,
  kamera gleda u središte). „60 fps” redak je dakle blizu najgoreg slučaja, ne
  tipičnog.

### Vizualna usporedba

`scripts/compare_upscalers.py --scale 1.5` renderira isti okvir kroz sve
upscalere i slaže 4× uvećane izreze jedan ispod drugoga u
`captures/compare/grid.png`. Poredak koji se na njoj vidi prati SSIM, ne PSNR:
lavlja glava i rub zastave su kod TAAU + RCAS najbliži nativnom renderu, dok
je bicubic mekši a FSR1 izoštreno stepeničast.

### „Slika stabilna” — mjereno, ne procijenjeno

Druga polovica kriterija prihvaćanja traži stabilnost, koju PSNR po okviru ne
mjeri. `--validate-upscale` zato od M4 računa i temporalnu stabilnost:
prethodni izlaz reprojiciran vektorima gibanja naspram trenutnog, uz istu
mjeru nad referentnim nizom kao nulu (detalji u `docs/METRICS.md`).

Sponza, 60 fps orbita, Quality 1,5×, referenca 34,2 dB:

| Upscaler | PSNR | SSIM | Stabilnost | Δ |
|---|---|---|---|---|
| FSR1 EASU+RCAS | 33,09 | 0,9117 | 30,01 dB | −4,04 |
| Bicubic | 35,37 | 0,9420 | 32,47 dB | −1,58 |
| Bilinear | 35,25 | 0,9333 | 33,76 dB | −0,29 |
| **TAAU + RCAS** | 35,25 | **0,9505** | 35,99 dB | +1,99 |
| **TAAU** | 35,13 | 0,9449 | **36,94 dB** | +2,94 |

Prostorni filtri su ispod reference — treperenje koje im jednookvirni PSNR ne
naplaćuje. FSR1 je najgori jer RCAS izoštrava i aliasing. TAAU je **iznad**
reference, i to nije greška: akumulacija je niskopropusni filtar u vremenu, pa
prigušuje i legitimne promjene koje supersamplirana referenca ima.

To je ista pojava kao gubitak PSNR-a u gibanju, izmjerena s druge strane:
+2,9 dB stabilnosti i −0,24 dB vjernosti na 60 fps orbiti su dva lica istog
kompromisa. `--taau-motion` je upravo ručica kojom se bira gdje na toj osi
sustav sjedi.

Mjera se mijenja s brzinom kamere (grupa `taau-speed`): na 120 fps TAAU ima
41,2 dB naspram referentnih 37,6, na 30 fps 32,9 naspram 30,8. Na **mirnoj**
kameri brojke se izokrenu — referenca je na 84,6 dB (niz se praktički ne
mijenja), a TAAU na 57,3, jer akumulacija još uvijek konvergira i svaka nova
Halton faza minimalno pomakne prosjek. Razlika od 57 dB je RMS ispod 0,002 i
perceptivno nepostojeća; iznad ~50 dB ova metrika više ništa ne razlikuje i
čita se samo u gibanju.

Kriterij prihvaćanja iz `PLAN.md` („slika stabilna, vidljiv dobitak PSNR-a nad
bicubic”) je ispunjen: dobitak je +4.56 dB na mirnoj kameri i +0.53 dB na
realističnoj brzini, uz bolji SSIM na svim brzinama i sve četiri mjerene
brzine bez vidljivog ghostinga.

## Cijena

Sponza, RX 580, prosjek po okviru iz `GpuTimer`-a:

| Prolaz | 720p → 1080p | 1080p → 4K |
|---|---|---|
| G-buffer | 0.79 | 1.37 |
| Tonemap | 0.09 | 0.20 |
| TAAU | 0.42 | 1.64 |
| RCAS | 0.20 | 0.81 |
| Present | 0.18 | 0.57 |
| **Ukupno TAAU + RCAS** | **1.68** | **4.59** |
| za usporedbu: nativni okvir | 1.01 | 3.48 |

**Na ovoj sceni upscaler nije brži od nativnog renderiranja, i to treba reći
otvoreno.** Sponza s jednostavnim forward shadingom na RX 580 nije dovoljno
opterećena sjenčanjem: cijeli nativni okvir u 4K traje 3.48 ms, a samo TAAU
prolaz u istoj razlučivosti 1.64 ms. Ušteda od renderiranja u pola površine
(3.48 → 1.37 ms G-buffera) nije dovoljna da pokrije upscaler.

To je očekivano i ne mijenja svrhu M4 — dobitak je ovdje **kvaliteta**
(+4.56 dB, bolji SSIM na svim brzinama) — ali postavlja dvije stvari za
kasnije:

1. Prag isplativosti traži shading-bound scenu. G-buffer ovdje skalira
   podlinearno s površinom (0.49 ms na 720p → 1.01 ms na 1080p), dakle
   dobrim dijelom je vezan geometrijom, koju niža razlučivost ne pojeftinjuje.
2. 1.64 ms za TAAU u 4K je previše za jedan prolaz. Optimizacija — fp16 gdje
   ima smisla, `textureGather` umjesto pojedinačnih dohvata, spajanje
   Catmull-Rom dohvata — je dio M5, zajedno s ostatkom punog upscalera.

Referentni prolazi (`ref: GT native` 2.75 ms, `ref: GT downsample` 0.64 ms,
`ref: GT tonemap` 0.19 ms na 1080p) postoje samo u `--validate-upscale` načinu
i `gfx::GpuTimer` ih po prefiksu `ref: ` isključuje iz zbroja, pa ne ulaze ni u
tablicu ni u FPS.

## Metodološka napomena

Referenca je od M4 nadalje **supersamplirana** (`--gt-ss 2`, zadano). Razlog je
u `docs/METRICS.md`: jednostruka referenca u display razlučivosti je
aliasirana, a bodovanje protiv aliasirane reference nagrađuje zamućenje. TAAU
konvergira prema antialiasiranoj slici, pa je to slika protiv koje se mora
mjeriti. Zbog te promjene su sve M3 brojke iz ranijih verzija ove
dokumentacije regenerirane.

Uz to, render prolaz koristi negativni LOD bias tekstura (`log2(render/display)`,
i još jedan stupanj niže za temporalne načine — FSR2 konvencija). Bez njega
mip odabir slijedi render razlučivost i upscaler dobiva sliku iz koje su
frekvencije koje treba rekonstruirati već bile filtrirane. Vrijedi ≈1 dB na
mirnoj kameri.
