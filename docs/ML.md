# M9 — naučena mješavina za generiranje okvira

Tema traži i naučenu komponentu. Pravi FSR3 je u cijelosti klasičan računalni
vid; naučeni pristupi (DLSS, FSR4 „Redstone“) žive na hardveru s vlasničkim
matričnim putom (Tensor Cores + cuDNN/TensorRT, XMX i sl.). FSR3-lite je, kao
i pravi FSR3, namjerno pisan prenosivo — OpenGL 4.6 compute shaderi bez
vlasničkog ML puta — pa M9 ne zamjenjuje jezgru nego mjeri jedno izolirano
pitanje: **može li mala mreža, uz iste ulaze, bolje od ručne heuristike
odlučiti kako spojiti kandidate koje klasični lanac već proizvodi — i koliko
to košta kad se, namjerno, ne koristi tenzorska/matrična akceleracija, čak ni
na kartici koja je ima.**

Opcija (a) iz `PLAN.md` (Modul E): blend/inpainting maska, ne naučeni
upsampling.

**Ukratko.** Mreža od 7 199 parametara (c8-c16), učena na vlastitom rendereru
i izvedena u compute shaderima, na svih osam mjernih pogleda podiže SSIM, a
PSNR na sedam od osam, za +0,03 do +0,20 dB (1080p Quality: 35,00 → 35,10 dB).
Dobitak je stabilan preko tri sjemena, ali malen, ne popravlja sustavno najgori
okvir i ne prenosi se na neviđenu scenu. Plaća se ~1,1 ms GPU-a po okviru na
1080p (generiranje okvira 0,33 → 1,40 ms), što pri sintetskom opterećenju ×12
spušta prikazani FPS s 329 na 290 i dodaje ~0,8 ms latencije. **Ni na kartici s
Tensor Core jedinicama se ne isplati, jer ih naša implementacija namjerno ne
koristi**: heuristika je 0,10 dB lošija za manje od četvrtine cijene
generiranja okvira (0,33 naspram 1,40 ms). Vrijedi kao izmjeren odgovor na
pitanje teme — gdje je granica malog naučenog modela bez vlasničke matrične
akceleracije — a ne kao zamjena jezgre.

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

Vlastiti trener, `tools/fg_train` (C++20, OpenMP, samo CPU), umjesto PyTorcha:
model je dovoljno malen da su unaprijedni i povratni prolaz pet operacija
nekoliko stotina linija petlji koje prevoditelj vektorizira, pa vlastiti
trener izbjegava Python ovisnost (projekt nema nijednu osim Pillowa) i,
pišući obje strane u istom stilu, olakšava provjeru shadera naspram trenera
bit po bit (niže) — s PyTorchem bi ta usporedba morala premostiti dva
različita run-timea.

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
unaprijedni prolaz i uspoređuje na pikselima izvan receptivnog polja ruba
patcha (28 px), a uz rub samog okvira na svim pikselima. Za izabrani model
srednja apsolutna razlika je 7,1·10⁻⁵, najveća 4,7·10⁻⁴ — zaokruživanje
half-floata. Isti broj (6,9·10⁻⁵ / 4,9·10⁻⁴) dala je provjera prvog modela
prije i poslije preslagivanja prolaza, pa preslagivanje nije promijenilo
funkciju. Bez ove provjere greška u redoslijedu stupaca `mat4`-a ili u
poravnanju poolinga davala bi mrežu koja „radi“ i tiho gubi.

## Rezultati

### Izbor modela: validacijske putanje, cijeli okviri

Šest modela c8-c16 (`scripts/ml_sweep.sh`): dva gubitka × tri sjemena, svi na
18 putanja. PSNR u dB i dobitak naspram heuristike; nijedna od ovih putanja
nije ni u skupu za učenje ni među mjernim pogledima.

| Model | Sponza r4, 30 fps | proc. t80, 60 fps | Sponza r2, 20 fps | proc. t100, 20 fps | srednji dobitak |
|---|---|---|---|---|---|
| heuristika | 47,14 | 34,01 | 33,34 | 32,81 | — |
| Charbonnier, sjeme 1 | 48,00 (+0,86) | 34,27 (+0,26) | 33,41 (+0,07) | 33,23 (+0,42) | **+0,40** |
| Charbonnier, sjeme 2 | 47,77 (+0,63) | 34,25 (+0,24) | 33,45 (+0,11) | 33,19 (+0,38) | +0,34 |
| Charbonnier, sjeme 3 | 47,71 (+0,57) | 34,33 (+0,32) | 33,42 (+0,08) | 33,20 (+0,39) | +0,34 |
| MSE, sjeme 1 | 46,76 (−0,38) | 34,12 (+0,11) | 33,37 (+0,03) | 33,08 (+0,27) | +0,01 |
| MSE, sjeme 2 | 46,11 (−1,03) | 34,10 (+0,09) | 33,35 (+0,01) | 33,04 (+0,23) | −0,18 |
| MSE, sjeme 3 | 46,74 (−0,40) | 34,10 (+0,09) | 33,35 (+0,01) | 33,01 (+0,20) | −0,03 |

Gubitak je odlučio, sjeme nije: sva tri Charbonnier modela dobivaju na sve četiri
putanje, a sva tri MSE modela gube na Sponzi. Suprotno očekivanju, MSE —
gubitak koji PSNR mjeri — daje lošiji PSNR. Vjerojatno objašnjenje, **koje nije
zasebno provjereno**: gradijent MSE-a proporcionalan je pogrešci, pa je uz istu
stopu učenja na malim pogreškama (većina piksela) gotovo nula i učenje vode
rijetke velike pogreške, dok Charbonnier svakom pikselu daje signal
podjednake jačine. Provjera bi tražila MSE uz veću stopu učenja ili dulje
učenje; to nije napravljeno. Izabran je model s najvećim srednjim dobitkom na validaciji
(Charbonnier, sjeme 1) i on je `captures/ml/weights/blend-c8-c16.bin`.

### Mjerni pogledi (izabrani model, mjereno jednom)

`scripts/run_metrics.py --group fg-ml`: isti pogledi, okviri i zastavice kao
tablice M7/M8 (`--jitter`); stupac heuristike reproducira te tablice na
drugu decimalu.

| Pogled | Heuristika PSNR / SSIM | Mreža PSNR / SSIM | Dobitak | Najgori okvir (heur. → mreža) |
|---|---|---|---|---|
| 1080p Quality (1,5×) | 35,00 / 0,9443 | 35,10 / 0,9458 | +0,10 dB / +0,0016 | 27,56 → 27,62 |
| 1080p native | 36,50 / 0,9628 | 36,69 / 0,9650 | +0,19 dB / +0,0022 | 27,88 → 27,10 |
| 1080p Performance (2×) | 33,55 / 0,9166 | 33,58 / 0,9176 | +0,03 dB / +0,0010 | 27,40 → 27,33 |
| 720p Quality | 33,95 / 0,9293 | 33,98 / 0,9307 | +0,03 dB / +0,0014 | 27,43 → 27,57 |
| proceduralna scena | 34,06 / 0,9708 | 34,26 / 0,9722 | +0,20 dB / +0,0015 | 33,74 → 33,87 |
| 120 fps | 35,94 / 0,9553 | 36,03 / 0,9568 | +0,09 dB / +0,0014 | 31,23 → 30,28 |
| 30 fps | 32,85 / 0,9309 | 32,83 / 0,9337 | −0,02 dB / +0,0028 | 24,11 → 23,71 |
| 20 fps | 30,95 / 0,9122 | 31,11 / 0,9181 | +0,16 dB / +0,0058 | 20,23 → 20,71 |

Što tablica kaže:

- **Dobitak je malen i dosljedan.** SSIM raste na svih osam pogleda, PSNR na
  sedam (30 fps: −0,02 dB). Najveći je tamo gdje kandidati nose najviše
  informacije — native render i proceduralna scena s objektima u pokretu — a
  najmanji uz Performance i 720p, gdje su svi kandidati jednako mutni i pravilo
  miješanja ima malo izbora.
- **Najgori okvir se ne popravlja sustavno** (native −0,78 dB, 120 fps
  −0,95 dB, 20 fps +0,48 dB). Mreža uči prosjek; rijetki okviri s brzim
  zamahom kamere u skupu su za učenje zastupljeni koliko su i rijetki.
- **Mjera nakon prvog modela.** Prvi model (prije ispravka ruba okvira i
  proširenja skupa) na istim je pogledima gubio 0,16 dB na 30 fps i 0,39 dB na
  20 fps. Dodane putanje od 20/30 fps odabrane su gledajući taj rezultat;
  izbor između modela nakon toga napravljen je samo na validaciji.

### Ovisnost o sjemenu (mjerni pogledi)

Ista tri Charbonnier modela sa sweepa, na 1080p Quality i proceduralnoj sceni
(`--group fg-ml-models`):

| Model | Sponza PSNR / SSIM | najgori okvir | proceduralna PSNR / SSIM | najgori okvir |
|---|---|---|---|---|
| heuristika | 35,00 / 0,9443 | 27,56 | 34,06 / 0,9708 | 33,74 |
| sjeme 1 (izabrano) | 35,10 / 0,9458 | 27,62 | 34,26 / 0,9722 | 33,87 |
| sjeme 2 | 35,20 / 0,9470 | 25,99 | 34,27 / 0,9724 | 33,85 |
| sjeme 3 | 35,13 / 0,9466 | 25,68 | 34,37 / 0,9729 | 33,93 |

Srednji dobitak ne ovisi o sjemenu: sva tri modela su iznad heuristike na oba
pogleda (+0,10 do +0,20 dB na Sponzi, +0,20 do +0,31 dB na proceduralnoj
sceni), pa je rezultat svojstvo metode, a ne sretnog izvlačenja. Najgori okvir
Sponze ovisi: sjeme 1 ga zadrži, sjemena 2 i 3 ga spuste za ~1,7 dB. Izabrani
model nije biran po tome — validacija ga je izabrala prije ovog mjerenja — ali
ni ne smije se čitati kao dokaz da mreža čuva najgori okvir.

### Veličina mreže i skup podataka (mjerni pogledi)

Svi Charbonnier, sjeme 1, 12 000 koraka (`scripts/ml_train.sh`).

| Model | parametara | Sponza PSNR / SSIM | najgori okvir | proceduralna PSNR / SSIM |
|---|---|---|---|---|
| heuristika | — | 35,00 / 0,9443 | 27,56 | 34,06 / 0,9708 |
| c4-c8 | 1 875 | 34,91 / 0,9455 | 25,26 | 34,15 / 0,9715 |
| **c8-c16** | 7 199 | 35,10 / 0,9458 | 27,62 | 34,26 / 0,9722 |
| c12-c24 | 15 979 | 35,18 / 0,9462 | 28,92 | 34,36 / 0,9729 |
| c8-c16, samo Sponza | 7 199 | 35,05 / 0,9460 | 26,01 | 34,05 / 0,9707 |

- **Kapacitet se vidi.** Najmanja mreža na Sponzi pada ispod heuristike (SSIM
  joj je ipak viši), najveća je najbolja na oba pogleda i jedina podiže najgori
  okvir (+1,4 dB). Razlike između c8-c16 i c12-c24 (0,08–0,10 dB) manje su od
  raspona sjemena iz prethodne tablice, pa rang tih dviju nije siguran; rang
  c4-c8 ispod njih jest.
- **Neviđena scena ne dobiva ništa.** Model učen samo na Sponzi na
  proceduralnoj sceni daje heuristiku (34,05 naspram 34,06 dB): ne šteti, ali
  ono što je naučio o Sponzinim kandidatima ne prenosi se na scenu s
  pokretnim objektima i drugom vrstom disokluzije. Dobitak glavnog modela na
  proceduralnoj sceni (+0,20 dB) dolazi iz drugih dijelova iste animacije u
  skupu za učenje. Na Sponzi mu model sa svih putanja donosi +0,05 dB više,
  unutar raspona sjemena.

## Cijena

### GPU po prolazu (1080p Quality, Sponza, 300 okvira, mirno računalo)

| Prolaz | c4-c8 | c8-c16 | c12-c24 |
|---|---|---|---|
| značajke + enc0 | 0,13 | 0,19 | 0,29 |
| pool0 | 0,01 | 0,02 | 0,06 |
| enc1 | 0,06 | 0,19 | 0,48 |
| pool1 | 0,01 | 0,01 | 0,01 |
| enc2 | 0,03 | 0,14 | 0,35 |
| enc3 | 0,03 | 0,12 | 0,36 |
| up1 + skip | 0,02 | 0,02 | 0,03 |
| dec1 | 0,05 | 0,24 | 0,47 |
| dec0 + mješavina | 0,13 | 0,14 | 0,17 |
| **naučeni dio ukupno** | **0,48** | **1,07** | **2,22** |
| **generiranje okvira** (heuristika 0,33) | 0,80 | 1,40 | 2,52 |

Na 720p Quality c8-c16 košta 0,52 ms (generiranje okvira 0,17 → 0,73 ms).

Dva prolaza ne ovise o širini mreže: značajke i mješavina oba dohvaćaju sedam
kandidata i dva vektorska polja po pikselu pune rezolucije, ukupno ~1,1 ms za
najmanju mrežu. To je donja granica ovog oblika modela na 1080p, i razlog zašto
je i c4-c8 skoro dvostruko skuplji od cijele heuristike.

### Prikaz u stvarnom vremenu (1080p Quality, ravnomjerni pacing)

`--run-seconds 8 --load N`, iste zastavice kao `docs/PACING.md`; snimke u
`captures/ml/realtime/`. Brojevi su iz sažetka same aplikacije u istoj sesiji
(stupac bez FG-a zato nije identičan tablici M8, koja odbacuje prve okvire).

| Opterećenje | bez FG | FG, heuristika | FG, mreža | latencija do pravog okvira (heur. / mreža) |
|---|---|---|---|---|
| ×0 | 1058 fps | 1090 fps | 708 fps | 2,0 / 3,0 ms |
| ×12 | 199 fps | 329 fps | 290 fps | 6,2 / 7,0 ms |
| ×24 | 116 fps | 209 fps | 188 fps | 9,7 / 10,8 ms |

Uz opterećenje generiranje okvira s mrežom i dalje diže FPS (×12: +46 % naspram
bez FG-a), ali heuristika ga diže više (+66 %). Na neopterećenoj sceni mreža
pomiče prag isplativosti: FG s njom gubi gotovo trećinu FPS-a (1058 → 708 fps),
dok heuristika na ovoj kartici nema trošak vidljiv u prikazanom FPS-u
(1058 → 1090 fps) — na brzom GPU-u je stvarni okvir toliko jeftin da
naizmjenično umetanje jeftinih generiranih okvira poveća broj prikaza, ne
smanji ga; to je vidljivo tek kad je generirani okvir sam skup, kao kod mreže.

### Isplati li se

Ne bitno bolje ni na kartici s Tensor Core jedinicama koje naša implementacija
namjerno ne koristi, i to je izmjereno, a ne pretpostavljeno: +0,10 dB na
1080p Quality je razlika koju tablica vidi, a oko teško, dok generiranje
okvira raste s 0,33 na 1,40 ms i pri ×12 prikazani FPS pada s 329
(heuristika) na 290 (mreža). Apsolutni trošak je niži nego na sporijem GPU-u
(1,1 ms naspram nekadašnjih 2,7 ms), ali razlog nije nestao: FSR3-lite je
namjerno pisan prenosivo, u općim OpenGL compute shaderima, pa `mat4`
operacije mreže (`shaders/ml_conv.comp`) troše obične ALU jedinice, ne
Tensor Core. Dvije stvari bi to mogle promijeniti, i obje su izvan ovog
oblika implementacije: vlasnički matrični put (cijena mreže bila bi mali dio
1,1 ms) i bogatiji kandidati (mreža ovdje ne može bitno bolje od najboljeg
kandidata po pikselu; na validacijskim patchevima izabranog modela taj je
limit 50,8 dB naspram 47,3 dB heuristike i 48,0 dB mreže). To je, u malom,
razlog zašto DLSS, FSR4/Redstone i XeSS 2 FG zahtijevaju baš određeni
vlasnički hardver, dok FSR3 (i FSR3-lite) rade posvuda.

## Demonstracija

| Što | Kako |
|---|---|
| Cijeli postupak na malom primjeru (~1 min) | `scripts/ml_demo.sh`: snimi 2 putanje na 720p, istrenira c4-c8 u 1 500 koraka, provjeri shader naspram trenera i ispiše naredbu za interaktivno pokretanje. Zadnji put: +0,54 dB na kontrolnoj putanji, razlika shader/trener najviše 4,8·10⁻⁴. |
| Uživo | `build/fsr3lite ... --fg --fg-ml captures/ml/weights/blend-c8-c16.bin`. Tipka **M** mijenja naučenu i heurističku mješavinu, tipka **0** kruži kroz prikaze 9 → 10 → 11. Prikaz **11** su težine mreže. |
| Boje prikaza 11 | siva = heuristika · tamno/svijetlo zelena = igrini vektori iz t−1 / t · tamno/svijetlo plava = optical flow iz t−1 / t · crvena = trenutni okvir · narančasta = prethodni okvir |
| Slike za rad | `python3 scripts/ml_gallery.py` → `captures/ml/gallery/*.png` i `legend.png`. Stupci: referenca · heuristika · mreža · težine · greška heuristike ×6 · greška mreže ×6, uz izreze gdje mreža najviše pomaže i gdje najviše šteti. |
| Pojedinačni okviri | `--validate-fg --fg-shots DIR --fg-shot-frames 57,63` zapisuje `frameN_reference/_blend/_heuristic/_learned/_weights.png`. |

Tri slike odabrane za rad, od osam koje skripta generira (najgori okvir
heuristike, najveći dobitak i najveći gubitak mreže, po tri mjerna pogleda):

- `captures/ml/gallery/sponza-20fps-frame18.png` — u istom okviru najveći
  dobitak (izrez 512,640: 15,21 → 38,15 dB, mreža uklanja prugavi artefakt na
  rubu stupa/prozora) i najveći gubitak (izrez 1152,768: 25,11 → 21,03 dB,
  mreža zamuti sitnu granu lišća) na 20 fps, gdje je disokluzija najveća.
- `captures/ml/gallery/sponza-1080p-q-frame57.png` — isti obrazac u
  normalnom radnom režimu (1080p Quality, 1,5×): dobitak na rubu
  disokluzije (stepenica, 18,32 → 22,83 dB), mali gubitak na ponavljajućoj
  teksturi (39,55 → 31,51 dB na tom izrezu, uz +1,59 dB na cijelom okviru).
- `captures/ml/gallery/proceduralna-frame67.png` — neviđena scena: razlika
  je zanemariva i na najboljem izrezu (29,03 → 30,11 dB) i na cijelom okviru
  (+0,30 dB), u skladu s nalazom da mreža na neviđenoj sceni ne dobiva ništa
  (vidi tablicu veličine mreže gore).

### Greška: prikaz težina usred izvođenja (popravljeno)

Ako se prikaz težina (11) uključi usred izvođenja, varijanta shadera
prevedena s `#define` i prvi put pokrenuta tek tada — s teksturom alociranom
u istom okviru u kojem se prvi put i piše — pokvarila je taj okvir na jednom
razvojnom stroju (Mesa/AMD, Linux): 26,6 dB umjesto 37,6 dB. Popravak je
jedan program s uniformom
`uWriteWeights` umjesto dvije `#define`-varijante, i tekstura alocirana
unaprijed (`fg_ml_blend.comp`, `MlBlend::ensure`), pa uključivanje prikaza
usred izvođenja ne prevodi ništa novo niti prvi put piše u tek alociranu
teksturu. Provjera: 0 različitih okvira naspram izvođenja bez prikaza.

Trajanje passa 12 ponovno je izmjereno nakon popravka (`--debug-view 0`
naspram `--debug-view 11`, isti pogled, 1080p Quality, mirno računalo):
grananje po uniformu ne mijenja cijenu — 0,18 ms naspram 0,22 ms na
prolazu, 2,17 naspram 2,12 ms na cijelom okviru, razlika unutar šuma
mjerenja na ovom GPU-u.

## Reprodukcija

```
scripts/ml_dataset.py                       # ~3 min, 2,9 GB u captures/ml/data/
scripts/ml_sweep.sh                         # 2 gubitka x 3 sjemena, ~50 min CPU
scripts/ml_select.py captures/ml/exp/*.bin  # izbor na validaciji, cijeli okviri
cp captures/ml/exp/c8-c16-charbonnier-s1.bin captures/ml/weights/blend-c8-c16.bin
scripts/ml_train.sh                         # ablacije veličine i podataka
scripts/run_metrics.py --group fg-ml        # mjerni pogledi: heuristika i mreža
scripts/run_metrics.py --group fg-ml-models # sjemena i ablacije
build/fg_train gradcheck                    # provjera povratnog prolaza
```

Provjera shadera naspram trenera za bilo koji model:

```
build/fsr3lite --width 1920 --height 1080 --scale 1.5 --upscaler fsr --scripted --jitter \
    --validate-fg --warmup 16 --fixed-dt 0.0333333 --frames 40 \
    --scene assets/sponza/Sponza.gltf --scene-fit 12 --path-radius 4 --path-phase 1.3 \
    --fg-ml captures/ml/weights/blend-c8-c16.bin --ml-dump /tmp/verify.bin
build/fg_train eval --weights captures/ml/weights/blend-c8-c16.bin --data /tmp/verify.bin --margin 28
```

Učenje nije bit-deterministično: OpenMP zbraja gradijente dretvi redom kojim
su uzorci raspoređeni, pa ponovljeno učenje istog sjemena daje model koji se
razlikuje na zadnjim decimalama. Težine korištene u tablicama su u
repozitoriju (`captures/ml/weights/`, `captures/ml/exp/`).
