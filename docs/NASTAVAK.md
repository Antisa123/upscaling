# Gdje smo stali i kako nastaviti

Stanje na dan 16. 9. 2026. Dokument je za predaju posla: što je gotovo, što
je ostalo i odakle krenuti. Za opseg rada vidi [`PLAN.md`](../PLAN.md), za
build i pokretanje [`README.md`](../README.md).

## Ukratko

- **M0–M9 su gotovi.** Rezultati su u `docs/`; tablice modula su u
  `PLAN.md` (poglavlje 9).
- **Otvoren je samo dio ML modula koji je tražio mentor.** Mentor je na
  pitanje je li ML obavezan odgovorio: *„bilo bi dobro imati neku svoju
  varijantu izgrađenu radi pokazivanja demonstracije (na manjim primjerima),
  onda možete dodatno obraditi komercijalne“*.
  - Vlastita varijanta i alati za demonstraciju su gotovi.
  - Tekst o komercijalnim rješenjima i odjeljak o demonstraciji još nisu
    napisani.
- **Nakon toga slijedi M10:** mjerenja za rad, grafovi i pisanje rada.
  Struktura rada je u `PLAN.md`, poglavlje 12.

## Što je gotovo u M9 (naučena mješavina)

Detalji i sve brojke su u [`docs/ML.md`](ML.md).

- **Mreža.** Mali U-Net (1 875 – 15 979 parametara; odabrani model c8-c16
  ima 7 199) po pikselu bira težine za 7 kandidata koje klasični pipeline
  već ima. Kandidati su:
  - izlaz heuristike,
  - warp igrinim vektorima iz t−1 i iz t,
  - warp optical flowom iz t−1 i iz t,
  - trenutni i prethodni okvir bez warpa.
- **Implementacija.** Radi u compute shaderima, s passevima 10–12
  (`shaders/fg_ml_*.comp`, `shaders/ml_*.comp`, `src/framegen/ml_blend.*`).
- **Trener.** Vlastiti, u C++20 s OpenMP-om na CPU-u (`tools/fg_train`).
  Arhitektura i format datoteka dijele se kroz `src/ml/blend_net.h`.
- **Provjera shader = trener.** Razlika je srednje 7·10⁻⁵, a najviše 5·10⁻⁴.
- **Rezultat.**
  - SSIM je viši na svih 8 mjernih pogleda.
  - PSNR je +0,03 do +0,20 dB na 7 od 8.
  - Na najgorem okviru i na neviđenoj sceni nema dobitka.
- **Cijena.** FG na 1080p traje 1,8 → 4,6 ms. Pri opterećenju ×12 FPS je
  147 → 120. **Na RX 580 se ne isplati.** To je i zaključak za rad:
  komercijalna rješenja zato traže ML akceleratore (vidi niže).

### Demonstracija (gotovo, nije opisano u `docs/ML.md`)

| Što | Kako |
|---|---|
| Cijeli postupak na malom primjeru (~1 min) | `scripts/ml_demo.sh`: snimi 2 putanje na 720p, istrenira c4-c8 u 1 500 koraka, provjeri shader naspram trenera i ispiše naredbu za interaktivno pokretanje. Zadnji put: +0,54 dB na kontrolnoj putanji, razlika shader/trener najviše 4,8·10⁻⁴. |
| Uživo | `build/fsr3lite ... --fg --fg-ml captures/ml/weights/blend-c8-c16.bin`. Tipka **M** mijenja naučenu i heurističku mješavinu, tipka **0** kruži kroz prikaze 9 → 10 → 11. Prikaz **11** su težine mreže. |
| Boje prikaza 11 | siva = heuristika · tamno/svijetlo zelena = igrini vektori iz t−1 / t · tamno/svijetlo plava = optical flow iz t−1 / t · crvena = trenutni okvir · narančasta = prethodni okvir |
| Slike za rad | `python3 scripts/ml_gallery.py` → `captures/ml/gallery/*.png` i `legend.png`. Stupci: referenca · heuristika · mreža · težine · greška heuristike ×6 · greška mreže ×6, uz izreze gdje mreža najviše pomaže i gdje najviše šteti. |
| Pojedinačni okviri | `--validate-fg --fg-shots DIR --fg-shot-frames 57,63` zapisuje `frameN_reference/_blend/_heuristic/_learned/_weights.png`. |

## Što je ostalo (redom)

1. **Pregledati galeriju** (`captures/ml/gallery/`) i odabrati 2–3 slike
   za rad.
   - Galerija je ponovno generirana nakon popravka opisanog u točki 3, ali
     slike još nitko nije pogledao.
   - PNG-ovi nisu u gitu, pa ih treba generirati lokalno
     (`scripts/ml_gallery.py`). Skripta treba CSV-ove iz
     `scripts/run_metrics.py`.
2. **Napisati poglavlje o komercijalnim rješenjima** na hrvatskom, npr.
   `docs/ML_KOMERCIJALNO.md`. To je poglavlje 3 rada.
   - Izvor su bilješke
     [`docs/reference/ml_commercial_notes.md`](reference/ml_commercial_notes.md)
     (engleski). U njima su izvori (URL-ovi), usporedna tablica i popis
     neprovjerenih tvrdnji.
   - Neprovjerene tvrdnje ne prenositi kao činjenice.
   - Što povezati s našim radom:
     - **FSR 3** je klasičan. Optical flow mu je SAD pretraga po blokovima
       8×8 (iz AFMF-a), što odgovara našem M6.
     - **XeSS 2 FG** ima reprojekciju vektorima, naučeni flow i naučenu
       mrežu za mješavinu. To je najbliže našem M9.
     - **Arm NSS** je UNet koji predviđa jezgre za klasični akumulator. I to
       je naš pristup: mreža ne crta piksele nego bira između postojećih.
     - **DLSS 3/4, FSR 4 / Redstone i PSSR** zahtijevaju ML akceleratore
       (Tensor Cores, FP8/INT8, XMX). Za RX 580 AMD nema najavu. Naša
       mjerenja cijene pokazuju zašto.
     - **Sva komercijalna rješenja za generiranje okvira interpoliraju**
       (drže najnoviji okvir, pa trebaju Reflex / Anti-Lag 2 / XeLL). Isto
       radimo i mi, uz +8–16 ms latencije (`docs/PACING.md`). Jedina
       ekstrapolacija je Reflex 2 Frame Warp, koji do srpnja 2026. nije
       izašao.
     - **UI** svi odvajaju od scene, kao i mi (M8).
     - **Istraživački modeli** (RIFE, FILM, EMA-VFI) rade samo iz RGB
       okvira, bez vektora i dubine, pa su za igre preskupi. Na primjer,
       RIFE ima 9,8 M parametara naspram naših 7 199.
3. **Dodati odjeljak „Demonstracija“ u `docs/ML.md`.** Sadržaj je tablica
   iznad, plus opis greške koja je u međuvremenu popravljena:
   - Ako se prikaz težina uključi usred izvođenja, varijanta shadera
     prevedena s `#define` i prvi put pokrenuta tek tada (s teksturom
     alociranom u istom okviru) pokvari taj okvir na Mesa/RX 580: 26,6 dB
     umjesto 37,6 dB.
   - Popravak: jedan program s uniformom `uWriteWeights` i tekstura
     alocirana unaprijed (`fg_ml_blend.comp`, `MlBlend::ensure`).
   - Provjera: 0 različitih okvira naspram izvođenja bez prikaza.
4. **Uskladiti reference.** U `PLAN.md` (Modul E) i `docs/METRICS.md` treba
   spomenuti `ml_demo.sh`, `ml_gallery.py` i novo poglavlje. U `README.md`
   dodati novo poglavlje u tablicu dokumentacije.
5. **Po želji: ponovno izmjeriti trajanje passa 12** nakon popravka iz
   točke 3, na neopterećenom računalu. Očekuje se ista brojka (grananje po
   uniformu), ali nije izmjereno.
6. **M10: rad.**
   - Brojke su u `captures/metrics/summary.md` i u `docs/*.md` po modulu.
   - Tablice se ponovno generiraju s `scripts/run_metrics.py` (vidi
     `docs/METRICS.md`).

## Na što paziti

- **GPU mjerenja vremena rade se samo na neopterećenom računalu.**
  - Igra, preglednik s videom ili CPU trening u pozadini kvare brojke
    (kvalitetu ne).
  - Trening mreže opterećuje sve jezgre, pa ga ne pokretati uz mjerenja u
    stvarnom vremenu.
- **Skup za učenje nije u gitu** (~200 MB po putanji).
  - `scripts/ml_dataset.py` ga ponovno snimi.
  - `scripts/ml_train.sh` trenira modele, a `scripts/ml_select.py` bira
    model na validacijskim putanjama.
  - Mjerni pogledi nikad nisu u skupu za učenje. Tako treba i ostati.
- **Trening nije bit-deterministički** (OpenMP). Ponovljeni trening daje
  model unutar raspona triju sjemena u `docs/ML.md`, ne istu datoteku.
- **Naučeni modeli su u `captures/ml/weights/`.**
  - `blend-c8-c16.bin` je odabrani model.
  - Ostalo su ablacije (veličina, sjeme, samo Sponza).
- **Scene nisu u gitu.** Dovodi ih `scripts/fetch_assets.sh`.
- **Git povijest je prepisana 16. 9. 2026.** Iz poruka su maknuti
  automatski dodani retci. Postojeći klon treba osvježiti:
  `git fetch && git reset --hard origin/main`, ili ponovno klonirati.
