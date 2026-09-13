# M9 — naučena mješavina za generiranje okvira

Tema traži i naučenu komponentu. Pravi FSR3 je u cijelosti klasičan računalni
vid; naučeni pristupi (DLSS, FSR4 „Redstone“) žive na hardveru s matričnim
jedinicama, kojih RX 580 nema. M9 zato ne zamjenjuje jezgru nego mjeri jedno
izolirano pitanje: **može li mala mreža, uz iste ulaze, bolje od ručne
heuristike odlučiti kako spojiti kandidate koje klasični lanac već proizvodi —
i koliko to košta na GPU-u bez podrške za tenzore.**

Opcija (a) iz `PLAN.md` (Modul E): blend/inpainting maska, ne naučeni
upsampling.

<!-- REZULTATI: sažetak se upisuje nakon mjerenja -->

## Pokretanje

```
./build/fsr3lite --upscaler fsr --scale 1.5 --fg --fg-ml captures/ml/weights/blend-c8-c16.bin \
    --scene assets/sponza/Sponza.gltf --scene-fit 12 --path-radius 3 --path-phase -0.6
```

| Zastavica | Značenje |
|---|---|
| `--fg-ml <težine>` | prolazi 10–12: naučena mješavina zamjenjuje izlaz heuristike |
| `--ml-dump <datoteka>` | uz `--validate-fg`: snimanje skupa za učenje (patchevi značajki, kandidata i referentnog okvira) |
| `--ml-patches <n>` / `--ml-patch-size <px>` | patcheva po okviru (zadano 8, skripta koristi 4) / veličina (96) |
| `--ml-seed <n>` | sjeme izbora patcheva |
| `--time-offset <s>` | početno vrijeme scene, da snimka pokrije drugi dio animacije od mjernog |

## Što mreža radi — i što ne radi

Mreža **ne crta piksele**. Za svaki piksel predviđa softmax težine nad sedam
boja kandidata, a izlaz je njihov ponderirani zbroj:

| # | Kandidat |
|---|---|
| 0 | izlaz heuristike (prolazi 7–9, s inpaintingom) |
| 1, 2 | game vektor: warp iz prethodnog / iz trenutnog okvira |
| 3, 4 | optical flow vektor: warp iz prethodnog / iz trenutnog okvira |
| 5, 6 | trenutni / prethodni okvir bez warpa |

Tri su razloga za takav oblik:

1. **Čista usporedba.** Ulazi i kandidati su isti kao heuristici; naučeno je
   samo pravilo miješanja. Razlika u PSNR-u je razlika pravila, ne dodatne
   informacije.
2. **Ne može halucinirati.** Izlaz je konveksna kombinacija postojećih boja,
   pa mreža ne treba kapacitet za sintezu detalja — zato stane u nekoliko
   tisuća parametara i u compute shader.
3. **Počinje od heuristike.** Pristranost zadnjeg sloja inicijalizirana je
   tako da kandidat 0 dobije ~90 % težine; svaki odmak od klasičnog rezultata
   učenje mora zaraditi na podacima.

Polje kojem nedostaje vektor u svoje mjesto stavlja boju heuristike, pa izbor
„praznog“ kandidata ne košta ništa.

### Ulazi: 20 značajki po pikselu

Računa ih `shaders/fg_ml_common.glsl` — ista funkcija u prolazu značajki i u
prolazu mješavine, pa kandidati koje je mreža naučila vagati jesu, po
konstrukciji, oni koje vaga pri izvođenju.

| Sloj | Značajke |
|---|---|
| 0 | valjanost game polja, valjanost flow polja, maska okluzije prema t−1, prema t |
| 1 | je li uzorak na ekranu, za sva četiri warpa |
| 2 | slaganje boja para game warpova, para flow warpova, game naspram flow, trenutni naspram prethodnog |
| 3 | udaljenost heuristike od game warpa, od flow warpa; pokrivenost heuristike; razina piramide game vektora |
| 4 | razina piramide flow vektora; duljina game vektora; razlika game i flow vektora; luminancija |

Udaljenosti boja idu kroz korijen (zanimljive su male razlike), duljine
vektora kroz `log2(1 + px) / 8`. Normalizacija (srednja vrijednost i skala po
značajci, iz skupa za učenje) upisuje se u datoteku težina i pri učitavanju
utapa u prvi sloj, pa shader nema dodatnog posla.

### Arhitektura (U-Net-lite)

| Sloj | Operacija | Rezolucija (1080p) | Kanali |
|---|---|---|---|
| enc0 | 1×1 | 1920×1080 | 20 → c0 |
| enc1 | 2×2 prosjek, 3×3 | 960×540 | c0 → c1 |
| enc2 | 2×2 prosjek, 3×3 | 480×270 | c1 → c1 |
| enc3 | 3×3 | 480×270 | c1 → c1 |
| dec1 | bilinearno ×2 + enc1, 3×3 | 960×540 | c1 → c0 |
| dec0 | bilinearno ×2 + enc0, 1×1, softmax, mješavina | 1920×1080 | c0 → 7 |

ReLU iza svakog sloja osim zadnjeg. Padding 3×3 konvolucije replicira rub
(clamp koordinate), pooling je 2×2 prosjek, a povećanje bilinearno s centrima
na pola teksela i rubom na clamp — ono što izračuna jedan GL_LINEAR dohvat.
Trener implementira iste tri operacije.

Kanali žive četiri po RGBA16F sloju teksturnog polja, a težine su `mat4`
(`shaders/ml_conv.comp`): 3×3 konvolucija 16 → 16 je 4 × 9 × 4 = 144 umnoška
`mat4 · vec4` po pikselu — oblik posla u kojem su i GPU-ovi bez matričnih
jedinica još dobri. Veličine ulaze kao `#define`, pa svaka petlja ima
konstantnu granicu.

Raspored prolaza nije raspored slojeva, i razlika je izmjerena (1080p, cijena
prolaza na GPU-u):

- **enc0 se računa u prolazu značajki.** Kao zaseban prolaz čitao je natrag 20
  kanala pune rezolucije koje je prethodni prolaz upravo napisao; značajke se
  sad pišu samo pri snimanju skupa podataka.
- **Pooling i zbroj povećanja sa skip vezom su zasebni, jeftini prolazi**
  (`shaders/ml_resample.comp`), a konvolucije čitaju teksele izravno. Unutar
  petlje po tapovima koštali su filtrirani dohvat po tapu i ulaznom sloju:
  enc2 (pooling u petlji) trošio je 0,52 ms, a enc3 — ista konvolucija na istoj
  rezoluciji s izravnim čitanjem — 0,14 ms.
- **Zadnji 1×1 sloj računa se u prolazu mješavine**: njegov izlaz troši samo
  softmax odmah iza, pa bi zaseban prolaz pisao teksturu pune rezolucije ni za
  što.

Te tri promjene spustile su cijenu naučenog dijela s 4,18 na 2,75 ms bez ikakve
promjene funkcije: usporedba shadera s trenerom (niže) dala je iste brojeve
prije i poslije.

## Skup podataka

`scripts/ml_dataset.py` pokreće renderer u lockstep načinu s `--validate-fg
--ml-dump`. Svaki mjereni okvir daje patcheve 96×96 značajki i kandidata,
zajedno sa **stvarno renderiranim međuokvirom** kao oznakom (isti referentni
render kao u `docs/FRAMEGEN.md`: supersampliran 2×2, bez jittera).

- **Izbor patcheva:** 4 po okviru, pola jednoliko nasumično, pola vučeno
  razmjerno pogrešci heuristike. Samo jednoliko bi uglavnom bio statični zid
  na kojem je svaki kandidat jednako dobar; samo po pogrešci mreža nikad ne bi
  vidjela lak piksel.
- **Mreža patcheva poravnata na 4 piksela**, jer mreža dvaput poolira: patch
  mora poolirati na istoj mreži kao cijeli okvir da bi izlaz trenera bio
  usporediv s izlazom shadera.
- **Veličina okvira u zaglavlju datoteke** (format v2), da trener zna koje
  stranice patcha su rub samog okvira (vidi „Prva greška“ niže).
- **Podjela po putanji kamere, ne po okviru.** Uzastopni okviri iste putanje
  gotovo su ista slika; nasumična podjela po okvirima ocjenjivala bi mrežu na
  okvirima koje je efektivno već vidjela. Mjerni pogledi iz
  `scripts/run_metrics.py` (Sponza, radijus 3, faza −0,6; proceduralna scena od
  t = 0, faza 0) nikad se ne snimaju: kut orbite je `faza + 0,35 t`, i nijedna
  putanja za učenje ne ulazi u mjereni raspon kutova.
- **Raznolikost:** 18 putanja za učenje — Sponza na radijusima 2–5 i
  različitim fazama, 20/30/60/120 fps, Quality, Performance i native, te
  proceduralna scena u pet drugih dijelova animacije (t = 10–45 s). Četiri od
  njih (tri na 20 fps, jedna na 30 fps) dodane su **nakon prvog mjerenja na
  mjernim pogledima**, na kojem je prvi model gubio na 20 i 30 fps, a skup je
  imao samo dvije takve putanje od četrnaest. To je odluka donesena gledanjem
  testnih brojeva i navodi se kao takva.
- **Validacija:** dvije snimljene putanje (Sponza radijus 4, 30 fps;
  proceduralna scena na t = 80 s) služe treneru za izbor koraka; za izbor
  između modela `scripts/ml_select.py` uz njih mjeri još dvije putanje na
  20 fps, na cijelim okvirima.

Poznato ograničenje: Sponza je približno zrcalno simetrična, pa putanja za
učenje može gledati „zrcalni“ dio atrija mjernog pogleda (drugačije osvjetljen i
s drugim detaljima). Zato postoji i model učen **samo na Sponzi**, za koji je
proceduralna scena potpuno neviđena.

Jedna snimka (proceduralna scena, t = 60 s, 20 fps) izbačena je i zamijenjena
(t = 45 s): cijeli okviri bili su NaN. Uzrok je bio u rendereru, ne u M9: na
prvom okviru „prethodna“ kamera je jedinična matrica (ishodište, pogled prema
−z), pa za pod y = 0 vrijedi w = 0 duž z = 0. Jedan jitterirani uzorak pogodio
je točno tu liniju i `gbuffer.frag` je dijeljenjem `xy / w` upisao
(−inf, 0/0) u motion vektor; množenje NaN · 0 u resetu povijesti
`fsr_accumulate.comp` ne poništava, a Catmull-Rom uzorkovanje povijesti ga je
širilo 4 → 25 → 60 piksela po okviru dok cijeli okvir nije bio NaN. Popravak je
zaštitni pojas u `gbuffer.frag`: točka čija bi projekcija na prethodni okvir
bila izvan ±10⁴ poluekrana (iza ili na prethodnoj ravnini oka) dobiva vektor
jasno izvan ekrana, što svi potrošači već čitaju kao „nema povijesti“. Pojas je
namjerno širok: prva verzija na near ravnini mijenjala je rezultate Sponze od
okvira 51 (kamera prolazi kroz geometriju i valjano projicira točke tik unutar
near ravnine); ovakva ostavlja standardna mjerenja bit-identičnima (provjereno
na `fg-1080p-q` i `fsr-default`, sve kolone kvalitete). Trener uz to odbacuje
svaki patch s NaN/inf vrijednošću.

## Učenje

Vlastiti trener, `tools/fg_train` (C++20, OpenMP, samo CPU), umjesto
PyTorcha: RX 580 nema ROCm, projekt nema Python ovisnosti osim Pillowa, a
model je dovoljno malen da su unaprijedni i povratni prolaz pet operacija
nekoliko stotina linija petlji koje prevoditelj vektorizira. Posjedovanje obje
strane omogućuje i da se provjere jedna naspram druge (niže).

| Postavka | Vrijednost |
|---|---|
| gubitak | na RGB-u; unutar 16 px od ruba patcha se ne broji (mreža tamo vidi replicirani padding umjesto stvarnog konteksta), **osim na stranici koja je rub okvira** |
| optimizator | Adam, lr 2·10⁻³, 300 koraka zagrijavanja, kosinusni pad do 2 % |
| batch / koraci | 16 / 12 000 |
| augmentacija | svih 8 diedralnih transformacija (sve značajke su magnitude ili zastavice, pa su sve jednako valjane) |
| izbor modela | korak s najmanjom MSE na validacijskim putanjama |
| trajanje | ~8 min za c8-c16 na 12 dretvi |

`fg_train gradcheck` uspoređuje analitički gradijent s centralnim razlikama na
nasumičnim podacima: najveća relativna pogreška po sloju 1,2·10⁻³ – 4,9·10⁻²
(ReLU pregibi), nijedan uzorkovani parametar iznad 5 %.

### Prva greška: rub okvira

Prva serija modela učena je s maskom od 16 px na **svim** stranicama patcha.
Na validacijskim patchevima izgledala je dobro (+0,14 do +0,27 dB), a na
cijelim okvirima validacijske putanje Sponze (radijus 4, 30 fps) dva modela
različite širine (c4-c8 i c12-c24) izgubila su **točno jednako, −4,08 dB**
(43,06 naspram 47,14 dB). Karta razlike naspram heuristike bila je crna
svuda osim trake od 20–45 px uz lijevi i desni rub i uz donji rub okvira.

Uzrok: piksel uz rub okvira uvijek je unutar 16 px od ruba svog patcha, pa ga
gubitak nikad nije brojao. Upravo tu kretanje kamere otkriva scenu koju nijedan
od dva okvira ne sadrži, značajke izlaze iz raspona viđenog u učenju, a softmax
se zasiti na istom (pogrešnom) kandidatu — zato su dva različita modela dala
bit-identičan PSNR. Na rubu okvira replicirani padding trenera točno je ono što
shader radi, pa tamo maska nije potrebna; nakon ispravka skup je ponovno snimljen
i svi modeli ponovno učeni. Modeli prve serije sačuvani su u
`captures/ml/logs/prefix/`.

Pouka za mjerenje: validacija na patchevima nije vidjela ništa od ovoga. Izbor
između modela (gubitak, sjeme, podaci) zato se radi skriptom
`scripts/ml_select.py` na **cijelim okvirima** validacijskih putanja, a mjerni
pogledi iz `run_metrics.py` ocjenjuju se jednom, izabranim modelom.

## Provjera: shader računa istu funkciju kao trener

Snimka napravljena **s učitanom mrežom** (`--fg-ml … --ml-dump …`) u svakom
patchu nosi i izlaz shadera. `fg_train eval` na njoj izvodi vlastiti
unaprijedni prolaz i uspoređuje: na unutarnjem dijelu (rub 28 px, izvan
receptivnog polja ruba patcha) srednja apsolutna razlika je 6,9·10⁻⁵, najveća
4,9·10⁻⁴ — zaokruživanje half-floata. Bez te provjere greška u redoslijedu
stupaca `mat4`-a ili u poravnanju poolinga davala bi mrežu koja „radi“ i
tiho gubi.

<!-- REZULTATI, CIJENA, REPRODUKCIJA: nakon mjerenja -->
