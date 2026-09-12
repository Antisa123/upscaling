# M6 — optical flow (piramidalni block matching)

Modul C iz plana. Procjenjuje gibanje iz **same slike**, bez pomoći
rasterizacije: uzima prikazani okvir i prethodni prikazani okvir i za svaki
blok od 8×8 texela luminancije traži najbolje podudaranje. Rezultat je polje
vektora koje M7 (generiranje međuokvira) koristi za ono što vektori gibanja iz
G-buffera ne opisuju — sjene, refleksije, promjene sjenčanja, animirane
teksture i sav sadržaj koji se ne renderira po trokutima (UI, čestice).

Upscaler ga **ne** koristi. Optical flow je zaseban modul i po zadanom je
isključen, jer bi inače tiho dodao svoju cijenu svakom mjerenju upscalera koje
je već u tablicama.

Konvencija: vektori su u **UV prostoru** i pokazuju iz **trenutnog okvira u
prethodni**, identično `velocity` buferu (`prevUV = uv + mv`). Zato su dva
polja izravno usporediva i izravno zamjenjiva.

Granularnost: luminancija je na **pola** razlučivosti prikaza (960×540 za
1080p), blok je 8 texela, dakle **jedan vektor na 16×16 prikaznih piksela** —
mreža 120×68 na 1080p.

## Pokretanje

```
./build/fsr3lite --validate-flow --scale 1.5 --upscaler fsr --frames 120
```

| Zastavica | Značenje |
|---|---|
| `--optical-flow` | uključuje modul (implicitno u `--validate-flow`) |
| `--validate-flow` | ocjenjuje polje protiv vektora iz rasterizacije (vidi „Referenca”) |
| `--of-levels <n>` | broj razina piramide (zadano 7) |
| `--of-radius <r>` | radijus pretrage u texelima razine (zadano 4) |
| `--of-smoothness <s>` | kazna po texelu udaljenosti od predikcije (zadano 0.0005) |
| `--of-filter <0\|1>` | 3×3 vektorski medijan (zadano 1) |
| `--of-upscale <0\|1>` | izbor kandidata pri prijelazu na finiju razinu (zadano 1) |
| `--of-temporal <0\|1>` | polje prošlog okvira kao kandidat (zadano 1) |
| `--of-novelty <n>` | doplata na kandidate koji nisu naslijeđeni s grublje razine (zadano 0.001) |
| `--of-scene-change <0\|1>` | detekcija reza (zadano 1) |
| `--of-scene-threshold <t>` | prag udaljenosti histograma za rez (zadano 0.25) |
| `--of-scene-statistic <0\|1\|2>` | statistika nad devet sekcija: 0 = maksimum, 1 = srednja, 2 = medijan (zadano 1) |
| `--cut-every <n>` | umjetni rez svakih n okvira, za mjerenje detektora |
| `--flow-scale <px>` | koliko prikaznih piksela je puna zasićenost u HSV prikazu (zadano 24) |
| tipka `7` / `--debug-view 6` | procijenjeni tok, HSV |
| tipka `8` / `--debug-view 7` | vektori iz rasterizacije, na istoj mreži i skali |
| tipka `9` / `--debug-view 8` | pogreška podudaranja pobjedničkog bloka |

## Prolazi

| Prolaz | Shader | Razlučivost | Što radi |
|---|---|---|---|
| 1 | `of_luma.comp` | pola prikazne | luminancija iz prikazanog okvira |
| 2 | `of_pyramid.comp` | ×7 | jedna razina piramide po pozivu |
| 3a | `of_histogram.comp` | pola prikazne | histogram luminancije, 9 sekcija × 16 razreda |
| 3b | `of_scene_change.comp` | 1 grupa | devet udaljenosti → jedna presuda |
| 4 | `of_search.comp` | po razini | block matching, SAD, radijus R |
| 5 | `of_filter.comp` | po razini | 3×3 vektorski medijan |
| 6 | `of_upscale.comp` | po razini | izbor početnog vektora za finiju razinu |
| — | `of_epe.comp` | mreža blokova | mjerenje: endpoint error protiv `velocity` bufera |

Glavna petlja je 7 iteracija × 3 prolaza (search → filter → upscale), od
najgrublje razine prema najfinijoj. Piramida ping-ponga s prethodnim okvirom,
pa se svaki okvir gradi jednom i koristi dvaput.

### Zašto UV, a ne pikseli razine

Vektor u UV prostoru pune slike je neovisan o razlučivosti. Zato u lancu
coarse-to-fine **nigdje nema množenja s dva**, i cijela klasa grešaka „u kojoj
je razini ovaj vektor” ne može nastati. Pretraga svejedno ostaje cjelobrojna:
predikcija se na ulazu zaokruži na texelsku rešetku te razine
(`ivec2(round(predicted * vec2(uLevelSize)))`), a profinjenje je cjelobrojni
pomak na nju.

Posljedica koju treba reći otvoreno: **nema sub-pixel profinjenja**. Najfinija
razina radi na luminanciji u pola razlučivosti, pa je najmanji razlučiv pomak
1 texel = **2 prikazna piksela**. To je i razlog zašto stupac „unutar 1 px”
staje na ~62 % umjesto da ide prema 100 % — dobar dio preostale pogreške nije
kriva procjena nego kvantizacija. Sub-pixel (parabolička interpolacija oko
minimuma SAD-a) je prva stvar koju bi trebalo dodati ako M7 zatraži precizniji
tok.

### Kriterij i glatkoća

Kriterij je **SAD** (suma apsolutnih razlika luminancije), kao u
FfxOpticalFlow, a ne SSD: jedan specularni piksel koji se jako promijenio ne
smije moći poništiti inače savršeno podudaranje, a kvadriranje mu daje točno tu
moć.

SAD se normalizira u **srednju** apsolutnu razliku luminancije, pa je i kazna
glatkoće u istim jedinicama. To je važnije nego što zvuči: dobro podudaranje na
ovom sadržaju postiže ≈0.02, pa kazna od 0.01 po texelu ima veličinu cijelog
signala i pretraga se zalijepi za predikciju. Izmjereno, to je razlika između
7.2 px i 17.1 px srednje pogreške (tablica niže).

## Referenca: čime se uopće mjeri točnost

Optical flow nema ground truth u pravom slučaju uporabe — kad bi ga imao, ne bi
trebao postojati. Ali dok je **jedino gibanje gibanje kamere**, vektori iz
rasterizacije su egzaktni, i onda su besplatna referenca. Sponza je statična
scena, pa je cijela mjerna orbita upravo taj slučaj.

`of_epe.comp` uspoređuje po bloku: referentni vektor je prosjek `velocity`
bufera preko površine bloka (4×4 bilinearna uzorka), a pogreška je euklidska
udaljenost u **prikaznim pikselima** (endpoint error, EPE).

Raspodjela EPE-a je teškorepa: jedan bliski prolazak pored stupa, gdje slika
klizi 366 px po okviru, određuje srednju vrijednost cijele snimke. Zato se
posvuda izvještavaju **srednja vrijednost, medijan i p95**, a ne samo srednja.

## Rezultati

Mjerna orbita Sponze, RX 580, jedan vektor na 16×16 prikaznih piksela.

### Ovisnost o brzini kamere

Prozor je fiksiran u **vremenu scene**, ne u broju okvira: pola sekunde
zagrijavanja, jedna sekunda mjerenja, pri svakoj brzini. (Fiksni broj okvira
pri tri koraka mjeri tri različita komada putanje, a putanja nije ravnomjerno
teška — to je samo po sebi dovoljno da sporija brzina izgleda točnije.)

| Kamera | EPE sred. | EPE medijan | EPE p95 | unutar 1 px | unutar 2 px |
|---|---|---|---|---|---|
| mirna | 0.00 | 0.00 | 0.00 | 100.0 % | 100.0 % |
| 120 fps | 2.82 | 1.49 | 7.62 | 65.7 % | 89.0 % |
| 60 fps | 11.63 | 3.66 | 36.81 | 56.9 % | 80.2 % |
| 30 fps | 35.79 | 8.83 | 106.72 | 47.5 % | 70.4 % |

Za usporedbu, „nula posvuda” (procjenitelj koji ništa ne radi) na istoj putanji
daje srednju pogrešku 23.0 / 46.1 / 92.3 px pri 120 / 60 / 30 fps, a medijan
10.5 / 21.2 / 41.3 px. Procjenitelj dakle uklanja oko 87 % gibanja pri 120 fps,
75 % pri 60 i 61 % pri 30.

Degradacija s korakom nije linearna i nije slučajna: block matching ima **tvrd
doseg**. Radijus R na razini L pokriva 2^L · R texela, a preko toga se blok ne
pogoršava postupno — izgubi se. Pri 30 fps dio kadra jednostavno izađe iz
dosega najgrublje razine.

Mirna kamera daje **točno nulu**, što je korisna provjera: procjenitelj na
statičnoj slici ne izmišlja gibanje. (Napomena: `--fixed-dt 0` znači *zidni
sat*, ne mirnu kameru; mirna kamera je `--fixed-dt 0.0000001` ili izostavljanje
`--scripted`.)

### Ovisnost o razlučivosti

Ista orbita pri 60 fps, 104 okvira:

| Konfiguracija | EPE sred. | medijan | p95 | unutar 1 px | GPU ms |
|---|---|---|---|---|---|
| 1280×720, render 854×480, FSR | 4.44 | 1.10 | 19.93 | 65.3 % | 1.43 |
| 1080p, render 1280×720, FSR | 7.17 | 1.36 | 33.50 | 62.5 % | 2.71 |
| 1080p native | 7.37 | 1.37 | 33.35 | 61.6 % | 3.47 |
| 1080p, render 960×540, FSR | 6.94 | 1.40 | 33.36 | 61.2 % | 2.32 |
| 4K, render 1920×1080, FSR | 13.75 | 1.64 | 71.13 | 59.8 % | 7.59 |

Pogreška u pikselima raste s razlučivošću jer isti kut zakreta kamere znači
više piksela; relativna točnost (udio blokova unutar 1 px) gotovo je konstantna.
Vrijedno je i to da **upscaler ne šteti**: procjena iz 1080p rekonstruiranog iz
720p jednako je dobra kao iz nativnog 1080p (7.17 vs 7.37). Tok se računa iz
slike koju korisnik vidi, i ta slika je dovoljno dobra.

## Ablacije

Mjerna orbita, 60 fps, 104 okvira, sve ostalo na zadanom.

### Mehanizmi

| Konfiguracija | EPE sred. | medijan | p95 | unutar 1 px |
|---|---|---|---|---|
| sve zadano | 7.17 | 1.36 | 33.50 | 62.5 % |
| bez kandidata iz prošlog okvira | 11.04 | 1.84 | 49.96 | 60.1 % |
| bez medijan filtra | 7.95 | 2.39 | 34.64 | 56.5 % |
| bez izbora kandidata pri proširenju | 12.68 | 1.18 | 56.98 | 61.5 % |
| bez detekcije reza | 7.17 | 1.36 | 33.50 | 62.5 % |

Sva tri mehanizma zarađuju svoje mjesto, ali ne na isti način:

* **Temporalni kandidat** (polje prošlog okvira) je najveći pojedinačni
  doprinos srednjoj pogrešci. Gibanje je vremenski koherentno — kamera koja se
  okreće konstantnom brzinom daje gotovo isto polje dva okvira zaredom — pa je
  prošli okvir bolji prior od bilo čega što piramida može ponuditi, i za razliku
  od piramide ne degradira s veličinom pomaka.
* **Medijan filtar** se vidi u medijanu i u udjelu točnih blokova (1.36 vs
  2.39 px; 62.5 % vs 56.5 %), a jedva u srednjoj vrijednosti. To je očekivano:
  on uklanja izolirane ispade, a ne velike sistematske promašaje.
* **Izbor kandidata pri proširenju** je obrnut slučaj — medijan mu je čak
  *bolji* kad ga isključimo (1.18 px), ali p95 skoči sa 33.5 na 57.0. Slijepo
  kopiranje roditelja razmazuje granice grubljih blokova u finiju razinu, i tamo
  gdje je roditelj bio prosjek dvaju gibanja pogrešan je za obje polovice svoje
  površine. Ablacija bez p95 stupca ovo ne bi pokazala.
* **Detekcija reza** na ovoj putanji ne mijenja ništa, jer u njoj nema reza.
  Mjeri se zasebno, niže.

### Broj razina piramide (doseg)

| Razine | EPE sred. | medijan | p95 | doseg (texela luminancije) |
|---|---|---|---|---|
| 3 | 18.95 | 1.40 | 141.17 | 16 |
| 4 | 18.20 | 1.35 | 137.16 | 32 |
| 5 | 11.18 | 1.36 | 76.07 | 64 |
| 6 | 6.90 | 1.36 | 33.21 | 128 |
| 7 | 7.17 | 1.36 | 33.50 | 256 |

Medijan je **konstantan** kroz cijelu tablicu, a p95 pada četverostruko: razine
ne poboljšavaju tipičan blok nego spašavaju one koji brzo lete. 6 i 7 su
izjednačene na ovoj putanji jer 128 texela (256 prikaznih piksela) već pokriva
sve što se na njoj događa; 7 je zadano jer košta 0.026 ms i pokriva i brže
kadrove.

### Radijus pretrage (točnost naspram cijene)

| Radijus | EPE sred. | medijan | p95 | GPU ms (cijeli okvir) |
|---|---|---|---|---|
| 2 | 8.48 | 1.47 | 37.43 | 2.61 |
| 4 | 7.17 | 1.36 | 33.50 | 2.70 |
| 6 | 6.67 | 1.33 | 30.60 | 2.83 |
| 8 | 6.54 | 1.32 | 30.20 | 3.02 |

Broj kandidata raste s (2R+1)², dobitak se gasi. Od 4 do 8 točnost se popravi
za 9 % uz 46 % skuplji modul; 4 je koljeno i ostaje zadano.

### Glatkoća

| Kazna / texel | EPE sred. | medijan | p95 | unutar 1 px |
|---|---|---|---|---|
| 0 | 7.08 | 1.41 | 30.90 | 63.7 % |
| 0.0002 | 6.68 | 1.36 | 31.09 | 63.6 % |
| 0.0005 | 7.17 | 1.36 | 33.50 | 62.5 % |
| 0.002 | 8.30 | 1.39 | 42.37 | 57.5 % |
| 0.01 | 17.14 | 2.71 | 107.05 | 39.9 % |

Kazna postoji zato što je block matching unutar ravne plohe loše postavljen
problem: svaki kandidat postiže isti SAD, pa je pobjednik onaj kojeg je
redukcija slučajno prva vidjela. Kazna to razrješava u korist grublje razine,
što je ispravan prior. Ali jedinice su nemilosrdne — na 0.01 pretraga više ne
pretražuje. Zadanih 0.0005 je kompromis: nešto skuplje od optimuma na ovoj
putanji (0.0002), ali mjerljivo stabilnije na bržim snimkama, gdje bez kazne
polje postane šumno (13.0 px pri 120 fps bez kazne naspram 6.3 px s njom, u
odvojenoj vremenski prozorovanoj snimci).

### Novelty

| Novelty | EPE sred. | medijan | p95 |
|---|---|---|---|
| 0 | 7.40 | 1.41 | 36.88 |
| 0.0005 | 7.32 | 1.40 | 34.96 |
| 0.001 | 7.17 | 1.36 | 33.50 |
| 0.004 | 8.22 | 1.23 | 35.11 |
| 0.01 | 10.95 | 1.20 | 45.09 |
| 1 | 11.93 | 1.22 | 49.45 |

Doplata na dva kandidata koja nisu naslijeđena s grublje razine (prošli okvir i
nula). Unutar bloka niskog kontrasta svaki kandidat postiže rezultat unutar
tisućinke od svakog drugog, pa bez doplate ta dva pobjeđuju na šumu i bacaju
vektor koji je piramida već bila pogodila. Zanimljiv je oblik tablice:
**medijan se popravlja s većom doplatom, a srednja vrijednost i p95 se
kvare** — jaka doplata štiti tipičan blok, a oduzima izlaz upravo blokovima
koji su brzi i kojima prošli okvir treba. 0.001 je zadano.

## Detekcija promjene scene

Nakon reza dva uzastopna okvira nemaju nikakve veze, a block matching će
svejedno naći „najbolje” podudaranje i vratiti smeće. Detektor uspoređuje
histograme luminancije po **devet sekcija** slike (16 razreda po sekciji),
računa udaljenost totalne varijacije `0.5·Σ|p−q|` ∈ [0,1] po sekciji, i iz
devet brojeva izvodi jednu presudu. Rezultat ostaje u SSBO-u koji prolaz
pretrage čita izravno; na CPU dolazi s dva okvira zakašnjenja (prsten od tri
readback bufera) i služi samo za ispis.

Koja statistika nad devet sekcija? Sve tri se pišu u CSV svaki okvir, pa je
izbor mjerenje a ne tvrdnja. `scripts/flow_cuts.py` forsira rez svakih 8 okvira
(`--cut-every`) na tri orbite i izvještava najmanju udaljenost koju je
proizveo pravi rez naspram najveće koju je proizvelo obično gibanje:

| Orbita | Statistika | min(rez) | max(gibanje) | margina | detektirano | lažnih pozitiva |
|---|---|---|---|---|---|---|
| mjerna (r=3, faza −0.6) | maksimum | 0.568 | 0.463 | +0.105 | 15/15 | 11/105 |
| mjerna (r=3, faza −0.6) | **srednja** | 0.253 | 0.207 | +0.046 | 15/15 | 0/105 |
| mjerna (r=3, faza −0.6) | medijan | 0.232 | 0.179 | +0.053 | 14/15 | 0/105 |
| široka (r=5) | maksimum | 0.963 | 0.830 | +0.133 | 15/15 | 9/105 |
| široka (r=5) | **srednja** | 0.691 | 0.306 | +0.385 | 15/15 | 4/105 |
| široka (r=5) | medijan | 0.609 | 0.366 | +0.244 | 15/15 | 2/105 |
| vanjska (r=11) | maksimum | 0.974 | 0.026 | +0.948 | 15/15 | 0/105 |
| vanjska (r=11) | **srednja** | 0.342 | 0.012 | +0.330 | 15/15 | 0/105 |
| vanjska (r=11) | medijan | 0.211 | 0.010 | +0.202 | 12/15 | 0/105 |

Maksimum ima najširu marginu i **ipak je najgori izbor**: brzi zaokret zamijeni
sadržaj dvije-tri sekcije na vodećem rubu u cijelosti, što se od reza ne
razlikuje ni po čemu osim po ostatku slike — 9 do 11 lažnih poziva na 105
okvira običnog gibanja. Medijan je suprotna greška: rez na vizualno sličan
sadržaj (pola orbite dalje, isti kameni atrij) pomakne samo nekoliko sekcija
daleko, pa ga medijan propusti. Srednja vrijednost hvata sve rezove na sve tri
orbite uz nula lažnih poziva na dvije od njih.

Ista stvar vidi se i u ablacijskoj tablici, na putanji u kojoj **nema** reza:

| Statistika | EPE sred. | p95 | unutar 1 px |
|---|---|---|---|
| maksimum | 17.27 | 129.04 | 58.6 % |
| srednja | 7.17 | 33.50 | 62.5 % |
| medijan | 7.17 | 33.50 | 62.5 % |

Maksimum sam sebi pokvari točnost za 2.4×, jer pet uzastopnih okvira bliskog
prolaska proglasi rezovima i baci njihov tok.

Ono što se ne smije prešutjeti: na **mjernoj** orbiti margina srednje
vrijednosti je samo +0.046, a prag 0.25 leži jedva ispod najslabijeg reza
(0.253). Detektor tu radi, ali bez rezerve. Rez unutar vizualno homogenog
prostora granični je slučaj za svaku metodu utemeljenu na histogramu — ona po
konstrukciji ne zna ništa o prostornom rasporedu — i pravo rješenje je da motor
javi rez eksplicitno, kao što FSR3 i traži (`reset` zastavica). `--cut-every`
tu zastavicu i koristi; detektor je za slučaj kad je nema.

## Cijena

Sponza, RX 580, prosjek po okviru:

| Prolaz | 720p → 1080p | 1080p → 4K |
|---|---|---|
| OF luma + piramida | 0.108 | 0.391 |
| OF histogram + presuda | 0.039 | 0.159 |
| OF search/filter/upscale (7 razina) | 0.407 | 1.314 |
| **Optical flow ukupno** | **0.558** | **1.872** |
| za usporedbu: FSR accumulate | 0.676 | 2.692 |
| za usporedbu: cijeli okvir | 2.680 | 7.662 |

Po razinama (1080p → 4K, ms):

| Razina | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|---|---|---|
| 720p → 1080p | 0.026 | 0.018 | 0.018 | 0.018 | 0.029 | 0.070 | 0.228 |
| 1080p → 4K | 0.028 | 0.019 | 0.019 | 0.030 | 0.074 | 0.240 | 0.905 |

Najfinija razina je **41 %** cijene modula, a šest grubljih zajedno 0.179 ms,
manje od trećine — što je i razlog zašto se broj razina ne isplati štedjeti.
Ključna optimizacija u `of_search.comp` je stavljanje cijelog prozora pretrage u
dijeljenu memoriju prije evaluacije kandidata: naivno, prozor se pročita
(2R+1)² puta, i to je razlika između prolaza ograničenog propusnošću i prolaza
ograničenog ALU-om.

Modul košta oko **21 %** okvira na 1080p i **24 %** u 4K. To je cijena koja se
plaća samo ako se generiraju međuokviri — a tamo je alternativa renderirati
cijeli okvir, što na istoj sceni košta 2.68 ms.

## Gdje procjenitelj griješi

`captures/flow/hsv_compare.png` (generira ga `scripts/flow_debug.py`) postavlja
procijenjeno polje uz egzaktno, u istom HSV kodiranju i na istoj skali. Struktura
je ista: isti horizontalni gradijent, isto iščezavanje zasićenosti prema točki
nedogleda, ista promjena nijanse na rubovima kadra. To je kriterij prihvaćanja
za M6 i on je ispunjen.

Vidljivi defekti, i zašto:

1. **Plava zavjesa lijevo.** Fino periodično tkanje daje bloku mnogo jednako
   dobrih podudaranja, pa je nijansa tamo šumna gdje je referenca glatka. To je
   klasični slučaj periodične teksture i aperture problema: kriterij nema
   jedinstveni minimum, i nijedan iznos pretrage to ne popravlja. Kazna
   glatkoće i medijan filtar ublažavaju posljedicu, ne uzrok.
2. **Ravni zidovi i pod.** Isti problem u blažem obliku — nema strukture, pa
   pobjeđuje prior. Karta pogreške podudaranja (prikaz 9) te blokove i pokazuje
   crvenkastima: procjenitelj zna da nije našao dobro podudaranje. Ta
   informacija je u `z` kanalu polja i M7 je treba iskoristiti kao težinu, a ne
   tretirati sve vektore kao jednako pouzdane.
3. **Silhuete.** Blok od 16×16 prikaznih piksela preko ruba stupa sadrži dva
   gibanja i mora izabrati jedno. To je granica rezolucije mreže, ne greška
   algoritma; FSR3 to rješava na istoj granularnosti i isto.

## Format izlaza

Jedna RGBA16F tekstura, jedan texel po bloku:

| Kanal | Sadržaj |
|---|---|
| `xy` | pomak u UV prostoru, trenutni okvir → prethodni |
| `z` | srednja apsolutna razlika luminancije pobjedničkog podudaranja (0 = savršeno) |
| `w` | 1 ondje gdje je vektor uopće procijenjen |

`z` je ono što M7 treba za odluku kome vjerovati: vektoru toka ili vektoru iz
rasterizacije.

## Reprodukcija

```
scripts/run_metrics.py --group flow flow-ablation flow-speed   # tablice
scripts/flow_cuts.py                                           # detekcija reza
scripts/flow_debug.py                                          # HSV figura
```

Napomene o mjerenju koje vrijede i za ostale module:

* `--fixed-dt 0` znači **zidni sat**, ne zaustavljenu kameru.
* `--warmup N` pomiče prvi redak CSV-a; agregatni ispis `[validate-flow]` ima
  vlastito, fiksno zagrijavanje od 2 okvira i ne sluša ga.
* Usporedbe pri različitom `--fixed-dt` moraju fiksirati **prozor u vremenu
  scene**, a ne broj okvira; `flow-speed` grupa to radi računajući broj okvira
  iz koraka.

## Teorijska podloga

Fleet i Weiss, *Optical Flow Estimation* (Handbook of Mathematical Models in
Computer Vision, 2006) — pretpostavka konstantnosti svjetline, aperture problem
i loša postavljenost problema unutar homogenih područja, što su točno tri
mehanizma koje gornje ablacije mjere. Implementacija slijedi
`FfxOpticalFlow` iz FidelityFX SDK-a (piramida, SAD block matching, tri prolaza
po razini, histogramska detekcija reza).
