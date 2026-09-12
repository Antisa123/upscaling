# M5 — puni upscaler (FSR3-stil)

M4 (`docs/TAAU.md`) je namjerno imao samo tri dijela: reprojekciju, akumulaciju
i clamp boje. M5 dodaje ono što ih u FSR2/FSR3 čini upotrebljivima — dilataciju
vektora gibanja, odbacivanje povijesti po dubini, lockove za tanke detalje,
Lanczos rekonstrukciju i reactive masku — i mjeri svaki mehanizam zasebno.

`taau.comp` ostaje netaknut. On je ablacijska osnovica u tablici, a dijeljenje
koda s `fsr_accumulate.comp` pretvorilo bi svaki redak te tablice u tvrdnju o
jednom `#ifdef`-u umjesto o algoritmu.

## Pokretanje

```
./build/fsr3lite --upscaler fsr --scale 1.5 --validate-upscale --frames 120
```

| Zastavica | Značenje |
|---|---|
| `--upscaler fsr` | dilatacija + depth clip + lockovi + Lanczos + akumulacija |
| `--upscaler fsr-rcas` | isto, plus RCAS (vidi „RCAS” niže — na ovom sadržaju šteti) |
| `--fsr-dilate <0\|1>` | dilatacija vektora gibanja po najbližoj dubini (zadano 1) |
| `--fsr-disocclusion <s>` | jačina odbacivanja povijesti po dubini; 0 = M4 ponašanje (zadano 1) |
| `--fsr-depth <t>` | relativno odstupanje dubine koje se još tolerira (zadano 0.02) |
| `--fsr-lock-life <f>` | koliko okvira lock traje; 0 isključuje lockove (zadano 4) |
| `--fsr-lock-relax <r>` | koliko se clamp kutija širi na zaključanom pikselu (zadano 2) |
| `--fsr-lock-contrast <c>` | prag kontrasta za detekciju tankog detalja (zadano 0.5) |
| `--fsr-lanczos <s>` | širina rekonstrukcijske jezgre u gibanju (zadano 1.0) |
| `--fsr-lanczos-still <s>` | ista širina s mirnom kamerom (zadano 2.0) |
| `--fsr-reactive <s>` | jačina reactive maske; zadano 0, vidi „Reactive maska” |
| tipka `6` | prikaz disocclusion maske (crveno = povijest se odbacuje) |

## Prolazi

Tri compute prolaza umjesto M4-ova jednog:

| Prolaz | Razlučivost | Što radi |
|---|---|---|
| `fsr_dilate.comp` | render | dilatacija vektora + disocclusion maska iz dubine |
| `fsr_locks.comp` | render | detekcija tankih detalja |
| `fsr_accumulate.comp` | display | Lanczos rekonstrukcija, reprojekcija, lock stroj stanja, rektifikacija, akumulacija |

### 1. Dilatacija i depth clip

Oba posla žele istu 3×3 okolinu dubine pa dijele prolaz.

**Dilatacija.** Vektor gibanja vrijedi za površinu koju piksel stvarno pokriva.
Na silueti render piksel pokriva dvije površine a sprema jednu, i display
piksel koji se s njim reprojicira može pripadati onoj drugoj. Uzimanje vektora
**najbliže** dubine u 3×3 prozoru pomiče svaki rubni piksel prema prednjoj
površini — onoj čije je gibanje vizualno dominantno i na čijem se izlaznom rubu
ghosting i vidi.

**Depth clip.** G-buffer po pikselu piše ovokvirnu linearnu udaljenost i
udaljenost koju je ista točka površine imala prošli okvir. Oboje je besplatno:
projekcija je reverse-Z s beskonačnom dalekom ravninom, pa je `clip.w = -viewZ`
točno — bez rekonstrukcije, bez inverzne matrice i bez gubitka preciznosti od
prolaska kroz [0,1] dubinski spremnik čija se razlučivost daleko od kamere mjeri
u metrima. Ako vektor pokazuje na površinu koja je tamo doista bila, prošlookvirni
dubinski spremnik na toj poziciji se slaže s drugim brojem. Ako se ne slaže,
piksel je prošli okvir bio skriven i njegova povijest je tuđa boja.

Prag je relativan (`uDepthTolerance * Z`) i uvećan za lokalni nagib dubine u
3×3 prozoru. Bez nagiba bi pod gledan pod oštrim kutom, gdje su susjedni pikseli
legitimno metrima udaljeni, bio proglašen disokludiranim svaki okvir.

### 2. Lockovi

Rektifikacija boje je neselektivna: povlači povijest prema susjedstvu tekućeg
okvira bez obzira je li povijest bila u pravu. Na tankom detalju — karika lanca,
stabljika biljke, rub draperije, bilo što široko oko jednog render piksela — to
je kobno. Detalj pada na render uzorak samo na nekim Halton fazama, pa na
okvirima gdje promaši susjedstvo *ne sadrži* njegovu boju i clamp briše
akumulirani dokaz da detalj postoji. Rezultat treperi u ritmu jitter sekvence,
što je puno uočljivije od ghostinga od kojeg je clamp branio.

Detekcija je FSR2-ova: piksel je strogi ekstrem luminancije duž barem jednog od
četiri smjera kroz 3×3 prozor. Linija široka jedan piksel je ekstrem *poprijeko*
a ne *uzduž* — što je upravo ono što je razlikuje od šuma (ekstrem u svakom
smjeru) i od ruba (ekstrem ni u jednom).

Lock živi `--fsr-lock-life` okvira, putuje s poviješću (reprojicira se
najbližim susjedom — lock je token, a bilinearna mješavina dvaju tokena ne znači
ništa), i gubi se čim piksel postane disokludiran ili mu se luminancija odmakne
od one s kojom je lock stvoren. Dok traje, clamp kutija se širi i **gubi presjek
s tvrdim min/max** okoline — jer je upravo taj presjek operacija koja briše
detalj kojeg u prozoru tog okvira nema.

### 3. Lanczos rekonstrukcija, prilagođena gibanju

M4 je koristio Gaussovu jezgru preko 3×3 render uzoraka. M5 koristi Lanczos-2
preko 4×4, računat iz kvadrata udaljenosti polinomom koji FSR1/FSR2 koriste
umjesto dva `sinc` poziva. Negativni lobovi su poanta: oni su ono što
rekonstrukciji dopušta da frekvenciju *razluči* umjesto da je usrednji.

Širina jezgre je vezana na **istu mjeru gibanja** kao i duljina akumulacije, i
to nije kozmetika nego posljedica iste analize:

```glsl
stillness    = exp(-uMotionDecay * mvPixels);
maxWeight    = mix(1.0, uMaxWeight, stillness);
lanczosScale = mix(uLanczosScale, uLanczosScaleStill, stillness);
```

Širina jezgre trguje oštrinom jednog okvira za njegov šum, a taj je šum bezopasan
samo dok ima dovoljno okvira da se usrednji. Na mirnoj kameri ih ima osam, pa
jezgra smije biti uska i svaki okvir smije biti šumovit; u brzom gibanju ih je
efektivno jedan i uska se jezgra nema iza čega sakriti. Izmjereni optimum na
miru je 2.0, na 30 fps orbiti 1.0, a kriva jedinica košta oko 0.9 dB u oba
smjera.

## Rezultati

Sponza, 1280×720 → 1920×1080 (Quality 1.5×), 120 okvira, referenca 2×
supersamplirana (grupa `fsr-speed` u `captures/metrics/summary.md`):

| Kamera | TAAU (M4) | FSR (M5) | Δ |
|---|---|---|---|
| mirna | 39.36 / 0.9723 | **40.06 / 0.9828** | +0.70 dB, +0.0105 |
| 120 fps | 37.50 / 0.9643 | 37.53 / 0.9636 | +0.03 dB, −0.0007 |
| 60 fps | 35.13 / 0.9449 | **35.48 / 0.9460** | +0.35 dB, +0.0011 |
| 30 fps | 34.36 / 0.9372 | **34.95 / 0.9388** | +0.59 dB, +0.0016 |

Najvažnija posljedica: na 60 fps orbiti FSR **prvi put nadmašuje bicubic i po
PSNR-u i po SSIM-u** (35.48 / 0.9460 naspram 35.37 / 0.9420). M4 je tamo još
zaostajao 0.12 dB. Na 30 fps orbiti bicubic je i dalje 0.32 dB ispred po PSNR-u,
ali iza po SSIM-u.

**Temporalna stabilnost se popravila time što se približila referenci, a ne time
što je porasla.** Na 60 fps orbiti: referenca 34.00 dB, M4 36.94 (+2.94),
M5 34.15 (+0.15). M4 je bio *previše* stabilan — akumulacija je prigušivala i
legitimne promjene, što je isto ono kašnjenje koje mu je uzimalo PSNR u gibanju.
M5 se ponaša temporalno kao ground truth.

### Svi režimi skaliranja

| Režim | Render | PSNR / SSIM | GPU ms | FPS (GPU) |
|---|---|---|---|---|
| NativeAA 1.0× | 1920×1080 | 38.16 / 0.9745 | 2.95 | 339 |
| Quality 1.5× | 1280×720 | 35.48 / 0.9460 | 2.18 | 460 |
| Balanced 1.7× | 1129×635 | 34.71 / 0.9339 | 2.01 | 497 |
| Performance 2.0× | 960×540 | 33.74 / 0.9148 | 1.79 | 558 |
| Ultra Performance 3.0× | 640×360 | 31.42 / 0.8535 | 1.43 | 701 |

U 4K (Performance 2.0×, render 1920×1080): 36.10 / 0.9491 uz 5.77 ms.

NativeAA je slučaj u kojem omjer skaliranja iznosi 1:1 i upscaler radi kao čisti
temporalni antialiasing — korisna gornja granica za ostale retke.

## Ablacije

Svaki redak gasi točno jedan mehanizam, sve ostalo je na zadanom. Mjereno u dva
režima jer se dva mehanizma ponašaju suprotno ovisno o tome koliko je povijest
duga (`fsr-ablation` i `fsr-ablation-still` u `captures/metrics/summary.md`):

| Konfiguracija | 60 fps orbita | mirna kamera |
|---|---|---|
| FSR, sve zadano | 35.48 / 0.9460 | 40.06 / 0.9828 |
| bez dilatacije vektora | 35.29 / 0.9452 | **37.56 / 0.9783** |
| bez depth clipa | 35.35 / 0.9458 | 40.09 / 0.9829 |
| bez lockova | **35.54** / 0.9455 | 39.55 / 0.9763 |
| M4 TAAU, za usporedbu | 35.13 / 0.9449 | 39.36 / 0.9723 |

**Dilatacija je najveći pojedinačni dobitak, i to na mirnoj kameri: +2.50 dB.**
Isprva je to protuintuitivno — kamera stoji, pa zašto bi ispravnost vektora na
silueti uopće bila bitna? Zato što je povijest tada najdulja: pogrešan vektor na
silueti ne griješi jednom, nego se njegova greška akumulira kroz svih osam
okvira. U gibanju, gdje se povijest ionako skraćuje na približno jedan okvir,
isti mehanizam vrijedi samo 0.19 dB. Stabilnost pada s 51.7 na 37.7 dB bez
dilatacije — treperenje na svakom rubu.

**Depth clip je mjerljivo koristan samo u gibanju** (+0.13 dB), a na mirnoj
kameri je neutralan do neznatno štetan (−0.03 dB): kad se ništa ne pomiče,
ništa se ni ne disokludira, pa test samo povremeno lažno pozitivno odbaci
ispravnu povijest. Njegova vrijednost nije u prosjeku nego u najgorem prozoru:
`worst window` SSIM ide s −0.4244 na −0.3475, dakle lokalizirani artefakti na
novootkrivenim površinama.

**Lockovi koštaju 0.06 dB u gibanju i donose 0.51 dB (+0.0065 SSIM) na miru** —
opet zato što štite povijest, a povijesti u gibanju gotovo i nema. Prag
kontrasta 0.2 je bolji na miru (40.22 / 0.9843) a lošiji u gibanju (35.41);
zadano je 0.5 kao kompromis.

### Reactive maska: negativan rezultat

Sponza nema prozirnosti — jedine nekontinuirane površine su alfa-testirano
lišće i draperije. Maska se generira iz blizine alfa praga (piksel tik iznad
praga je onaj koji test ovaj okvir zadrži a sljedeći možda odbaci, samo zato što
je jitter pomaknuo uzorak). Mjereno, monotono šteti:

| `--fsr-reactive` | 60 fps orbita | mirna kamera |
|---|---|---|
| 0 (zadano) | 35.48 / 0.9460 | 40.00 / 0.9824 |
| 0.3 | 35.44 / 0.9459 | 39.89 / 0.9823 |
| 1.0 | 35.24 / 0.9450 | 39.78 / 0.9821 |

Razlog je jednostavan i vrijedi ga zapisati: alfa test je **deterministički**.
Isti uzorak daje isti odgovor svaki okvir, vektori gibanja su ispravni, i
povijest koju maska baca bila je točna. Reactive maska rješava problem koji ova
scena nema, a naplaćuje ga svejedno. Mehanizam ostaje u kodu, isključen, za
sadržaj koji ima čestice i pravu prozirnost — ali tvrdnja da „pomaže” ovdje ne
bi bila potkrijepljena.

### RCAS: sad šteti, i to je dobra vijest

| `--sharpness` | FSR + RCAS, mirna | FSR + RCAS, 60 fps |
|---|---|---|
| 0.8 | 37.70 / 0.9646 | 33.90 / 0.9243 |
| 1.2 | 38.83 / 0.9744 | 34.56 / 0.9340 |
| 2.0 (zadano za FSR) | 39.65 / 0.9804 | 35.13 / 0.9415 |
| 3.0 | 39.90 / 0.9820 | 35.39 / 0.9445 |
| isključen | **40.00 / 0.9824** | **35.58 / 0.9464** |

Krivulja je monotona prema „isključeno”, kao kod FSR1 EASU-a i za razliku od
M4-ova TAAU-a, gdje je RCAS vrijedio **+1 dB**. To nije regresija nego mjera
napretka: RCAS je kod TAAU-a popravljao *nedostatak* — Nyquist koji je resample
povijesti odnio (izvod u `docs/TAAU.md`). M5 je taj nedostatak uklonio
prilagodljivom jezgrom i lockovima, pa izoštravanju više nema što vratiti i
može samo dodavati grešku. Način `fsr-rcas` postoji kao ablacijski redak, ne kao
preporuka.

## Cijena

Sponza, RX 580, prosjek po okviru:

| Prolaz | 720p → 1080p | 1080p → 4K |
|---|---|---|
| G-buffer | 0.91 | 1.65 |
| Tonemap | 0.09 | 0.20 |
| FSR dilate | 0.25 | 0.54 |
| FSR locks | 0.06 | 0.13 |
| FSR accumulate | 0.68 | 2.64 |
| RCAS | 0.20 | 0.81 |
| Present | 0.16 | 0.57 |
| **Ukupno (bez RCAS-a)** | **2.15** | **5.73** |
| za usporedbu: nativni okvir | 1.01 | 3.48 |
| za usporedbu: M4 TAAU | 1.60 | — |

Dvije stvari treba reći otvoreno:

1. **G-buffer je poskupio s 0.79 na 0.91 ms** zbog dodatnog RG32F izlaza s
   linearnim dubinama. To je cijena toga što je depth clip utemeljen na dokazu
   umjesto na pogađanju, i plaća se u svakom okviru, dok se dobitak vidi samo na
   disokluzijama. Za +0.13 dB u gibanju to je loš omjer; za `worst window` SSIM
   i za ono što se vidi na rubovima nije.
2. **Ni ovdje upscaler ne ubrzava.** Kao i u M4, Sponza s jednostavnim forward
   shadingom nije dovoljno opterećena sjenčanjem da bi renderiranje u pola
   površine pokrilo cijenu upscalera. Dobitak M5 je kvaliteta.

### Optimizacija koja nije upalila

3×3 prozor statistike sadržan je u 4×4 Lanczos prozoru, pa se dvije petlje daju
spojiti u 16 dohvata umjesto 25. Implementirano i izmjereno: **sporije**, 0.95 ms
naspram 0.68 ms na 1080p. Test pripadnosti nije invarijantan po petlji (ovisi o
pomaku koji se računa po pikselu), pa prevoditelj izgubi potpuno odmotanu
sekvencu dohvata s konstantnim pomacima — a devet dodatnih čitanja iz keša
jeftinije je od toga. Manje memorijskih operacija nije isto što i brže. Kod je
vraćen na dvije petlje, a komentar u `fsr_accumulate.comp` čuva mjerenje da se
ista ideja ne pokuša ponovno.

Preostali prostor za ubrzanje `fsr_accumulate` prolaza (2.64 ms u 4K je
dominantan trošak): `textureGather` umjesto pojedinačnih dohvata, fp16 ondje gdje
raspon to dopušta, i spajanje devet bilinearnih Catmull-Rom dohvata. Ostaje za
kasnije — M6 (optical flow) je na kritičnom putu, ovo nije.

## Što još nije tu

**Luma piramida** iz plana (`PLAN.md`, prolaz 1) nije implementirana u M5.
U FSR2 služi auto-ekspoziciji, a ovaj pipeline tone-mapa **prije** upscalera pa
je ekspozicija već fiksirana; dodavanje prolaza koji ništa ne mijenja samo bi
poskupilo okvir. Piramida se gradi u M6, gdje je piramidalni block matching
stvarno treba, i tamo će je se moći iskoristiti i za detekciju promjene
sjenčanja.

**Reverzibilni interni tonemap** (`c/(1+max(c))`) iz plana je iz istog razloga
izostavljen: njegova je svrha suzbijanje fireflyja u linearnom HDR-u, a ovdje su
ulazne vrijednosti već u [0,1] nakon tone mappinga.
