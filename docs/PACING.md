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

> **Popravljen nalaz:** procjena render perioda u `src/present/frame_pacer.cpp`
> je bila samoreferentna — mjerila je razmak između trenutaka kad par postane
> spreman, što na dovoljno brzom GPU-u uključuje i vrijeme koje prezentacijska
> nit sama čeka, pa se procjena hranila vlastitim čekanjem. Popravljeno
> mjerenjem stvarnog vremena render niti (`renderMs`, neovisno o
> prezentaciji). Uz to, `waitUntil()` je čekao preko `std::this_thread::
> sleep_for` za razmake reda 1–3 ms; na ovom Windows/MinGW stogu to je
> preskakalo do sljedećeg zrnca rasporeda OS-a (izmjereno ~10–13 ms
> prekoračenja), i to baš za tolike razmake kakvi su pola-perioda čekanja pri
> svakom opterećenju iznad ×8. Popravak: `waitUntil` sad čeka aktivnim
> petljanjem (`yield()`) umjesto uspavljivanjem za sve razmake ispod 30 ms —
> ovo je posvećena nit čija je jedina svrha to čekanje, pa je trošak
> opravdan. Nakon oba popravka `×0`–`×24` daju dosljedne, glatke rezultate
> (vidi std stupac niže).

### FPS u ovisnosti o cijeni renderiranja

Sponza je laka scena (G-buffer ≈ 0,16 ms na ovom GPU-u), pa se cijena igre
simulira sintetskim opterećenjem: ×n dodatnih G-buffer prolaza po okviru.

| opterećenje | bez FG | FG odmah | FG ravnomjerno | ubrzanje (ravnomjerno) | renderirano uz FG |
|---|---:|---:|---:|---:|---:|
| ×0 | 976,7 | 1047,6 | **1121,8** | 1,15× | 560,8 |
| ×4 | 357,2 | 542,9 | **543,1** | 1,52× | 271,5 |
| ×8 | 233,1 | 385,0 | **374,4** | 1,61× | 187,1 |
| ×12 | 182,5 | 310,9 | **300,8** | 1,65× | 150,3 |
| ×16 | 146,7 | 265,2 | **256,5** | 1,75× | 128,2 |
| ×24 | 110,4 | 200,2 | **189,7** | 1,72× | 94,8 |

![FPS](../captures/pacing/fps-vs-load.png)

**FPS raste na svakom mjerenom opterećenju**, i to sve više što je opterećenje
veće (1,15× na ×0, 1,75× na ×16). Na ovom GPU-u je čak i neopterećena scena
iznad praga iz `docs/FRAMEGEN.md` (generirani okvir je jeftiniji od
stvarnog). `immediate` način je dosljedno malo ispred `ravnomjerno` u sirovom
FPS-u (ne čeka namjerno), ali `ravnomjerno` je taj koji ravnomjerno raspoređuje
prikaz — vidi std niže.

### Ravnomjernost

| opterećenje ×12 | std intervala | p1 | p50 | p99 | generirani → pravi | pravi → generirani |
|---|---:|---:|---:|---:|---:|---:|
| bez FG | 0,54 | 4,48 | 5,52 | 6,55 | — | — |
| FG odmah | **3,01** | **0,18** | 0,55 | 7,20 | 0,23 | 6,21 |
| FG ravnomjerno | **0,35** | 2,54 | 3,37 | 4,24 | 3,24 | 3,41 |

![histogram](../captures/pacing/histogram-L12.png)
![vremenski slijed](../captures/pacing/timeline-L12.png)

Bez pacinga generirani i pravi okvir stižu gotovo istovremeno (0,23 ms), a
onda čekaju 6,2 ms do sljedećeg para — bimodalno, kao i na sporijem GPU-u.
S pacingom su dva naizmjenična razmaka gotovo jednaka (3,24 i 3,41 ms) i std
je 0,35 ms — manji nego bez generiranja uopće (0,54 ms), jer nit prikaza
svaki drugi okvir drži na vremenskoj osi neovisno o tome kako je render nit
rasporedila svoj posao. Isto vrijedi na svim opterećenjima: std 0,18–0,58 ms
ravnomjerno naspram 3,0–4,8 ms odmah.

### Latencija

Srednja vrijednost, ms, od uzorkovanja ulaza do swapa (vidi gore što to ne
uključuje).

| opterećenje | bez FG | FG odmah: pravi | FG ravnomjerno: prvi odziv (generirani) | FG ravnomjerno: pravi | cijena pacinga (pravi − bez FG) |
|---|---:|---:|---:|---:|---:|
| ×0 | 1,22 | 2,07 | 1,63 | 2,45 | +1,2 |
| ×4 | 2,98 | 3,83 | 3,58 | 5,32 | +2,3 |
| ×8 | 4,47 | 5,34 | 5,24 | 7,81 | +3,3 |
| ×12 | 5,66 | 6,58 | 6,54 | 9,79 | +4,1 |
| ×16 | 6,99 | 7,68 | 7,68 | 11,50 | +4,5 |
| ×24 | 9,23 | 10,13 | 10,44 | 15,64 | +6,4 |

Latencija pravog okvira uz ravnomjerni prikaz raste s opterećenjem, ali
umjereno i predvidivo (+1,2 ms uz ×0 do +6,4 ms uz ×24), dosljedno preko
cijelog raspona. Prvi odziv — generirani okvir, koji već nosi pola gibanja
iz novog ulaza — stiže na ekran kasnije nego bez generiranja uopće:
generiranje, dakle, ne smanjuje latenciju ni u najpovoljnijem čitanju, nego
kupuje glatkoću po cijeni odziva, i to je poštena formulacija rezultata.

### Vsync (monitor bez fiksne granice u ovom mjerenju)

| redak | prikazano fps | std | p99 | latencija pravog |
|---|---:|---:|---:|---:|
| ×12 bez FG | 182,6 | 0,54 | 6,59 | 5,68 |
| ×12 FG ravnomjerno | 300,5 | 0,38 | 4,26 | 9,81 |
| ×24 bez FG | 110,6 | 0,96 | 10,61 | 9,24 |
| ×24 FG ravnomjerno | 189,2 | 0,57 | 6,46 | 15,70 |

Uz ×12 generiranje podigne 182,6 na 300,5 fps (1,64×) uz malo rasipanje na
oba retka. Uz ×24 podigne 110,6 na 189,2 fps (1,71×). Pod Windows
kompozitorom (DWM) prikaz bez vsynca ionako mjeri stvarna vremena swapa, ne
fotone — isto ograničenje kao i ranije pod Waylandom/Hyprlandom, samo na
drugom stogu.

### Alternative stagingu (×12)

| redak | prikazano fps | std | p99 | latencija pravog |
|---|---:|---:|---:|---:|
| staging (zadano) | 300,8 | 0,35 | 4,24 | 9,79 |
| staging + `glFlush` po fazi | 323,5 | 0,36 | 4,20 | 9,10 |
| staging + `glFinish` po fazi | 259,9 | 0,32 | 4,65 | 11,39 |

`glFinish` po fazi drži najmanje rasipanje ali gubi FPS (259,9 naspram 300,8),
kao i na sporijem GPU-u. `glFlush` uz staging izgleda nešto brži od zadanog
(323,5 naspram 300,8), no razlika je unutar raspona koji isti redak pokazuje
između ponovljenih pokretanja, pa se ne proglašava rezultatom. Usporedba
"bez staginga" (blit neposredno prije swapa) iz ranijeg razvoja više nije
dostupna kao zastavica u trenutnom kodu — staging je jedina implementacija —
pa se ne može ponovno izmjeriti; obrazloženje zašto staging postoji ostaje
ono iz arhitekture gore.

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

