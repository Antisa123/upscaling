# Plan rada: Upscaling + generiranje okvira u stvarnom vremenu (FSR3-lite)

Status: prijedlog plana, v1 (2026-09-11)

---

## 0. Mapiranje zahtjeva iz opisa teme

| Zahtjev iz opisa | Gdje se ispunjava |
|---|---|
| "kombinira upscaling i generiranje interpoliranih okvira" | Modul B (upscaler) + Modul D (frame generation), spojeni u jedan pipeline |
| "napredne metode računalnog vida" | Modul C: piramidalni optical flow (block matching + median filtriranje), temporalna reprojekcija, detekcija disokluzije |
| "umjetne inteligencije" | Modul E: naučeni modul (mali CNN) + poglavlje usporedbe s DLSS / FSR4 Redstone |
| "analizira korisničke unose, podatke o sceni i kretanju" | Modul A: kamera + jitter + G-buffer (dubina, motion vektori, reactive maska), mjerenje latencije unosa |
| "implementacija ključnih tehnika iz AMD FSR3" | Svi passevi FSR3 upscalera i frame interpolacije implementirani od nule (Modul B, C, D) |
| "povećati broj sličica u sekundi" | Modul F: mjerenje FPS-a, frame time histogrami, 1% low |
| "poboljšati percepcijsku kvalitetu" | Modul F: PSNR/SSIM/FLIP/LPIPS naspram ground-truth rendera |
| "optimizirati performanse u aplikacijama u stvarnom vremenu" | Modul F: profiliranje po passu (GPU timestamp queries), analiza latencije |

**Otvoreno prema mentoru:** Excel na SharePointu je zaključan (HTTP 403) — treba nam njegov sadržaj da provjerimo formalne zahtjeve (opseg, rok, forma predaje, je li rad individualan).

---

## 1. Odluke

| Odluka | Izbor | Obrazloženje |
|---|---|---|
| Jezik / API | C++20 + OpenGL 4.6 compute shaderi (GLSL 460) | Sve potrebne značajke postoje: `imageAtomicMax` nad `r32ui` (nužno za splatting polja motion vektora), `GL_KHR_shader_subgroup`, timestamp queries, SSBO. Bitno manje infrastrukture nego Vulkan → vrijeme ide u algoritam, a ne u sinkronizaciju. |
| GPU / meta | Radeon RX 580 (Polaris, Mesa) | Render 960×540 ili 1280×720 → izlaz 1920×1080. 4K nije realan cilj na ovom GPU-u. |
| Scena | vlastiti mali renderer + glTF scena (Sponza ili Bistro) | Nužno: bez kontrole nad jitterom, dubinom i motion vektorima temporalni upscaling ne postoji. Gotov engine bi to sakrio. |
| Referentne implementacije | čitaju se, ne kopiraju | FidelityFX passevi su u API-neutralnim headerima i služe kao pseudokod; naša implementacija je vlastita i to mora biti eksplicitno navedeno u radu. |
| ML | mali naučeni modul + poglavlje analize | Pravi FSR3 je klasičan CV; ML se traži opisom teme, pa ide kao mjerljiv eksperiment (Modul E), a ne kao cijela jezgra — RX 580 nema matrix jedinice ni ROCm podršku. |

**Ključna prednost postavke:** vlastiti renderer može renderirati *ground truth* — i puni native okvir (referenca za upscaler) i pravi među-okvir na t+0.5 (referenca za interpolaciju). Time kvaliteta postaje mjerljiva brojkom, a ne dojmom. To je najjači dio rada i većina sličnih radova to nema.

---

## 2. Arhitektura sustava

```
                 ┌──────────────── Modul A: test-bed renderer ────────────────┐
   ulaz (miš/    │  kamera (jitter Halton 2,3) → G-buffer @ render rez.       │
   tipkovnica) → │  color HDR │ depth (reverse-Z) │ velocity (UV) │ reactive  │
                 └───────┬─────────────────────────────┬──────────────────────┘
                         │                             │
          ┌──────────────▼───────────────┐   ┌─────────▼───────────────────┐
          │ Modul B: temporalni upscaler │   │ Modul C: optical flow       │
          │ 6 passeva, render→display    │   │ piramidalni block matching  │
          └──────────────┬───────────────┘   └─────────┬───────────────────┘
                         │ upscaled(t-1), upscaled(t)  │ OF vektori
                         └──────────────┬──────────────┘
                                        ▼
                     ┌──────────────────────────────────────┐
                     │ Modul D: frame generation (9 passeva)│
                     │ → interpolirani okvir na t-0.5       │
                     └──────────────────┬───────────────────┘
                                        ▼
                        UI kompozicija → pacing (nit prikaza) → present
                                        │
                     ┌──────────────────▼───────────────────┐
                     │ Modul F: mjerenje (metrike, timing)  │
                     └──────────────────────────────────────┘
```

---

## 3. Modul A — Test-bed renderer

Cilj: minimalan, ali korektan izvor ulaza za oba algoritma.

- Forward ili jednostavan deferred prolaz, glTF učitavanje (`cgltf` / `tinygltf`), osnovni PBR — vizualna vjernost nije cilj, ali scena mora imati **tanku geometriju, teksturni detalj visoke frekvencije i pokretne objekte** jer se tu vide artefakti.
- **Jitter kamere:** Halton(2,3) niz, broj faza `8 * (displayW/renderW)^2`, offset u rasponu `[-0.5, 0.5]` piksela render rezolucije, ugrađen u projekcijsku matricu kao `(2*jx/renderW, -2*jy/renderH)`.
- **Motion vektori:** u UV prostoru, smjer *trenutni → prethodni*, po pikselu; za pokretne objekte koristi se prethodna model matrica i prethodni view-projection. Jitter se dosljedno uklanja (ili dosljedno zadržava) — nedosljednost je klasičan uzrok "razmazane" slike.
- **Reverse-Z dubina** + beskonačni far plane (bolja preciznost, ista konvencija kao FSR).
- **Reactive / transparency maska:** označava piksele gdje je povijest nepouzdana (prozirni objekti, čestice, animirane teksture).
- **Skriptirana putanja kamere** s deterministički reproduciranim korakom — bez toga mjerenja nisu usporediva.
- **Ground-truth način rada:** offline prolaz koji sprema (a) native render na display rezoluciji bez jittera, (b) render scene točno na `t - 0.5` koraka, kao referencu za interpolirani okvir.
- Dva ekskluzivna moda vremena: *realtime* (mjerenje FPS-a/latencije) i *lockstep* (fiksni `dt`, snimanje okvira za metrike).

Definicija gotovog: scena se vrti na ≥120 FPS na 1080p native, dumpanje ground-truth sekvence radi, velocity buffer vizualno provjeren (debug prikaz vektora).

---

## 4. Modul B — Temporalni upscaler (FSR3-stil)

Šest compute passeva; implementira se **postupno**, svaki korak je mjerljiv:

1. **Luminance pyramid** — luma na 50% render rezolucije + mip lanac (SPD ili obični mip chain), iz najnižeg mipa auto-ekspozicija.
2. **Reconstruct & dilate** — dilatacija dubine i motion vektora po najbližoj dubini u 3×3 okolini; rekonstrukcija prethodne dubine reprojekcijom (atomic min/max u reverse-Z).
3. **Depth clip** — usporedba rekonstruirane prethodne dubine i trenutne → **disocclusion maska**.
4. **Create locks** — detekcija tankih detalja (luma kontrast u 3×3) i postavljanje "zaključanog" piksela koji se izuzima iz clampanja da detalj ne nestane.
5. **Reproject & accumulate** (najskuplji pass, na display rezoluciji):
   - upsample trenutnog okvira Lanczos(x,2) kernelom (konceptualno 5×5, stvarno se uzorkuje 4×4; sinc preko LUT-a),
   - reprojekcija prethodnog izlaza i lock statusa po dilatiranim MV-ima,
   - YCoCg bounding box iz iste 5×5 okoline → **color rectification** (clamp povijesti na box), oslabljen po disocclusion maski i lockovima,
   - akumulacija s malim udjelom trenutnog okvira, povećanim ondje gdje reactive maska to traži.
   - Interni format: reverzibilni tonemap `c/(1+max(c))` i inverz, prostor YCoCg.
6. **RCAS** — izoštravanje na display rezoluciji, `sharpness ≈ 0.2–0.5`.

Uz to, **baseline-i za usporedbu** (jeftini, a nose cijelo poglavlje rezultata): bilinear, bicubic (Catmull-Rom), FSR1 (EASU+RCAS, prostorni upscaling bez povijesti), te naivni TAAU bez lockova/disokluzije.

Mip bias za teksture: `log2(render/display) - 1.0`.

Režimi skaliranja (isti nazivi kao FSR, radi usporedivosti): Quality 1.5×, Balanced 1.7×, Performance 2.0×, Ultra Performance 3.0×.

---

## 5. Modul C — Optical flow (računalni vid)

Piramidalni block matching, po uzoru na `FfxOpticalFlow` — **ovo je jezgra "računalnog vida" u radu**:

- Priprema: luma resurs + piramida od 7 razina (SPD), ping-pong s prethodnim okvirom.
- **Detekcija promjene scene:** histogram luminancije u 9 sekcija, usporedba s prethodnim okvirom; iznad praga se vraća nulto gibanje (sprječava nasumične vektore na rezu scene).
- Glavna petlja, 7 iteracija × 3 passa:
  1. **Search** — za svaki nepreklapajući 8×8 blok, pretraga prozora 24×24 (±8 px, 256 kandidata), kriterij minimalni SAD luminancije; rezultat je korekcija na vektor iz prethodne razine.
  2. **Filter** — 3×3 medijan (vektor s minimalnom sumom udaljenosti do ostalih) uklanja outliere.
  3. **Upscale** — 2× (vektor ×2) uz izbor između 4 kandidata po SAD-u na 4×4 blokovima; ne izvodi se u zadnjoj iteraciji.

**Odstupanja implementacije od ovog nacrta (izmjerena, `docs/OPTICALFLOW.md`):**
vektori se nose u UV prostoru pune slike na svakoj razini, pa je „upscale ×2”
identiteta i prolaz radi samo izbor kandidata (roditelj + tri susjeda, polje
prošlog okvira, nula); radijus pretrage je 4 texela umjesto 8, jer je od 4 do 8
točnost bolja za 9 % uz 46 % skuplji modul; presuda detektora reza uzima
**srednju** od devet udaljenosti sekcija, a ne maksimum — maksimum ne razlikuje
brzi zaokret od reza i sam sebi pokvari točnost za 2,4×.

Teorijska podloga za pisani dio: Fleet & Weiss, *Optical Flow Estimation* (gradijentne metode, Lucas-Kanade, coarse-to-fine piramide) — objašnjava **zašto** block matching s piramidom rješava problem velikih pomaka i aperturni problem. Opcijski dodatak za poglavlje usporedbe: implementacija Lucas-Kanade varijante i usporedba točnosti/brzine s block matchingom.

---

## 6. Modul D — Generiranje okvira

Devet passeva, ulaz su dva uzastopna *upscalana* okvira + game motion vektori + optical flow:

1. **Setup** — čišćenje brojača i polja (koriste se atomici).
2. **Estimate interpolated frame depth** — reprojekcija dilatirane dubine za *pola* duljine motion vektora → dubina među-okvira.
3. **Compute game motion vector field** — splatting MV-a u referentni okvir među-slike; MV u donjih 16 bita 32-bitnog UINT-a, u gornjih 16 prioritet: 1 bit primary/secondary, 10 bita po udaljenosti od kamere, 5 bita po sličnosti boje; upis preko `imageAtomicMax`. Polje ostaje rupičasto — to je očekivano.
4. **Game MV field inpainting pyramid** — mip lanac polja (SPD); `.xy` vektori, `.z` prioritet po dubini, `.w` prioritet po boji; pri redukciji se bira najbliži vektor u 2D i najdalji po dubini.
5. **Compute optical flow vector field** — isto, ali iz OF vektora; prioritet po magnitudi vektora i sličnosti reprojicirane boje.
6. **Compute disocclusion mask** — **dvije** maske: među-okvir↔prethodni i među-okvir↔trenutni; određuju koji je smjer reprojekcije neupotrebljiv.
7. **Compute interpolation** — dvostupanjski blend: prvo boja iz dva warpa po game MV-ima težinski po disocclusion maskama, zatim boja po optical flow vektorima; njihov omjer određuje sličnost boja (bolje poklapanje pobjeđuje). Prvi okvir nakon reseta = kopija zadnjeg backbuffera.
8. **Compute inpainting pyramid** — mip lanac interpolirane slike koji ignorira rupe (potreban kanal pokrivenosti/težine).
9. **Inpainting** — popunjavanje preostalih rupa iz piramide + čišćenje UI-ja.

Uz to:
- **Frame pacing:** red okvira u kojem se interpolirani okvir prikazuje *prije* stvarnog, s ravnomjernim razmakom; bez toga FPS raste, a glatkoća ne. Mjeri se stvarna raspodjela vremena prikaza.
- **UI:** HUD se komponira **nakon** interpolacije (inače se razmazuje po ekranu); implementirati i debug način s "tear lines" trakama koje pokazuju prikazuje li se interpolirani okvir.
- **Latencija:** frame generation povećava latenciju za ~1 render okvir — to se mora izmjeriti i pošteno prikazati, jer je to glavna kritika ove tehnologije.

**Odstupanja implementacije od ovog nacrta (M7, `docs/FRAMEGEN.md`):**
setup je jedan compute prolaz koji piše vrijednosti „prazno” u svih pet ciljeva
scattera, a ne brojače; polja vektora i dubina međuokvira su u render
razlučivosti (jedan izvorni piksel po texelu cilja), interpolacija u prikaznoj;
x i y komponenta idu u dvije `r32ui` slike s istim prioritetom (16 bita
prioriteta + 16-bitni float), kao u `FfxFrameInterpolation`; u polju toka
nema dubine, pa je gornji bit „siguran blok” (pogreška podudaranja ispod praga)
umjesto primarnog; piramida polja pri redukciji zadržava **najniži** prioritet
(pozadinu), jer su rupe koje popunjava disokluzije; maske se računaju iz dubine
međuokvira naspram dubinskih bufera t−1 i t. Prolazi 8–9 (inpainting slike) su M8.

**Odstupanja implementacije (M8, `docs/PACING.md`, `docs/FRAMEGEN.md`):**
UI se komponira na render niti prije predaje okvira (u oba okvira para), a ne
nakon reda za prikaz — nit prikaza samo kopira i swapa. Prikaz ima vlastitu nit
i GL kontekst koji dijeli teksture; predaja ide preko `glFinish`, jer
`glFenceSync` na Mesi 26.2 ne radi. Oba okvira para crtaju se u back buffer
odmah pri predaji („staging”), a u pravom trenutku ostaje samo swap: dva
konteksta na istom GPU-u ne mogu preskočiti red poslova, pa je blit u trenutku
prikaza čekao 3–6 ms iza render niti. Prolazi 8–9 rade nad pokrivenošću
(alfa iz prolaza 7) umjesto nad posebnom maskom rupa; „čišćenje UI-ja” u
prolazu 9 nije potrebno jer se UI nikad ne nalazi u ulazu generiranja, osim u
namjerno lošem `--ui baked` načinu koji služi kao usporedba. Cijena igre
simulira se sintetskim opterećenjem (`--load`), jer je Sponza lakša od praga
isplativosti.

---

## 7. Modul E — Naučena (AI) komponenta

Cilj: dokazati razumijevanje razlike između analitičkog (FSR3) i naučenog (DLSS, FSR4 Redstone) pristupa, uz vlastiti mjerljiv eksperiment.

- **Dataset:** generira ga naš renderer — parovi (ulazni podaci, ground truth) iz skriptiranih putanja kamere, nekoliko tisuća okvira.
- **Model:** mali CNN (3–5 konvolucijskih slojeva, U-Net-lite) za jedan od ova dva zadatka:
  - (a) **blend/inpainting maska** za frame generation — mreža predviđa težine spajanja dvaju warpova umjesto ručne heuristike; ili
  - (b) **naučeni upsampling kernel** koji zamjenjuje Lanczos + akumulaciju.
  Preporuka: **(a)** — manji model, jasno izolirana usporedba s heuristikom, manji rizik.
- **Trening:** offline, PyTorch, na vanjskom GPU-u (Colab/fakultetski stroj) — lokalni RX 580 nema ROCm podršku.
- **Inferencija:** izvoz težina → ručno pisani compute shader (mali model je izvediv u GLSL-u), ili, ako ne stane u budžet, offline inferencija i usporedba samo po kvaliteti + procjena troška.
- **Poglavlje analize:** zašto ML pristupi traže matrix/tensor jedinice, što FSR4 Redstone donosi i uz koje hardverske uvjete — `docs/ML_KOMERCIJALNO.md`.

**Odstupanja implementacije (M9, `docs/ML.md`):**
- **Trening lokalno, vlastitim trenerom** (`tools/fg_train`, C++20 + OpenMP, CPU) umjesto PyTorcha na vanjskom GPU-u: projekt nema Python ovisnosti osim Pillowa, a model od nekoliko tisuća parametara uči se za ~8 min. Trener i shader provjeravaju se jedan naspram drugog na snimkama s izlazom shadera.
- **Mreža ne predviđa masku dvaju warpova nego softmax težine nad sedam kandidata** (izlaz heuristike, četiri warpa, dva nepomaknuta okvira), inicijalizirane na heuristiku.
- **Inferencija je u GLSL-u na punoj rezoluciji**, ne offline; raspored prolaza optimiran izmjereno (4,18 → 2,75 ms bez promjene funkcije).
- **Skup: 18 putanja za učenje + 4 validacijske, podjela po putanji;** mjerni pogledi nikad nisu snimljeni. Izbor modela radi se na cijelim okvirima validacijskih putanja, jer validacija na patchevima nije otkrila grešku maske na rubu okvira (−4 dB).
- Usput popravljen NaN u povijesti FSR upscalera (dijeljenje s w = 0 u `gbuffer.frag` na prvom okviru); standardna mjerenja bit-identična.
- **Demonstracija:** `scripts/ml_demo.sh` (cijeli postupak na malom primjeru, ~1 min) i `scripts/ml_gallery.py` (slike za rad iz spremljenih fg-ml mjerenja) — vidi odjeljak „Demonstracija" u `docs/ML.md`.

---

## 8. Modul F — Evaluacija

**Kvaliteta (naspram ground trutha iz Modula A):**
- Upscaling: PSNR, SSIM, FLIP, LPIPS (offline) — naspram native rendera na display rezoluciji.
- Interpolacija: iste metrike naspram **stvarno renderiranog među-okvira** na t-0.5.
- Temporalne metrike: pogreška warpanja između uzastopnih okvira, indeks treperenja (flicker) — obična SSIM ne vidi judder ni treperenje.
- Galerija artefakata s uvećanim izrezima: disokluzija iza stupa, prozirnost/čestice, HUD, brzo okretanje kamere, tanke žice, šahovnica.

**Performanse:**
- GPU timestamp queries **po passu** → tablica troška svakog passa (ovo je ono što rad čini inženjerskim, a ne demo-om).
- Frame time histogram, 1% low, FPS pri Quality/Balanced/Performance/Ultra Performance.
- Potrošnja memorije po resursu.

**Latencija:**
- Vrijeme od uzorkovanja unosa do prikaza okvira koji ga sadrži; usporedba native / upscale / upscale+FG.

**Ablacijske studije** (svaka je po jedno potpoglavlje rezultata): bez lockova, bez disocclusion maske, bez optical flowa (samo game MV), bez inpaintinga, bez RCAS-a, bez reactive maske.

---

## 9. Milestones

| # | Faza | Trajanje | Gotovo kad | Status |
|---|---|---|---|---|
| M0 | Postavljanje: repo, CMake, GL kontekst, debug output, RenderDoc | 1 tj | Prazan prozor + compute "hello world" radi | ✅ gotovo |
| M1 | Renderer: glTF, kamera, G-buffer, jitter, motion vektori, reverse-Z | 3 tj | Debug prikaz MV-a korektan; ground-truth dump radi | ✅ gotovo |
| M2 | Metrički harness: PSNR/SSIM/FLIP skripte, skriptirana kamera, timestamp queries | 1 tj | Tablica metrika se generira jednom naredbom | ✅ gotovo |
| M3 | Baseline upscaleri: bilinear, bicubic, FSR1 EASU+RCAS | 1 tj | Prve brojke u tablici | ✅ gotovo |
| M4 | Minimalni TAAU: reprojekcija + akumulacija + clamp | 2 tj | Slika stabilna, vidljiv dobitak PSNR-a nad bicubic | ✅ gotovo (+4,56 dB na mirnoj kameri, bolji SSIM na svim brzinama — `docs/TAAU.md`) |
| M5 | Puni upscaler: dilate, depth clip, lockovi, luma piramida, reactive, RCAS | 3 tj | Sva 4 režima skaliranja rade, ablacije mjerljive | ✅ gotovo (+0,70 dB nad M4 na mirnoj kameri; luma piramida odgođena u M6 jer je ekspozicija fiksirana prije upscalera — `docs/FSR.md`) |
| M6 | Optical flow: piramida, search/filter/upscale, detekcija promjene scene | 3 tj | Debug vizualizacija toka (HSV) izgleda ispravno | ✅ gotovo (EPE 2,8 px pri 120 fps i 0,0 px na mirnoj kameri naspram 23,0 px bez procjene; 0,29 ms na 1080p na NVIDIA RTX 5070 — `docs/OPTICALFLOW.md`) |
| M7 | Frame generation jezgra: passevi 1–7 | 3 tj | Interpolirani okvir postoji i mjerljiv je naspram ground trutha | ✅ gotovo (34,9 dB naspram 26,1 dB 50/50 blenda na 1080p Quality pri 60 fps; mirna kamera 40,14 dB = blend; 0,24 ms + 0,24 ms optical flow na RTX 5070 — `docs/FRAMEGEN.md`) |
| M8 | Inpainting, UI kompozicija, frame pacing, latencija | 2 tj | FPS raste, frame time ravnomjeran, HUD čist | ✅ gotovo, uz otvoren nalaz na brzom GPU-u: do ×8 opterećenja prikazani FPS raste 1,14–1,56×; procjena render perioda u frame pacer-u bila je samoreferentna na dovoljno brzom GPU-u, pa ×12+ i dalje degenerira u nepravilan ritam (popravljeno djelomično, ostaje otvoreno) — HUD 52,0 dB komponiran naspram 20,0 dB upečen; inpainting slike ±0,1 dB — `docs/PACING.md` |
| M9 | ML modul: dataset, trening, integracija, usporedba | 2 tj | Usporedna tablica heuristika vs. naučeni blend | ✅ gotovo (mreža 7 199 parametara u compute shaderima; SSIM viši na svih 8 mjernih pogleda, PSNR na 7 od 8, +0,03 do +0,20 dB, stabilno preko 3 sjemena; najgori okvir i neviđena scena bez dobitka; cijena +1,1 ms na 1080p na RTX 5070 (+2,7 ms na RX 580); ni na kartici s Tensor Core jedinicama se ne isplati jer ih implementacija namjerno ne koristi — `docs/ML.md`) |
| M10 | Mjerenja, grafovi, pisanje rada | 3 tj | Sva poglavlja + reproducibilni rezultati | — |

Ukupno ≈ 24 tjedna uz sekvencijalni rad. Kritični put je M1 → M4 → M7; M6 (optical flow) je neovisan i može teći paralelno ako radi dvoje.

**Ako rok bude kraći**, prvo padaju: Ultra Performance režim, LPIPS, naučeni upsampler (ostaje samo naučeni blend), dio ablacija. **Ne smiju pasti:** ground-truth mjerenje, po-pass profiliranje, mjerenje latencije — to su nosivi rezultati rada.

---

## 10. Rizici

| Rizik | Mitigacija |
|---|---|
| Polaris nema brzu FP16 aritmetiku | Sve u FP32; FP16 put samo kao opcionalna optimizacija (FSR ionako ima FP32 fallback) |
| Krivi motion vektori = sve izgleda pokvareno | Prvo debug vizualizacija MV-a i reprojekcije; ne kretati na M4 bez toga |
| Frame pacing pod X11/Waylandom | Mjeriti stvarna vremena prikaza; benchmark s isključenim vsyncom, glatkoća mjerena zasebno. **Ostvaren (M8):** mjeri se vrijeme swapa, ne fotona; vsync-off okvire iznad 100 Hz kompozitor ne prikaže, pa je dodan i vsync redak |
| Atomici u MV field passu spori | Profilirati rano; alternativa je scatter preko storage buffera i sortiranje |
| ML dio "pojede" vrijeme | Strogo ograničen na blend mrežu; ako klizne, ostaje poglavlje analize + offline rezultat |
| Prevelik opseg | Svaki milestone daje upotrebljiv rezultat za rad i sam po sebi — rad je obranjiv i ako zadnja dva otpadnu |

---

## 11. Struktura repozitorija

```
ante-galic/
├── src/
│   ├── app/          # prozor, ulaz, petlja, mjerenje vremena
│   ├── renderer/     # glTF, kamera, jitter, G-buffer
│   ├── upscale/      # Modul B
│   ├── opticalflow/  # Modul C
│   ├── framegen/     # Modul D
│   └── metrics/      # timestamp queries, dump okvira
├── shaders/          # GLSL 460 compute
├── assets/           # scene
├── tools/            # Python: PSNR/SSIM/FLIP, grafovi, trening ML modula
├── captures/         # ground truth i rezultati
├── docs/
│   ├── reference/    # preuzeta FSR3 dokumentacija (lokalno)
│   ├── METRICS.md    # kako se generira tablica metrika
│   ├── GROUND_TRUTH.md  # kako se snima i provjerava referenca
│   ├── UPSCALING.md  # M3: prostorni baseline upscaleri
│   ├── TAAU.md       # M4: minimalni temporalni upscaler
│   ├── FSR.md        # M5: puni upscaler
│   ├── OPTICALFLOW.md   # M6: procjena gibanja iz slike
│   ├── FRAMEGEN.md   # M7: generiranje međuokvira
│   └── thesis/       # tekst rada
└── PLAN.md
```

---

## 12. Struktura pisanog rada

1. Uvod: problem — rezolucija i frekvencija osvježavanja rastu brže od rasterizacijske snage
2. Teorijska podloga: uzorkovanje i aliasing, temporalna akumulacija, procjena gibanja i optical flow (Fleet), reprojekcija i disokluzija
3. Pregled postojećih rješenja: FSR1/2/3, FSR4 Redstone, DLSS, XeSS, RIFE/FILM (ML interpolacija) — usporedna tablica pristupa
4. Arhitektura vlastitog sustava
5. Implementacija upscalera (pass po pass)
6. Implementacija procjene gibanja
7. Implementacija generiranja okvira
8. Naučena komponenta
9. Metodologija mjerenja (ground truth je ovdje glavni argument)
10. Rezultati: kvaliteta, performanse, latencija, ablacije, artefakti
11. Rasprava i ograničenja
12. Zaključak i mogući nastavak

**Literatura (jezgra):** FSR3 dokumentacija (interpolation, optical flow, super-resolution), GPUOpen FSR članci, Fleet & Weiss *Optical Flow Estimation*, Karis *High Quality Temporal Supersampling* (TAA), Yang et al. *A Survey of Temporal Antialiasing Techniques*, RIFE/FILM radovi, radovi o metrikama za interpolaciju okvira.

---

## 13. Što trebamo prije početka

1. Sadržaj zaključanog Excela (formalni zahtjevi, rok, je li rad timski).
2. Potvrda mentora traži li stvarnu ML implementaciju ili je dovoljna analiza — plan je napisan tako da se ML modul može dodati bez dirati jezgru.
3. Odabir scene (Sponza je dovoljna; Bistro je zahtjevniji, ali ima bolji materijal za artefakte).
