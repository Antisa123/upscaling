# M8 — prikaz: frame pacing, latencija, UI kompozicija

M7 je proizveo međuokvir i izmjerio ga naspram pravog rendera, pa ga bacio. M8
ga stavlja na ekran. Tri su pitanja i sva tri su pitanja o **vremenu i
redoslijedu**, ne o slici:

1. **Kada** se prikazuje koji okvir — frame pacing. Dvostruko više okvira koji
   stignu u parovima nije dvostruko glađe; to je isti trzaj s duplim brojačem.
2. **Koliko kasni** ono što igrač vidi za onim što je napravio — latencija.
   Međuokvir između t−1 i t postoji tek kad postoji t, pa pravi okvir t mora
   pričekati pola render perioda iza njega. To je cijena tehnologije i mjeri se.
3. **Gdje u lancu** ulazi UI — HUD nacrtan prije generiranja je samo još
   piksela koje warp pomiče po vektorima scene iza njih.

Uz to prolazi 8–9 (inpainting slike) iz `docs/FRAMEGEN.md`.

## Pokretanje

```
./build/fsr3lite --upscaler fsr --scale 1.5 --fg \
    --scene assets/sponza/Sponza.gltf --scene-fit 12 --path-radius 3 --path-phase -0.6
```

| Zastavica / tipka | Značenje |
|---|---|
| `--pacing paced\|immediate` / `Y` | ravnomjerno (zadano) ili generirani i pravi okvir odmah jedan za drugim |
| `--present-fg <0\|1>` / `G` | 0 = generiranje radi (i plaća se), ali se prikazuju samo pravi okviri; tipka `G` gasi i samo generiranje |
| `--vsync` / `V` | vsync na kontekstu prikaza |
| `--ui none\|composite\|baked` / `H` | HUD: bez njega, komponiran nakon generiranja, upečen prije njega; `H` ga skriva |
| `--tear-lines` / `T` | traka uz lijevi rub: zelena = pravi okvir, magenta = generirani, bijeli blok se pomiče jedan korak po prikazanom okviru |
| `--load <n>` / `[` `]` | sintetsko opterećenje: n dodatnih G-buffer prolaza po okviru |
| `--run-seconds <s>` | realtime mjerenje zadanog trajanja (zidni sat) |
| `--present-csv <put>` | jedan redak po prikazanoj slici: okvir, vrsta, vrijeme uzorkovanja ulaza, predaje, prikaza, čekanja na GPU |
| `--gpu-flush <0\|1\|2>` | ablacija: `glFlush` / `glFinish` nakon svake faze render niti (zadano 0, vidi niže) |

Bez `--frames` i `--run-seconds` HUD je po zadanom uključen (`composite`); u
mjernim pokretanjima isključen, jer nije besplatan i nije dio nijedne ranije
tablice.

## Arhitektura prikaza

```
render nit (skriveni 1×1 prozor, render kontekst)          nit prikaza (prozor, dijeljeni kontekst)
───────────────────────────────────────────────           ───────────────────────────────────────
ulaz, kamera, scena         ← t_uzorak
G-buffer, FSR, optical flow, generiranje
HUD, kompozicija u slot (pravi + generirani, RGBA8)
glFinish                    ← t_predano
submit(slot) ─────────────────────────────────────────►  blit generirani → back buffer, glFinish
                                                          swap                     ← prikaz generiranog
                                                          blit pravi → back buffer, glFinish
◄──────────────────────────────── „staged” ─────────────  (render nit smije dalje)
sljedeći okvir …                                          čekanje do t_gen + ½·P
                                                          swap                     ← prikaz pravog
                                                          slot natrag u slobodne
```

**Dvije niti, dva konteksta.** Pravi okvir mora na ekran pola render perioda
nakon generiranog, a render nit za to vrijeme već mora raditi sljedeći okvir.
Čeka li render nit sama, period okvira postaje render + čekanje i FPS se vrati
tamo gdje je bio. Zato prikaz ima vlastitu nit s vlastitim GL kontekstom koji
dijeli teksture s render kontekstom. Framebuffer objekti se među kontekstima ne
dijele, pa nit prikaza pravi svoje.

Render kontekst živi na **skrivenom 1×1 prozoru**: na izvornom razvojnom
stogu (SDL2, Wayland, Mesa) kontekst mora biti aktivan na nekoj površini —
`MakeCurrent` bez površine javlja uspjeh i ne ostavi ništa aktivno. Isto
vrijedi i na Windows/WGL-u, gdje je ovo kasnije mjereno.

**Predaja bez fencea.** Prirodan način da nit prikaza zna kad je okvir gotov je
`glFenceSync` + čekanje u drugom kontekstu. Na Mesi 26.2 (radeonsi), gdje je
ovo prvo izmjereno, `glFenceSync(GL_SYNC_FENCE, 0)` vraća `GL_INVALID_ENUM`;
to nije ponovno provjereno na Windows/NVIDIA driveru, pa `glFinish` ostaje
zajednički, provjereno ispravan put na oba. Render nit ionako ima samo
nekoliko stotina mikrosekundi CPU posla po okviru, pa je blokiranje na GPU-u
gotovo besplatno, a „predano” postaje izmjereno, ne procijenjeno vrijeme.

**Povratni pritisak.** Tri slota (par RGBA8 tekstura u prikaznoj razlučivosti).
Kad su sva u letu, `acquire()` blokira — isto što radi swap chain — pa render
nit ne može otići proizvoljno daleko ispred ekrana i gomilati latenciju.

**Period.** Čekanje između generiranog i pravog okvira je pola eksponencijalnog
prosjeka (α = 0,1) razmaka između dviju predaja. Mjeri se tamo gdje je
definiran — kad par postane spreman — a zastoji dulji od 250 ms (reload
shadera, pomicanje prozora) ne ulaze u prosjek. Ako sljedeći par stigne dok nit
prikaza još čeka, čekanje se prekida: nit kasni, a zadržavanje ovog okvira samo
bi prebacilo kašnjenje na idući.

### Zašto staging

Prva verzija radila je blit neposredno prije svakog swapa. Mjereno na
opterećenju ×12 (render ≈ 13 ms), „ravnomjerni” okviri su i dalje stizali u
parovima: std intervala 5,7–6,2 ms, p1 0,2 ms, a nit prikaza je na blit pravog
okvira čekala 3–6 ms. Uzrok je raspoređivanje posla na GPU-u: dva konteksta na
istom GPU-u ne mogu preskočiti red — kernel izvršava poslove redom kojim su
predani — a render nit preda cijeli okvir posla u otprilike milisekundu. Blit
predan pola okvira kasnije čeka iza svega što je render nit do tada predala, i
to čekanje je dugo i promjenjivo koliko i sam render.

Isprobane su dvije alternative na strani render niti (`--gpu-flush`):

- `glFlush` nakon svake faze: ne pomaže. Flush samo predaje posao kernelu, a
  red ostaje isti.
- `glFinish` nakon svake faze: pacing postaje dobar (std 0,7–1,0 ms), ali FPS
  padne za četvrtinu do trećinu — GPU u svakoj točki sinkronizacije stoji dok
  se CPU probudi i preda iduću fazu.

Staging rješava isto bez cijene: oba bliteta idu odmah pri predaji, dok je GPU
prazan (render nit je upravo napravila `glFinish`), a render nit nastavlja tek
kad je pravi okvir gotov u back bufferu. U trenutku prikaza pravog okvira
preostaje samo swap, koji ne čeka GPU. Render nit zbog toga čeka blit i
`glFinish` pravog okvira (i generiranog, uz FG) prije nego nastavi. To nije
besplatno ni bez generiranja: uz ×12 okvir bez FG-a pao je s 95,2 fps prije
staginga na 90,7–91,7 fps nakon njega (≈4 %, 5–8 s po pokretanju).

## Latencija: što se mjeri, a što ne

Dva broja po okviru N, oba od `t_uzorak(N)` — trenutka nakon obrade ulaza,
pomaka kamere i ažuriranja scene, dakle kad je stanje okvira fiksirano:

- **do pravog okvira**: `t_prikaz(pravi N) − t_uzorak(N)`,
- **do prvog odziva**: `t_prikaz(generirani N−½) − t_uzorak(N)` — generirani
  okvir između N−1 i N već sadrži pola gibanja iz ulaza N, pa je to prvi
  trenutak u kojem se ulaz uopće vidi.

Oba završavaju **swapom**. Što kompozitor (Hyprland na izvornom razvojnom
stogu, DWM na Windowsu) i monitor dodaju nakon toga aplikacija ne vidi i nije
uključeno; za to bi trebao vanjski senzor
(fotodioda ili kamera visoke brzine). Uspoređuju se, dakle, načini prikaza
međusobno na istom stogu, a ne apsolutna latencija od miša do fotona.

## HUD

`src/present/hud.{h,cpp}`, `shaders/ui_hud.comp`, `shaders/ui_compose*.comp`.
Font je DejaVu Sans Mono Bold rasteriziran Pillowom u atlas 16×6 ćelija
(`scripts/make_font_atlas.py`); tekst je `r8ui` tekstura znakova, graf
intervala prikaza `r32f` tekstura. Ploča (sedam redaka teksta + graf) crta se u
zaseban RGBA8 sloj prikazne razlučivosti, i to samo njezin pravokutnik.

- `composite`: generiranje radi na čistoj slici; HUD se komponira preko pravog
  i preko generiranog okvira neposredno prije predaje.
- `baked`: HUD se upeče u **kopiju** izlaza upscalera prije optical flowa i
  generiranja — upscalerova povijest ostaje čista, kao u igri koja UI crta
  nakon upscalinga — a generiranje interpolira između okvira koji ga sadrže.

Graf je zelen dok je interval unutar četvrtine ciljanog (pola render perioda uz
FG, cijeli bez njega), crven izvan toga; bijela crta je cilj.

## Rezultati

Sponza, kanonski kadar, 1080p Quality (render 1280×720), FSR u pipelineu,
realtime (zidni sat) 8 s po retku, prvih 30 okvira odbačeno. Windows, NVIDIA
RTX 5070, monitor bez fiksne gornje granice u ovom mjerenju; bez vsynca ako
nije drugačije navedeno. Prikazani FPS i intervali izračunati su iz stvarnih
vremena swapa (`--present-csv`), ne iz GPU brojača. Sirovi zapisi:
`captures/pacing/`.

> **Otvoren nalaz, ne skriven:** procjena render perioda u
> `src/present/frame_pacer.cpp` je bila samoreferentna — mjerila je razmak
> između trenutaka kad par postane spreman, što na dovoljno brzom GPU-u
> uključuje i vrijeme koje prezentacijska nit sama čeka (pola perioda po
> paru), pa se procjena hranila vlastitim čekanjem umjesto stvarnom cijenom
> renderiranja. Popravljeno je mjerenjem stvarnog vremena render niti
> (`renderMs`, neovisno o prezentaciji); to je ispravilo `×0`–`×8` niže. Pri
> `×12` i više ostaje otvoren, dublji problem: prezentacijska nit obrađuje
> parove strogo jedan po jedan (uključujući vlastito čekanje) prije nego
> pogleda sljedeći, pa pri dovoljno velikom opterećenju ravnomjerni prikaz i
> dalje degenerira u nepravilan (bimodalan) ritam. Retci `×12` naviše niže su
> zato izmjereni, ali ne i objašnjeni do kraja — čitaj ih kao poznato
> ograničenje, ne kao svojstvo dizajna.

### FPS u ovisnosti o cijeni renderiranja

Sponza je laka scena (G-buffer ≈ 0,16 ms na ovom GPU-u), pa se cijena igre
simulira sintetskim opterećenjem: ×n dodatnih G-buffer prolaza po okviru.

| opterećenje | bez FG | FG odmah | FG ravnomjerno | ubrzanje (ravnomjerno) | renderirano uz FG |
|---|---:|---:|---:|---:|---:|
| ×0 | 1059,1 | 1090,4 | **1208,6** | 1,14× | 604,2 |
| ×4 | 369,0 | 441,0 | **574,3** | 1,56× | 287,1 |
| ×8 | 260,1 | 333,0 | **377,6** | 1,45× | 188,8 |
| ×12 | **197,3** | 328,1 | 161,9 | 0,82× | 80,9 |
| ×16 | **159,6** | 278,6 | 127,9 | 0,80× | 63,9 |
| ×24 | 115,6 | 208,1 | **127,9** | 1,11× | 63,9 |

![FPS](../captures/pacing/fps-vs-load.png)

**FPS raste — do ×8, dosljedno s pragom iz `docs/FRAMEGEN.md` (≈0,48 ms).**
Na ovom GPU-u je čak i neopterećena scena iznad praga (generirani okvir je
jeftiniji od stvarnog, pa ravnomjerni prikaz dobiva 1,14× i ondje gdje ga na
sporijem RX 580 nije bilo). Od ×12 naviše "ravnomjerno" način pada **ispod**
"bez FG" — to je gornji nalaz, ne novi zaključak o isplativosti generiranja:
`immediate` način (FG odmah, bez namjernog čekanja) na istim opterećenjima i
dalje dosljedno ubrzava (1,4–2,0×), pa je do daljnjega pouzdaniji za
usporedbu na ovom GPU-u.

### Ravnomjernost

Opterećenje ×8 (unutar radnog raspona; za ×12 i više vidi napomenu na vrhu):

| opterećenje ×8 | std intervala | p1 | p50 | p99 | generirani → pravi | pravi → generirani |
|---|---:|---:|---:|---:|---:|---:|
| bez FG | 0,34 | 3,26 | 3,92 | 4,68 | — | — |
| FG odmah | **2,78** | **0,19** | 2,61 | 9,59 | 0,40 | 5,61 |
| FG ravnomjerno | **1,14** | 0,21 | 2,59 | 8,89 | 2,71 | 2,59 |

![histogram](../captures/pacing/histogram-L12.png)
![vremenski slijed](../captures/pacing/timeline-L12.png)

Bez pacinga generirani i pravi okvir stižu blizu jedan drugom (0,40 ms), a
onda čekaju 5,6 ms do sljedećeg para — bimodalno, kao i na sporijem GPU-u.
S pacingom su dva naizmjenična razmaka mnogo bliža (2,71 i 2,59 ms) i std je
1,14 ms — manji nego bez generiranja (0,34 ms) je i dalje veći, jer je scena
ovdje toliko brza da apsolutne razlike u milisekundama znače relativno više
šuma; princip (paced izjednačava dva naizmjenična razmaka) i dalje vrijedi.

### Latencija

Srednja vrijednost, ms, od uzorkovanja ulaza do swapa (vidi gore što to ne
uključuje). Retci ×12 naviše su pod istim otvorenim nalazom kao FPS tablica
gore — čitaj cijenu pacinga ondje kao gornju granicu, ne kao svojstvo dizajna.

| opterećenje | bez FG | FG odmah: pravi | FG ravnomjerno: prvi odziv (generirani) | FG ravnomjerno: pravi | cijena pacinga (pravi − bez FG) |
|---|---:|---:|---:|---:|---:|
| ×0 | 1,11 | 1,98 | 1,57 | 2,65 | +1,5 |
| ×4 | 2,89 | 4,68 | 3,38 | 5,03 | +2,1 |
| ×8 | 4,02 | 6,14 | 5,20 | 7,90 | +3,9 |
| ×12 ⚠ | 5,24 | 6,24 | 12,16 | 23,60 | +18,4 |
| ×16 ⚠ | 6,44 | 7,31 | 15,40 | 30,84 | +24,4 |
| ×24 ⚠ | 8,82 | 9,75 | 15,39 | 30,82 | +22,0 |

Do ×8 je obrazac isti kao na RX 580: latencija pravog okvira uz ravnomjerni
prikaz raste s opterećenjem, ali umjereno (+3,9 ms uz ×8), i prvi odziv
(generirani okvir) stiže kasnije nego bez generiranja uopće — generiranje,
dakle, ne smanjuje latenciju ni u najpovoljnijem čitanju, nego kupuje
glatkoću po cijeni odziva. Retci ⚠ pokazuju da se ta cijena pri višem
opterećenju na ovom GPU-u sad puno više nego udvostručuje umjesto da raste
umjereno, izravna posljedica otvorenog nalaza gore.

### Vsync (monitor bez fiksne granice u ovom mjerenju)

| redak | prikazano fps | std | p99 | latencija pravog |
|---|---:|---:|---:|---:|
| ×12 bez FG | 198,0 | 0,50 | 5,97 | 5,25 |
| ×12 FG ravnomjerno ⚠ | 198,3 | 4,13 | 16,37 | 16,81 |
| ×24 bez FG | 115,2 | 0,93 | 10,05 | 8,88 |
| ×24 FG ravnomjerno | 127,9 | 6,46 | 17,39 | 29,58 |

Uz ×12 vsync ne mijenja puno — na ovoj kartici je to isto opterećenje gdje
gornji otvoreni nalaz već vrijedi (gotovo nikakav dobitak, std i dalje
povišen). Uz ×24 generiranje ipak podigne 115,2 na 127,9 fps. Pod Windows
kompozitorom (DWM) prikaz bez vsynca ionako meri stvarna vremena swapa, ne
fotone — isto ograničenje kao i ranije pod Waylandom/Hyprlandom, samo na
drugom stogu.

### Alternative stagingu

Zadano (staging, ×8, unutar radnog raspona): 377,6 prikazanih fps, std 1,14 ms,
p99 8,89 ms, latencija pravog 7,90 ms.

`--gpu-flush` ablacija (`glFlush`/`glFinish` po fazi) je mjerena na ×12 (vidi
tablicu gore) i tamo je i dalje
pod istim otvorenim nalazom (`paced-flush-L12` 147,4 fps, `paced-finish-L12`
127,9 fps) — brojke postoje u `captures/pacing/`, ali ih ne vrijedi čitati
kao svojstvo `--gpu-flush` dok se ×12+ ne popravi. Usporedba "bez staginga"
(blit neposredno prije swapa) iz ranijeg razvoja više nije dostupna kao
zastavica u trenutnom kodu — staging je jedina implementacija — pa se ne
može ponovno izmjeriti na ovom GPU-u; obrazloženje zašto staging postoji
ostaje ono iz arhitekture gore.

### HUD: nakon generiranja ili prije njega

Lockstep, `--validate-fg`, statičan HUD. Referenca je pravi međuokvir s HUD-om
komponiranim preko njega; „HUD” stupci ocjenjuju samo pravokutnik ploče
(480×192 px). Grupa `fg-hud` u `run_metrics.py`.

| Konfiguracija | PSNR okvir | SSIM okvir | PSNR HUD | SSIM HUD | PSNR HUD, blend |
|---|---:|---:|---:|---:|---:|
| 60 fps, HUD nakon generiranja | **35,04** | 0,9450 | **52,01** | 0,9979 | 39,63 |
| 60 fps, HUD upečen prije generiranja | 30,65 | 0,9390 | 20,02 | 0,8811 | 39,64 |
| 20 fps, HUD nakon generiranja | **31,01** | 0,9143 | **48,26** | 0,9900 | 36,61 |
| 20 fps, HUD upečen prije generiranja | 26,77 | 0,8982 | 16,43 | 0,7078 | 36,61 |

![HUD](../captures/pacing/shots/hud-compare.png)

Upečeni HUD warp tretira kao dio scene iza njega: tekst se udvostruči, graf se
razvuče po vektorima zida. U pravokutniku HUD-a to je 32 dB razlike, a i cijeli
okvir izgubi 4,4 dB — više nego sve ablacije generiranja zajedno. Upečeni HUD je
čak lošiji od 50/50 blenda u istom pravokutniku (20,0 naspram 39,6 dB): blend
statičan HUD ostavlja netaknutim, a warp ga pomakne. Komponirani HUD nije
beskonačnih dB samo zato što je pozadina ploče poluprozirna (α = 0,6), pa se
kroz nju vidi generirana scena.

Tear lines (`--tear-lines`): lijevi rub pravog okvira je zelen, generiranog
magenta, a bijeli blok se pomakne za jedan korak po prikazanom okviru. Snimljen
par (lijevo pravi, desno generirani, isti trenutak predaje):

![tear lines](../captures/pacing/shots/tear-compare.png)

## Reprodukcija

```
python3 scripts/run_pacing.py                  # sve: ≈ 4 min, captures/pacing/
python3 scripts/run_pacing.py --loads 0,12     # podskup
python3 scripts/run_pacing.py --report-only    # ponovna analiza postojećih zapisa
python3 scripts/run_metrics.py --group fg-hud  # HUD tablica
```

Mjeriti na neopterećenom sustavu: druga GPU aplikacija (igra) u pozadini
izravno mijenja sve stupce ove tablice, a kvalitetu (PSNR) ne.

