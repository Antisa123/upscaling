# Metrički harness (M2)

## Jedna naredba

```sh
cmake --build build
scripts/run_metrics.py --frames 120
```

Ispisuje Markdown tablicu na stdout i zapisuje:

- `captures/metrics/summary.csv` — agregirani redak po konfiguraciji (za grafove)
- `captures/metrics/summary.md` — ista tablica za lijepljenje u rad
- `captures/metrics/<konfiguracija>.csv` — po-frame redci za svaku konfiguraciju

Konfiguracije su popisane u `RUNS` u `scripts/run_metrics.py`. Novi upscaler ili
interpolator dodaje se kao jedan redak u toj listi; oblik tablice se ne mijenja.

Korisne opcije: `--only native-1080p quality-1080p` (podskup konfiguracija),
`--group upscale`, `--frames N`, `--build-dir <dir>`, `--out <dir>`.

## Grupe

Svaki redak pripada grupi. Grupa određuje što se validira, a time i koje
stupce po-frame CSV uopće ima, pa se ne mora pretvarati da retci koji mjere
motion vektore i retci koji mjere upscalere dijele istu tablicu.

| Grupa | Dodaje | Mjeri |
|---|---|---|
| `mv` | `--validate-mv` | točnost motion vektora (reprojekcija vs. bez nje) |
| `upscale` | `--validate-upscale` | upscalirani okvir vs. nativni render iste razlučivosti |
| `rcas` | `--validate-upscale` | ablacija RCAS oštrine nad FSR1 izlazom |
| `native` | — | samo cijena okvira bez upscalera, kao referenca za ubrzanje |
| `taau` | `--validate-upscale --jitter` | temporalni upscaler, iste scene i presetovi kao `upscale` |
| `taau-ablation` | `--validate-upscale --jitter` | jedan redak po TAAU parametru |
| `taau-speed` | `--validate-upscale --jitter` | isti upscaleri kroz četiri brzine kamere |
| `fsr` | `--validate-upscale --jitter` | puni upscaler (M5) kroz svih pet režima skaliranja |
| `fsr-ablation` | `--validate-upscale --jitter` | jedan redak po M5 mehanizmu, 60 fps orbita |
| `fsr-ablation-still` | `--validate-upscale --jitter` | iste ablacije s mirnom kamerom |
| `fsr-speed` | `--validate-upscale --jitter` | M4 i M5 kroz četiri brzine kamere |
| `flow` | `--validate-flow --jitter` | točnost optical flowa po razlučivosti (M6) |
| `flow-ablation` | `--validate-flow --jitter` | jedan redak po parametru procjenitelja |
| `flow-speed` | `--validate-flow --jitter` | isti procjenitelj kroz četiri brzine kamere |

Dvije ablacijske grupe za M5 nisu redundantne: dilatacija i lockovi štite
povijest, a 60 fps orbita je skrati na približno jedan okvir. Mjereno samo
tamo, dilatacija bi ispala vrijedna 0.19 dB umjesto 2.50.

`summary.md` sadrži po jednu tablicu za svaku grupu; `summary.csv` je jedan
ravni zapis s unijom stupaca (prazno gdje grupa nešto ne mjeri).

Stupac tablice je `(stupac CSV-a, naslov, agregat)`, gdje je agregat `mean`,
`median`, `p95` ili `pct` (udio po okviru izražen u postotku). Isti CSV stupac
smije se pojaviti više puta s različitim agregatom — `flow` grupe to i rade, jer
je raspodjela pogreške procjene gibanja teškorepa: šačica okvira u kojima slika
klizi stotinama piksela određuje srednju vrijednost cijele snimke, pa bi ona
sama opisivala te okvire, a ne ostalih sto.

Flow grupe fiksiraju **prozor u vremenu scene**, ne u broju okvira: `flow-speed`
računa `--warmup` i `--frames` iz vremenskog koraka (pola sekunde zagrijavanja,
jedna sekunda mjerenja pri svakoj brzini). Fiksni broj okvira pri tri koraka
mjeri tri različita komada putanje, a putanja nije ravnomjerno teška — to je
samo po sebi dovoljno da sporija brzina izgleda točnije.

Temporalne grupe vraćaju jitter natrag zastavicom `--jitter`, jer je
pod-pikselni pomak po okviru jedino čime akumulacija raspolaže; bez njega TAAU
degenerira u prostorni filtar primijenjen više puta. Prostorni baselineovi
unutar tih grupa (npr. bicubic u `taau-speed`) ostaju nejittrirani — za njih je
jitter čista cijena koju ne mogu poništiti, pa bi ih uključivanje besplatno
prikazalo lošijima.

## Determinizam

Svaka konfiguracija se pokreće sa `--scripted --fixed-dt 0.016667 --no-jitter`,
dakle ista putanja kamere, isti vremenski korak i isti broj frameova. Sve što se
razlikuje između redaka tablice mora biti u `RUNS`, ne u `COMMON`.

## Referenca je supersamplirana

`--gt-ss N` (zadano 2) renderira referencu u N× display razlučivosti i
usrednjuje je **u linearnom HDR-u prije tone mappinga** — to je redoslijed u
kojem SSAA renderer razlučuje i jedini radiometrijski ispravan.

Nije riječ o dotjerivanju nego o preduvjetu da metrika išta znači za temporalni
upscaler. Jednostruka referenca u display razlučivosti je **aliasirana**, a
bodovanje protiv aliasirane reference nagrađuje zamućenje: što god
rekonstrukcija napravi s rubom, neće imati referentne stepenice, pa je mutniji
kandidat bliže njihovom prosjeku. TAAU konvergira prema antialiasiranoj slici,
pa je to slika protiv koje ga treba mjeriti. `--gt-ss 1` reproducira staro,
aliasirano ponašanje.

Zbog te promjene su sve M3 brojke iz ranijih verzija dokumentacije
regenerirane; tablice u `docs/UPSCALING.md` i `docs/TAAU.md` odgovaraju
trenutnom `captures/metrics/summary.md`.

## LOD bias tekstura

Odabir mipmape vodi **render** razlučivost, pa 720p prolaz koji hrani 1080p
izlaz bira mipove ograničene na 720p i upscaler dobiva sliku iz koje su
frekvencije koje treba rekonstruirati već bile filtrirane. FSR1 zato traži
`log2(render/display)`, a FSR2 još jedan stupanj niže.

Oboje se izvodi automatski iz omjera skaliranja i načina rada; `--mip-bias <b>`
gazi izvedenu vrijednost, što je ono što ablacijski redci trebaju. Referentni
render ostaje na 0 — on je već u vlastitoj nativnoj razlučivosti i biasiranje
bi pomaknulo metu, a ne kandidata.

## Zagrijavanje

Prvih 16 frameova (`--warmup N`) se ne zapisuje u CSV. Dva razloga:

1. Prsten timestamp upita treba tri framea prije nego dade ikakvu brojku, a
   reprojekcija treba prethodni frame.
2. Prvi frameovi nose kompilaciju shadera, stvaranje pipeline objekata i prve
   uploade tekstura. Na RX 580 je prvi frame reda 5–7 ms naspram 0,3 ms u
   stacionarnom stanju.

Zato `--frames 140` daje 124 retka.

Iz istog razloga `gpu_ms` u CSV-u **nije** izglađena vrijednost. `totalMs()` je
eksponencijalni prosjek (čitljiv u naslovu prozora, beskoristan kao mjerni niz:
jedan skok pri zagrijavanju ostaje vidljiv desecima frameova i tiho pomiče
svaku statistiku). CSV piše `lastTotalMs()`, dakle stvarni zbroj za taj frame.

## Passevi koji se mjere, ali se ne broje

`--validate-upscale` renderira i nativnu referencu u punoj razlučivosti, što
ciljani pipeline nikad ne radi. Takvi passevi se imenuju prefiksom `ref: ` i
`GpuTimer::totalMs()` ih preskače: pojavljuju se u ispisu po passevima, ali ne
ulaze u cijenu okvira.

## Što se mjeri

| Stupac | Značenje |
|---|---|
| `cpu_ms` | vrijeme CPU framea (performance counter) |
| `gpu_ms` | zbroj GPU passeva tog framea iz timestamp upita (bez `ref:` passeva) |
| `gpu_ms_p95` | 95. percentil — hvata zastoje koje srednja vrijednost sakrije |
| `psnr_reproj`, `ssim_reproj` | prethodni frame reprojiciran motion vektorima vs. trenutni |
| `psnr_direct`, `ssim_direct` | prethodni frame bez reprojekcije vs. trenutni (baseline) |
| `coverage` | udio piksela čija reprojekcija pada unutar ekrana |
| `psnr_upscale`, `ssim_upscale` | upscalirani okvir vs. isti trenutak renderiran nativno u punoj razlučivosti |
| `tstab_upscale` | temporalna stabilnost izlaza: prethodni izlaz reprojiciran vektorima gibanja vs. trenutni, u dB |
| `tstab_ref` | ista mjera nad referentnim nizom — nula za usporedbu |
| `epe_px` | pogreška procijenjenog toka naspram vektora iz rasterizacije, u prikaznim pikselima |
| `epe_within1`, `epe_within2` | udio blokova unutar 1 odnosno 2 piksela |
| `scene_change` | statistika udaljenosti histograma koju je presuda o rezu koristila |
| `sc_max`, `sc_mean`, `sc_median` | sve tri statistike nad devet sekcija, svaki okvir |
| `cut` | 1 ondje gdje je `--cut-every` forsirao rez (poravnato s dva okvira kašnjenja readbacka) |

Razlika `psnr_reproj - psnr_direct` je test ispravnosti motion vektora: mora
rasti s brzinom gibanja. Konfiguracije `motion-slow` / `motion-fast` postoje
upravo da se ta monotonost vidi u tablici.

## Temporalna stabilnost

PSNR i SSIM mjere koliko je **jedan okvir** blizu reference. Ne kažu ništa o
tome puzi li niz okvira, a puzajući rub je artefakt zbog kojeg temporalni
upscaler uopće postoji. `tstab_upscale` popunjava tu rupu: prethodni izlaz se
reprojicira vektorima gibanja trenutnog okvira i mjeri se koliko se još uvijek
razlikuje. Sve što gibanje objašnjava se poništi; ostatak je treperenje.

Brojka ima smisla samo uz istu mjeru nad **referentnim** nizom (`tstab_ref`),
koja nije nula: promjene sjenčanja, disokluzije i vlastito resampliranje
reference prežive reprojekciju. Čitanje je zato relativno:

* `tstab ≈ tstab_ref` — kandidat je stabilan koliko sadržaj dopušta;
* `tstab ≪ tstab_ref` — kandidat treperi (prostorni filtri, pogotovo izoštreni);
* `tstab ≫ tstab_ref` — kandidat je *previše* stabilan, tj. temporalno kasni.

Mjereno na Sponzi, 60 fps orbita, Quality 1,5× (referenca 34,2 dB):

| Upscaler | Stabilnost | Δ prema referenci |
|---|---|---|
| FSR1 EASU+RCAS | 30,01 dB | −4,04 |
| Bicubic | 32,47 dB | −1,58 |
| Bilinear | 33,76 dB | −0,29 |
| TAAU + RCAS | 35,99 dB | +1,99 |
| TAAU | 36,94 dB | +2,94 |

Bilinear je gotovo „stabilan” samo zato što je zamućen — zato se ova metrika
nikad ne čita bez PSNR-a i SSIM-a uz nju.

Iznad ~50 dB metrika prestaje razlikovati bilo što (RMS razlika pada ispod
0,002), pa se na mirnoj kameri ne čita — tamo su i referenca i kandidati u
rasponu u kojem je razlika perceptivno nepostojeća.

Vektori gibanja su u render razlučivosti i filtriraju se pri dohvatu, što je
pogrešno na siluetama — ali jednako pogrešno za svakog kandidata, pa usporedba
ostaje poštena.

## Ostale mjerne skripte

| Skripta | Što radi |
|---|---|
| `scripts/compare_upscalers.py` | figura s uvećanim izrezom, jedan red po upscaleru |
| `scripts/check_gt.py` | provjera snimljene reference (`docs/GROUND_TRUTH.md`) |
| `scripts/flow_cuts.py` | razdvajanje rezova od brzog gibanja, tri orbite × tri statistike |
| `scripts/flow_debug.py` | M6 figura prihvaćanja: tok, referenca i pogreška podudaranja |

Sve pišu u `captures/`, sve su bez vanjskih ovisnosti osim Pillowa ondje gdje
sastavljaju sliku.

## Implementacija

- `src/metrics/image_metrics.cpp` — MSE/MAE/PSNR/maxError/coverage i SSIM.
  Redukcija je dvostupanjska: 16×16 workgrupa piše parcijalne sume u SSBO,
  CPU ih zbraja. Atomici u floatu bi ovdje izgubili preciznost.
- SSIM je po Wang et al. 2004: 11×11 Gaussov prozor σ=1.5 nad lumom u display
  prostoru, C1=0.0001, C2=0.0009, separabilno u dva prolaza.
- `--validate-mv` uključuje SSIM (tri dodatna passa) i band-limita proceduralni
  uzorak, jer bi inače mjerio aliasing sadržaja umjesto točnosti vektora.
  `--no-filter-pattern` to gazi ako se aliasing baš želi mjeriti.
