# Poglavlje 3 — komercijalna i istraživačka rješenja

Pregled postojećih rješenja za upscaling i generiranje okvira, da se ovaj rad
(FSR3-lite iz nule, RX 580, bez matričnih/tenzorskih jedinica) postavi u
kontekst. Izvor su bilješke
[`docs/reference/ml_commercial_notes.md`](reference/ml_commercial_notes.md)
(engleski, s izvorima po tvrdnji). Gdje bilješke tvrdnju obilježavaju kao
**nepotvrđenu** (samo sekundarni izvor ili nemoguće provjeriti), to je ovdje
tako i preneseno — nepotvrđene tvrdnje se ne iznose kao činjenice.

## NVIDIA DLSS

DLSS 2 (ožujak 2020) uvodi temporalnu mrežu: ulazi su boja niske rezolucije,
igrini vektori gibanja i prethodni izlaz visoke rezolucije, obrađeni
konvolucijskim autoenkoderom na Tensor jedinicama, učenim naspram 16K
offline-renderirane referentne slike. Ne objavljuje se izravno predviđa li
mreža boju ili parametre mješavine (nepotvrđeno).

DLSS 3 (rujan 2022) generiranje okvira dodaje kao zaseban dio: ulazi su dva
okvira, hardverski optical flow (Ada Optical Flow Accelerator) i igrini
podaci (vektori, dubina). Optical flow hvata odraze, sjene i čestice koje
igrini vektori ne pokrivaju. Zahtijeva RTX 40. DLSS 4 (siječanj 2025) mijenja
mrežu za super-rezoluciju s CNN-a na transformer i seli generiranje okvira s
hardverskog optical flowa na naučeni model; „Multi Frame Generation" (do 3
dodatna okvira) ograničen je na RTX 50. DLSS 4.5 (2026) dodaje drugu
generaciju transformera i FP8 na RTX 40/50 — RTX 20/30 nemaju izvornu FP8
podršku, pa plaćaju veću cijenu. Streamline integracija traži dubinu, gusta
polja vektora i HUD-less/UI međuspremnik.

Reflex 2 Frame Warp (najavljen siječanj 2025) je iznimka: kasna reprojekcija
(ekstrapolacija) koja iz zadnjeg unosa i zadnjeg renderiranog okvira izračuna
novu kameru i warpa okvir neposredno prije prikaza, s predviđajućim
popunjavanjem rupa disokluzije. Je li to popunjavanje naučeno — nepotvrđeno.
Do srpnja 2026. nije isporučen ni u jednoj igri niti kroz driver (sekundarni
izvor).

## AMD FSR

FSR 1 je čisto prostorni, klasičan (EASU + RCAS) — isti par tehnika kao M3
ovog rada. FSR 2 je temporalan i klasičan (ne traži ML): ulazi su boja,
dubina, vektori, maske i jitter, kroz piramidu luminancije, rekonstrukciju i
dilataciju, depth-clip, lockove, reprojekciju s Lanczos akumulacijom i RCAS —
struktura koju M4/M5 ovog rada slijede.

**FSR 3/3.1 je klasičan i u dijelu za generiranje okvira.** Njegov optical
flow (temelj: AMD Fluid Motion Frames) radi po blokovima 8×8 bez preklapanja,
jedan vektor po bloku, 6-razinska piramida luminancije, 7 iteracija SAD
pretrage (prozor 24×24, `msad4`), medijan filter 3×3 i uzorkovanje na gornju
rezoluciju. To odgovara M6 ovog rada (piramidalni block matching), samo je
naš pristup gušći (vektor po pikselu, ne po bloku 8×8). Nema izjave o
Polarisu (RX 400/500, hardver ovog rada) — na njemu vrijede samo klasični
FSR 1/2/3.

FSR 4 (veljača 2025) je naučeni upscaler na FP8 matričnim jedinicama RDNA 4
(WMMA), isprva samo na RX 9000; FSR SDK 2.3 (lipanj 2026) prenosi ga na
RX 7000 kroz kvantizirani INT8 model. FSR „Redstone" (prosinac 2025) dodaje
naučeno generiranje okvira: mreža predviđa gibanje i izgled iz prethodnog i
trenutnog okvira, dubine i vektora, „optičkim tokom plus vektorima", pa se to
miješa s klasičnom reprojekcijom vektora — najbliže od AMD-ovih rješenja
onome što M9 ovog rada radi (naučeno pravilo miješanja preko klasičnih
kandidata), samo na hardveru koji RX 580 nema. Za RDNA 2 najavljeno je
„rano 2027." (sekundarni izvor, nepotvrđeno).

## Intel XeSS

XeSS-SR ima dva puta: XMX (Arc/Iris Xe) i lakši, sporiji DP4a put za bilo
koji GPU sa Shader Model 6.4+. XeSS 2 (prosinac 2024) je najbliži M9 ovog
rada od svih komercijalnih rješenja: tri stupnja — (1) reprojekcija igrinim
vektorima, (2) naučeni model za reprojekciju optičkim tokom (hvata sjene,
odraze, čestice), (3) **drugi naučeni model s funkcijom mješavine koji bira
između oba kandidata**. To je gotovo doslovno M9-ova arhitektura (mreža ne
crta piksele, bira/miješa kandidate), samo s naučenim, ne klasičnim, tokom
kao jednim od kandidata, i na hardveru s XMX-om. Whitepaper tvrdi da FG treba
Arc GPU s XMX-om; SDK README istovremeno navodi rad na „ne-Intel GPU-ima sa
SM 6.4" — bilješke to označavaju kao proturječje (nepotvrđeno je koje
vrijedi). XeSS 3 (2026) dodaje Multi Frame Generation, ograničen na Intel
hardver.

## Apple, Sony, mobilni

Apple MetalFX ima temporalni ML upscaler od 2022. i interpolator okvira od
2025. (`MTLFXFrameInterpolator`: dva okvira, vektori, dubina); je li sâm
interpolator naučen — nepotvrđeno. Sony PSSR (PS5 Pro, studeni 2024) je ML
upscaler na namjenskom ML hardveru (navedeno 300 TOPS INT8, sekundarni
izvor). Nadograđeni PSSR (veljača 2026) dolazi iz zajedničkog razvoja s AMD-om
(Project Amethyst) i, po Cernyjevoj izjavi novinarima (sekundarni citat),
koristi „isti model kao FSR Upscaling, treniran na drugim podacima" u INT8.
Generiranje okvira za PlayStation je najavljeno, ne u 2026.

Arm Neural Super Sampling (najavljen kolovoz 2025.) je konceptualno
najbliži analog cijelog M9 pristupa: 4-razinski UNet predviđa **filterske
jezgre 4×4 i temporalne koeficijente**, a klasični akumulator ih primjenjuje
— mreža opet ne crta piksele nego parametrizira klasični dio, kao i mreža
ovog rada. Arm ASR je, kao FSR 2, čisto klasičan. Qualcommov Snapdragon GSR
1/2 je klasičan (SGSR); "Adreno Neural Fusion" (najavljen za rujan 2026,
matrične jedinice u GPU-u za ML SR/FG) potvrđen je samo iz tiska.

## Istraživački modeli interpolacije (VFI)

RIFE, FILM, IFRNet, EMA-VFI (2022–2023) rade isključivo iz RGB parova okvira,
bez igrinih vektora, dubine ili UI odvajanja — moraju procijeniti cijelo
gibanje samo iz piksela. Cijena je desetci do stotine milisekundi na
vrhunskim GPU-ima (RIFE: 16 ms na 480p na TITAN X Pascal; FILM: 393 ms na
720p na V100; EMA-VFI: 25–132 ms na 512² na 2080Ti), naspram budžeta od ~8 ms
za cijeli skok 60→120 fps. RIFE ima 9,8 M parametara — 1361× više od mreže
ovog rada (7199). Trenirani su na prirodnom videu, ne na aliaseranim,
jitteriranim renderima s HUD-om, i nemaju integraciju s pacingom ili
Reflex-stilskim smanjenjem latencije. Zaključak bilješki: zato se ne koriste
izravno u igrama.

## Presjek

- **Hardver i preciznost.** Zajednički obrazac: inferencija je gusto
  matrično množenje u 8-bitnim formatima (FP8 na RTX 40/50 i RDNA 4; INT8 na
  RDNA 3 portu, XMX/DP4a na Intelu, 300 TOPS INT8 na PS5 Pro). Bez namjenske
  8-bitne podrške isti model košta znatno više — točno razlog zašto je FSR 4
  na RDNA 3 morao biti prekvantiziran u INT8, i točno razlog zašto M9 ovog
  rada mjeri granicu male mreže **bez** takvog hardvera.
- **Veličina mreže.** Skoro nitko ne objavljuje apsolutne brojke. NVIDIA
  daje samo relativne omjere (transformer = 2× parametara CNN-a). AMD, Intel,
  Sony i Apple ne objavljuju veličine. Arm NSS objavljuje arhitekturu
  (4-razinski UNet), ne veličinu. Mreža ovog rada (7199 parametara) je,
  koliko je dostupno provjeriti, redovima veličine manja od svega osim
  najmanjih VFI istraživačkih modela.
- **UI/HUD.** Svi komercijalni FG odvajaju UI od scene (DLSS-G: hudless + UI
  alfa/boja spremnik; FSR: callback, UI tekstura ili HUD-less
  auto-detekcija; XeSS-FG: ponavljanje ili re-render UI-a; MetalFX: tri
  režima) — isto što M8 ovog rada radi.
- **Interpolacija naspram ekstrapolacije.** Sva isporučena komercijalna
  rješenja (DLSS 3/4/4.5, FSR 3/Redstone, XeSS-FG/MFG, MetalFX) **interpoliraju**
  između dva već renderirana okvira, pa drže zadnji stvarni okvir — zato svako
  ide uz Reflex / Anti-Lag 2 / XeLL. Isto radi i M7/M8 ovog rada, uz izmjerenih
  +8–16 ms latencije (`docs/PACING.md`). Jedina objavljena ekstrapolacija,
  Reflex 2 Frame Warp, do srpnja 2026. nije isporučena.

## Usporedna tablica

| Rješenje | Proizvođač | Vrsta | Klasično / naučeno (što je naučeno) | Hardverski uvjet | Prvo izdanje |
|---|---|---|---|---|---|
| DLSS 2 | NVIDIA | SR | Naučena temporalna rekonstrukcija (CNN) | RTX 20+ (Tensor) | 2020 |
| DLSS 3 FG | NVIDIA | FG (2×) | Naučena sinteza; hardverski optical flow | RTX 40 | 2022 |
| DLSS 4 / 4.5 | NVIDIA | SR, RR, MFG do 6× | Transformer SR/RR; FG-tok preseljen na naučeni model | SR/RR sve RTX; MFG RTX 50 | 2025 / 2026 |
| Reflex 2 Frame Warp | NVIDIA | Kasna reprojekcija (ekstrapolacija) | Klasičan warp + predviđajuće popunjavanje (nepotvrđeno je li naučeno) | RTX 50 prvi | najavljen 2025, neisporučen do srp. 2026 |
| FSR 1 | AMD | SR | Klasičan (EASU+RCAS) | bilo koji GPU | 2021 |
| FSR 2 | AMD | SR | Klasičan temporalan | bilo koji GPU (SM6.2) | 2022 |
| FSR 3/3.1 | AMD | SR + FG | Klasičan; block-SAD optical flow 8×8 + reprojekcija vektora | bilo koji GPU (SM6.2) | 2023/2024 |
| FSR 4 | AMD | SR | Naučen (FP8 na RDNA 4, INT8 port na RDNA 3) | RX 9000; RX 7000 od lip. 2026 | 2025 |
| FSR Redstone FG | AMD | FG | Naučeno gibanje+izgled, miješano s reprojekcijom vektora | RX 9000, Win11 | 2025 |
| XeSS-SR | Intel | SR | Naučen temporalan; XMX naspram lakšeg DP4a | bilo koji SM6.4; najbolje na Arc XMX | 2022 |
| XeSS 2 FG / XeSS 3 MFG | Intel | FG (2×; do 4×) | Reprojekcija vektorima + naučeni optical flow + naučena mreža za mješavinu | Arc XMX; MFG samo Intel | 2024/2026 |
| MetalFX | Apple | SR, denoised SR, FG | Temporalni SR je naučen; interpolator nepotvrđeno | Apple silicij | 2022/2025 |
| PSSR / PSSR 2 | Sony | SR | Naučen; PSSR 2 = FSR4-obitelj model, INT8, vlastiti podaci | PS5 Pro | 2024/2026 |
| Arm ASR | Arm | SR | Klasičan (FSR2-izveden) | mobilni GPU-i | 2024 |
| Arm NSS | Arm | SR | Naučen UNet predviđa 4×4 jezgre + temporalne koeficijente za klasičan akumulator | Arm GPU s neural accel., 2026 | najavljen 2025 |
| Snapdragon GSR 1/2 | Qualcomm | SR | Klasičan | Adreno | 2023/2024 |
| RIFE / FILM / IFRNet / EMA-VFI | istraživanje | VFI | Naučen tok + fuzija, samo iz RGB parova | GPU, 16 ms–393 ms po okviru | 2022/2023 |

Puna tablica s izvorima (URL-ovi po retku) je u
[`docs/reference/ml_commercial_notes.md`](reference/ml_commercial_notes.md).

## Veza s ovim radom

- **FSR 3 je klasičan.** Njegov optical flow je SAD pretraga po blokovima
  8×8 (iz AFMF-a) — to odgovara M6 ovog rada, s gušćim poljem (po pikselu).
- **XeSS 2 FG** ima reprojekciju vektorima, naučeni optical flow i naučenu
  mrežu za mješavinu. To je komercijalno rješenje najbliže M9.
- **Arm NSS** je UNet koji predviđa jezgre za klasični akumulator — isti
  princip kao M9: mreža ne crta piksele nego bira/parametrizira između
  postojećih.
- **DLSS 3/4, FSR 4/Redstone i PSSR** zahtijevaju ML akceleratore (Tensor
  Cores, FP8/INT8, XMX). Za RX 580 AMD nema najavu. Mjerenja cijene u M9
  (`docs/ML.md`, poglavlje „Cijena") pokazuju zašto: naučeni dio ovog rada
  bez takvog hardvera nosi 2,7 ms po okviru na 1080p, dvostruko-do-trostruko
  više od cijele klasične heuristike koju zamjenjuje.
- **Sva komercijalna rješenja za generiranje okvira interpoliraju** (drže
  najnoviji okvir, pa trebaju Reflex / Anti-Lag 2 / XeLL). Isto radi i ovaj
  rad, uz +8–16 ms latencije (`docs/PACING.md`). Jedina objavljena
  ekstrapolacija, Reflex 2 Frame Warp, do srpnja 2026. nije izašla.
- **UI** svi odvajaju od scene, kao i ovaj rad (M8).
- **Istraživački modeli** (RIFE, FILM, EMA-VFI) rade samo iz RGB okvira, bez
  vektora i dubine, pa su za igre preskupi: RIFE ima 9,8 M parametara
  naspram 7199 u ovom radu.

## Neprovjereno

Popis u nastavku je prenesen iz bilješki bez dodatne provjere — vrijedi ga
čitati kao otvoreno pitanje, ne kao rezultat:

- Točan izlaz mreža DLSS 2/4 (boja naspram jezgri/težina mješavine); omjer
  „4× računa" DLSS 4 transformera (izvor navodi samo 2× parametara).
  Je li Reflex 2 popunjavanje rupa naučeno; status isporuke nakon srpnja 2026.
- Vrijedi li DLSS 4.5-ovo proširenje FG modela (dodatni UI spremnici) i za
  RTX 40.
- FSR 4 na RDNA 2 „rano 2027." i vrijeme za RDNA 3 APU (samo sekundarni
  izvori). Nema AMD-ove izjave za Polaris/RX 580.
- Broj parametara za IFRNet, EMA-VFI i FILM; bilo koja apsolutna veličina
  mreža DLSS-a, FSR 4, XeSS-a, PSSR-a ili NSS-a.
- Je li MetalFX-ov interpolator okvira naučen.
- PS5 Pro 300 TOPS/67 TFLOPS i PSSR 2 INT8 citat: sekundarno prenošenje
  seminara/intervjua, ne izvorni snimak.
- XeSS-FG na ne-Intel GPU-ima (SDK README naspram whitepapera, proturječno).
  Točni datumi XeSS 3 drivera su sekundarni.
- Qualcomm Adreno Neural Fusion (rujan 2026): samo iz tiska. Godine izdanja
  SGSR 1/2.
