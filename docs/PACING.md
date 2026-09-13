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

Render kontekst živi na **skrivenom 1×1 prozoru**: na ovom stogu (SDL2 preko
SDL3, Wayland, Mesa) kontekst mora biti aktivan na nekoj površini —
`MakeCurrent` bez površine javlja uspjeh i ne ostavi ništa aktivno.

**Predaja bez fencea.** Prirodan način da nit prikaza zna kad je okvir gotov je
`glFenceSync` + čekanje u drugom kontekstu. Na Mesi 26.2 (radeonsi)
`glFenceSync(GL_SYNC_FENCE, 0)` vraća `GL_INVALID_ENUM`. Render nit zato sama
napravi `glFinish` prije predaje; ionako ima samo nekoliko stotina mikrosekundi
CPU posla po okviru, pa je blokiranje na GPU-u gotovo besplatno, a „predano”
postaje izmjereno, ne procijenjeno vrijeme.

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

Oba završavaju **swapom**. Što kompozitor (Hyprland) i monitor dodaju nakon
toga aplikacija ne vidi i nije uključeno; za to bi trebao vanjski senzor
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
realtime (zidni sat) 8 s po retku, prvih 30 okvira odbačeno. Monitor 100 Hz,
Hyprland; bez vsynca ako nije drugačije navedeno. Prikazani FPS i intervali
izračunati su iz stvarnih vremena swapa (`--present-csv`), ne iz GPU brojača.
Sirovi zapisi: `captures/pacing/`.

### FPS u ovisnosti o cijeni renderiranja

Sponza je laka scena (G-buffer ≈ 0,9 ms), pa se cijena igre simulira
sintetskim opterećenjem: ×n dodatnih G-buffer prolaza po okviru.

| opterećenje | bez FG | FG odmah | FG ravnomjerno | ubrzanje (ravnomjerno) | renderirano uz FG |
|---|---:|---:|---:|---:|---:|
| ×0 | **313,0** | 305,4 | 308,3 | 0,98× | 154,1 |
| ×4 | 155,3 | 194,5 | **211,0** | 1,36× | 105,4 |
| ×8 | 112,0 | 166,2 | **168,8** | 1,51× | 84,3 |
| ×12 | 90,7 | 138,3 | **135,3** | 1,49× | 67,6 |
| ×16 | 73,4 | 120,3 | **116,2** | 1,58× | 58,0 |
| ×24 | 57,7 | 95,8 | **89,9** | 1,56× | 44,9 |

![FPS](../captures/pacing/fps-vs-load.png)

**FPS raste — iznad praga.** Na neopterećenoj sceni generiranje ne donosi
ništa: okvir bez njega traje 3,2 ms, a generiranje s tokom i dva bliteta prikaza
koštaju gotovo isto toliko, pa se renderirani FPS prepolovi i prikazani ostane
na mjestu. To je prag iz `docs/FRAMEGEN.md` (≈2,2 ms), sad izmjeren na ekranu.
Već uz ×4 (okvir ≈6,4 ms) ubrzanje je 1,36×, a od ×8 naviše 1,5–1,6×. Nikad
nije 2×: generiranje nije besplatno, pa se renderirani FPS uz FG uvijek spusti
(uz ×12 s 90,7 na 67,6), a prikazani je dvostruko od toga.

### Ravnomjernost

| opterećenje ×12 | std intervala | p1 | p50 | p99 | generirani → pravi | pravi → generirani |
|---|---:|---:|---:|---:|---:|---:|
| bez FG | 1,38 | 9,51 | 10,82 | 17,56 | — | — |
| FG odmah | **7,03** | **0,17** | 2,81 | 17,54 | 0,26 | 14,21 |
| FG ravnomjerno | **1,00** | 5,58 | 7,32 | 11,39 | 7,48 | 7,30 |

![histogram](../captures/pacing/histogram-L12.png)
![vremenski slijed](../captures/pacing/timeline-L12.png)

Bez pacinga generirani i pravi okvir stižu u razmaku od četvrt milisekunde, a
onda 14 ms ništa: brojač pokazuje 138 fps, a oko vidi 69 fps s dvostrukim
slikama. Histogram je bimodalan, s vrhom u nuli. S pacingom su dva naizmjenična
intervala jednaka (7,48 i 7,30 ms) i std je 1,0 ms — manje nego bez generiranja
(1,38 ms), jer nit prikaza svaki drugi okvir drži na vremenskoj osi neovisno o
tome kako je render nit rasporedila svoj posao. Isto vrijedi na svim
opterećenjima: std 0,52–1,08 ms ravnomjerno naspram 3,0–10,3 ms odmah.

### Latencija

Srednja vrijednost, ms, od uzorkovanja ulaza do swapa (vidi gore što to ne
uključuje).

| opterećenje | bez FG | FG odmah: pravi | FG ravnomjerno: prvi odziv (generirani) | FG ravnomjerno: pravi | cijena pacinga (pravi − bez FG) |
|---|---:|---:|---:|---:|---:|
| ×0 | 3,19 | 6,55 | 6,22 | 9,52 | +6,3 |
| ×4 | 6,42 | 10,29 | 9,22 | 14,00 | +7,6 |
| ×8 | 8,91 | 12,05 | 11,58 | 17,54 | +8,6 |
| ×12 | 11,03 | 14,46 | 14,48 | 21,96 | +10,9 |
| ×16 | 13,60 | 16,63 | 16,94 | 25,65 | +12,1 |
| ×24 | 17,33 | 20,89 | 21,92 | 33,20 | +15,9 |

Latencija pravog okvira uz ravnomjerni prikaz je vrijeme okvira s generiranjem
plus pola render perioda (uz ×12: 14,5 + 7,4 ≈ 22,0 ms, izmjereno 21,96). Prema
okviru bez generiranja to je **+11 ms uz ×12 i +16 ms uz ×24 — približno jedan
render okvir**, kako je plan i predvidio. Dio te cijene (≈3,5 ms uz ×12) je
samo generiranje, koje produlji okvir; ostatak je čekanje pola perioda, bez
kojeg nema ravnomjernosti.

Prvi odziv — generirani okvir, koji već nosi pola gibanja iz novog ulaza —
stiže na ekran približno kad bi i pravi okvir bez pacinga (14,5 ms uz ×12), ali
kasnije nego bez generiranja uopće (11,0 ms). Generiranje, dakle, ne smanjuje
latenciju ni u najpovoljnijem čitanju: podiže glatkoću po cijeni odziva, i to
je poštena formulacija rezultata.

### Vsync (100 Hz)

| redak | prikazano fps | std | p99 | latencija pravog |
|---|---:|---:|---:|---:|
| ×12 bez FG | 90,2 | 1,36 | 17,49 | 11,69 |
| ×12 FG ravnomjerno | 99,9 | 0,53 | 11,46 | 29,12 |
| ×24 bez FG | 57,1 | 1,91 | 24,22 | 17,55 |
| ×24 FG ravnomjerno | 92,5 | 0,91 | 13,31 | 32,24 |

S vsyncom je strop osvježavanje monitora. Uz ×12 bez generiranja okvir ionako
ne stiže do 100 Hz; s njim je prikaz prikovan na 100 fps s najmanjim rasipanjem
u cijeloj tablici (std 0,53 ms), ali uz 29 ms latencije jer swap sad čeka i
kompozitor. Uz ×24 generiranje podiže 57 na 92,5 fps. Pod Waylandom okviri iznad
osvježavanja bez vsynca ne stižu na ekran (kompozitor prikaže zadnji); zato su
glavne tablice bez vsynca mjerene na vremenima swapa, a ne na fotonima.

### Alternative stagingu (×12)

| redak | prikazano fps | std | p99 | latencija pravog |
|---|---:|---:|---:|---:|
| staging (zadano) | 135,3 | 1,00 | 11,39 | 21,96 |
| staging + `glFlush` po fazi | 141,7 | 1,01 | 10,64 | 20,99 |
| staging + `glFinish` po fazi | 104,0 | 0,70 | 11,96 | 28,67 |
| bez staginga (blit neposredno prije swapa), `glFlush` po fazi | 151,6 | 6,15 | 15,85 | 26,61 |

Posljednji redak je iz razvoja (5 s, prije staginga) i pokazuje zašto staging
postoji: FPS je najviši, a paced okviri ponovno stižu u parovima. `glFinish` po
fazi drži najmanje rasipanje, ali gubi četvrtinu FPS-a. `glFlush` uz staging
izgleda za 5 % brži od zadanog, ali razlika je unutar raspona koji isti redak
pokazuje između ponovljenih pokretanja (135–137 fps) uvećanog za jedan
neponovljeni uzorak, pa se ne proglašava rezultatom.

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

