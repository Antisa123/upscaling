# M7–M8 — generiranje međuokvira (prolazi 1–9)

Modul D iz plana. Iz dva uzastopna **upscalana** okvira, vektora gibanja iz
G-buffera i polja optical flowa (M6) gradi okvir na trenutku **t − 0,5** — onaj
koji bi se prikazao između njih. Ovaj milestone pokriva jezgru: okvir postoji i
mjeri se naspram pravog rendera istog trenutka. Prikaz u pravom trenutku
(frame pacing), kompozicija UI-ja, inpainting slike i latencija su M8.

Konvencija je ista kao u cijelom projektu: vektori su u **UV prostoru** i
pokazuju iz **trenutnog okvira u prethodni** (`prevUV = uv + mv`). Površina koja
se između t−1 i t pomaknula za `mv` bila je na pola puta u t−0,5, pa se
međuokvir nalazi na `uv + 0,5·mv` gledano iz trenutnog, odnosno iz njega se
trenutni okvir čita na `uv − 0,5·mv`, a prethodni na `uv + 0,5·mv`. Ta jedna
konvencija drži cijeli modul bez grešaka u smjeru.

## Pokretanje

```
./build/fsr3lite --validate-fg --scripted --upscaler fsr --scale 1.5 \
    --scene assets/sponza/Sponza.gltf --scene-fit 12 --path-radius 3 --path-phase -0.6 \
    --fixed-dt 0.0166667 --jitter --frames 40
```

`--validate-fg` traži `--scripted` (referenca se renderira na t − dt/2 duž
skriptirane putanje) i upscaler (modul interpolira između okvira koje upscaler
proizvodi; bez njega program odbija pokretanje umjesto da tiho mjeri nešto
drugo).

| Zastavica | Značenje |
|---|---|
| `--fg` | uključuje generiranje okvira (implicitno u `--validate-fg`); uključuje i optical flow ako se polje toka koristi |
| `--validate-fg` | ocjenjuje međuokvir i 50/50 blend istog para naspram pravog rendera na t − dt/2 |
| `--fg-game <0\|1>` | polje game vektora (zadano 1) |
| `--fg-flow <0\|1>` | polje optical flowa (zadano 1); s 0 se optical flow ni ne pokreće, osim uz `--validate-flow` |
| `--fg-masks <0\|1>` | dvije maske disokluzije (zadano 1) |
| `--fg-dilate <0\|1>` | dilatirani vektori iz upscalera umjesto sirovog `velocity` bufera (zadano 1) |
| `--fg-levels <n>` | razine piramide za popunjavanje rupa u poljima (zadano 7; 1 = bez popunjavanja) |
| `--fg-inpaint-pick <0\|1>` | pri redukciji piramide: 0 = najviši prioritet (najbliža površina), 1 = najniži (pozadina) (zadano 1) |
| `--fg-depth <t>` | relativna tolerancija dubine za primarni bit i maske (zadano 0.02) |
| `--fg-flow-bias <w>` | množitelj težine kandidata iz toka ondje gdje postoji game vektor (zadano 0.5) |
| `--fg-color-priority <0\|1>` | slaganje boja kao donjih 5 bita prioriteta scattera (zadano 0 — vidi ablacije) |
| `--fg-agreement <k>` | oštrina izbora između polja po slaganju boja (zadano 24) |
| `--fg-flow-error <e>` | pogreška podudaranja iznad koje blok toka nije „siguran” (zadano 0.05) |
| `--fg-flow-magnitude <px>` | pomak koji zasićuje prioritet po magnitudi u polju toka (zadano 64) |
| `--fg-inpaint <0\|1>` | M8: prolazi 8–9, inpainting slike iz piramide pokrivenosti (zadano 1) |
| `--fg-bounds <0\|1>` | M8: odbacivanje strane warpa čiji uzorak pada izvan ekrana (zadano 1) |
| `--fg-inpaint-coverage <c>` | M8: srednja pokrivenost koju texel piramide treba da bi se njegova boja koristila (zadano 0.3) |
| `--fg-coverage-masks <0\|1>` | M8: strana koju maska odbaci smanjuje pokrivenost (zadano 1); 0 = rupa je samo uzorak izvan ekrana |
| tipka `0` / `--debug-view 9` | interpolirani okvir |
| tipka `0` ponovno / `--debug-view 10` | crveno = maska prema prethodnom, zeleno = prema trenutnom, plavo = koliko se vektor morao popuniti iz piramide |

## Prolazi

| # | Shader | Razlučivost | Što radi |
|---|---|---|---|
| 1 | `fg_setup.comp` | render | pražnjenje pet ciljeva scattera (prioritet 0, dubina 0xFFFFFFFF) |
| 2 | `fg_depth.comp` | render | dubina međuokvira: scatter dubine na `uv + 0,5·mv`, `imageAtomicMin` |
| 3 | `fg_game_field.comp` | render | scatter game vektora s 16-bitnim prioritetom, `imageAtomicMax` |
| — | `fg_resolve_field.comp` | render | dvije `r32ui` slike → RGBA16F (vektor, prioritet, valjanost) |
| 4 | `fg_field_pyramid.comp` | ×7 | piramida polja koja preskače rupe |
| 5 | `fg_flow_field.comp` | render | isto iz optical flowa, prioritet po sigurnosti i magnitudi |
| 6 | `fg_disocclusion.comp` | render | dvije maske: međuokvir↔prethodni i međuokvir↔trenutni, iz dubine |
| 7 | `fg_interpolate.comp` | prikazna | dva warpa po polju, izbor između polja po slaganju boja; alfa = pokrivenost |
| 8 | `fg_inpaint_pyramid.comp` | ½ prikazne, ×N | piramida slike množena pokrivenošću (premultiplied), rupe ne ulaze u vlastitu ispunu |
| 9 | `fg_inpaint.comp` | prikazna | piksel s pokrivenošću < 1 miješa se s najfinijom razinom piramide koja ima dovoljno pokrivenosti |
| — | `fg_blend.comp` | prikazna | mjerenje: naivni 50/50 blend istog para |

Polja su u **render razlučivosti**: scatter tamo ima točno jedan izvorni piksel
po texelu cilja, što je gustoća na kojoj polje nije ni rasipno rijetko ni
samo-zaklanjajuće. Interpolacija radi u prikaznoj razlučivosti i polja čita
točkasto.

## Zašto scatter i atomici

Polje vektora međuokvira ne postoji ni u jednom ulazu. Game vektori opisuju
trenutni okvir, a međuokvir je drugačiji raster: površina koja se miče pojavi
se u njemu na drugom mjestu. Zato svaki izvorni piksel **piše** tamo gdje je
njegova površina u t − 0,5, a više piksela može pasti na isti texel. Pobjednika
treba izabrati bez drugog prolaza po kandidatima — to je `imageAtomicMax`, i
zato prioritet stoji u **gornjih 16 bita** iste 32-bitne riječi čiji je donji
dio 16-bitni float komponente vektora: atomik tada uspoređuje prvo prioritet, a
vrijednost samo kao razrješenje izjednačenja. Kao u `FfxFrameInterpolation`,
x i y idu u dvije zasebne slike s istim prioritetom.

Prioritet game polja, od najznačajnijeg bita:

| Bitovi | Značenje |
|---|---|
| 15 | **primarni**: ova površina je najbliža na ciljnom texelu prema prolazu 2 |
| 14–5 | dubina, bliže = više (`1023 / (1 + z)`) |
| 4–0 | slaganje boje trenutnog okvira s prethodnim na kraju vektora |

Prioritet polja toka (dubine nema):

| Bitovi | Značenje |
|---|---|
| 15 | **siguran**: pogreška podudaranja bloka ispod praga |
| 14–5 | magnituda, veća = više — pokretni objekt ispred statične pozadine |
| 4–0 | slaganje boje, isto kao gore |

Prioritet 0 znači „ništa ovdje”, pa se svaki pravi kandidat podiže na barem 1.

## Popunjavanje rupa u polju

Scatter ostavlja rupe gdje se slika rasteže, a rupa nije mjesto bez gibanja
nego mjesto u koje ništa iz prethodnog okvira nije stiglo — definicija
disokluzije. Piramida polja to rješava u log(n) koraka: potrošač koji na
razini 0 nađe rupu penje se dok neka razina nema odgovor.

Koje od (najviše četiri) valjanih djece preživi redukciju je izbor.
Zadano je **najniži prioritet**, dakle pozadina: pikseli u disokluziji
pripadaju onome što je bilo *iza* objekta koji se odmaknuo, pa ih opisuje
vektor pozadine. `--fg-inpaint-pick 0` bira suprotno; oba broja su u ablacijama.

## Maske disokluzije: koja dubina s kojim bufferom

Kamera koja se miče mijenja udaljenost svake površine između t−1 i t, pa jedan
broj ne može biti očekivana dubina za oba smjera. Prva verzija uspoređivala je
dubinu iz polja (udaljenost pobjedničke površine **u trenutku t**, jer prolaz 2
scattera dubinu trenutnog okvira) s oba buffera — ispravno prema t, pogrešno
prema t−1. Sada:

- **prema t:** dubina iz polja naspram trenutnog dubinskog buffera na kraju vektora;
- **prema t−1:** drugi kanal G-bufferove dubine (udaljenost koju je površina
  imala prošli okvir), pročitan u pikselu trenutnog okvira do kojeg vektor
  vodi, naspram prethodnog buffera.

Nebo nema geometriju: G-buffer čisti udaljenost na 10⁶, a dilatirani buffer
upscalera je half-float, gdje je to +∞. Bez posebnog slučaja ∞/∞ daje NaN,
clamp ga razriješi u „zaklonjeno”, i svaki piksel neba prestane vjerovati
ijednom okviru. Takvi texeli nemaju dokaz i obje maske su 0.

## Interpolacija: dva izbora

1. **Unutar polja** — koji od dva warpa vrijedi. To je geometrija i odgovaraju
   maske: površina skrivena u t−1 uzima se iz t i obrnuto; gdje su obje vidljive,
   koriste se obje, što usput uprosječi i pola pogreške preuzorkovanja.
2. **Između polja** — kojem vektoru vjerovati. Nakon popunjavanja oba polja
   imaju odgovor za svaki piksel, pa odlučuje dokaz: koliko se dva kraja vektora
   slažu u boji. Točan vektor dovede istu površinu na samu sebe i daje dva gotovo
   ista warpa; pogrešan daje dvije različite slike i time se sam prijavi.
   Težina je `exp(−k · udaljenost boja)`; gdje je upotrebljiv samo jedan smjer,
   drugog mišljenja nema i težina je srednja (0,5).

Gdje nijedno polje nema ništa, pada se na 50/50 blend i alfa kanal se označi
za M8 inpainting. Prvi okvir i okvir nakon reza su kopija trenutnog: blend dviju
nepovezanih slika bio bi strogo gori od dvaput prikazanog istog okvira.

## Referenca i metodologija

Referenca je **pravi render trenutka t − dt/2**, dobiven istim postupkom kao
`mid_*.png` u `docs/GROUND_TRUTH.md`: scena i kamera pomaknu se na
`sceneTime − 0,5·dt` bez bilježenja povijesti, renderira se bez jittera u 2×2
supersamplingu, razriješi i tonemapira, pa vrate. Uspoređuje se u
display-referred prostoru bez drugog tonemapa.

Uz međuokvir se svaki okvir mjeri i **50/50 blend istog para ulaznih okvira**
naspram iste reference. To je cijena nečinjenja, i jedina poštena usporedba:
ista razlučivost, isti upscaler, isti trenutak. Stupac `blend` u
`docs/GROUND_TRUTH.md` (28,85 dB) računat je nad nativnim referentnim okvirima
na drugoj orbiti i služi samo kao orijentir.

Rezovi se ne ocjenjuju: preko reza nema gibanja duž kojeg bi se interpoliralo,
modul to i kaže padom na kopiju, a ocjena te kopije protiv okvira s drugog
kraja scene mjerila bi scenu, ne modul.

Sponza je statična geometrija, pa su game vektori na njoj **egzaktni**. Polje
toka tamo po konstrukciji može samo izgubiti; zato ablacije idu i na
proceduralnoj sceni s kutijama koje kruže i vrte se oko stupova.

## Rezultati

Sve na Sponzi, kanonski kadar (`--scene-fit 12 --path-radius 3 --path-phase -0.6`),
FSR u pipelineu, jitter uključen. „Blend” je 50/50 blend istog para ulaznih
okvira naspram iste reference. „min” je najgori pojedinačni okvir.

Tablice u ovom odjeljku su stanje na kraju M7 (prolazi 1–7). M8 je dodao
prolaze 8–9 i odbacivanje uzoraka izvan ekrana, što zadanu konfiguraciju
pomiče za +0,06 dB (34,94 → 35,00); vidi „Inpainting slike (M8)” niže. Redak
„kao M7” u `fg-ablation` reproducira 34,94 do stotinke, pa su M7 brojke i dalje
točne za M7 konfiguraciju.

### Po razlučivosti (60 fps, okviri 16–120)

| Konfiguracija | PSNR | min | SSIM | PSNR blend | SSIM blend | dobitak |
|---|---|---|---|---|---|---|
| 1080p, render 1280×720 (Quality) | **34,94** | 27,56 | 0,9438 | 26,11 | 0,6775 | +8,83 dB |
| 1080p native (FSR 1,0×) | 36,43 | 27,92 | 0,9623 | 26,14 | 0,6814 | +10,29 dB |
| 1080p, render 960×540 (Performance) | 33,52 | 27,51 | 0,9162 | 26,16 | 0,6817 | +7,36 dB |
| 1280×720, render 854×480 | 33,92 | 27,50 | 0,9289 | 26,67 | 0,6982 | +7,25 dB |

Donja granica iz `docs/GROUND_TRUTH.md` (28,85 dB) prijeđena je u svakom retku,
a in-engine blend na istim ulazima za 7–10 dB. Kvaliteta međuokvira prati
kvalitetu ulaza: bolji upscaling daje bolje warpove, pa native vodi, a
Performance zaostaje.

### Po brzini kamere (1080p Quality; 0,5 s zagrijavanja + 1 s mjerenja)

| Kamera | PSNR | min | SSIM | PSNR blend | dobitak |
|---|---|---|---|---|---|
| mirna | 40,14 | 40,02 | 0,9832 | 40,14 | 0,00 dB |
| 120 fps | 35,90 | 31,21 | 0,9551 | 27,17 | +8,73 dB |
| 60 fps | 34,58 | 27,60 | 0,9468 | 25,56 | +9,02 dB |
| 30 fps | 32,80 | 24,01 | 0,9299 | 24,06 | +8,74 dB |
| 20 fps | 31,09 | 20,33 | 0,9112 | 23,08 | +8,01 dB |

Mirna kamera je provjera ispravnosti: kad se ništa ne miče blend je već
egzaktan i modul ga ne smije pokvariti — i ne pokvari, do stotinke. Srednji
dobitak je gotovo neovisan o brzini, ali **najgori okvir** nije: pri 30 fps
pada na razinu srednje vrijednosti blenda, a pri 20 fps ispod nje. Pretpostavka
linearnog gibanja i sve šire disokluzije tu prestaju vrijediti, a to je upravo
posao inpaintinga slike (M8). Redak 60 fps razlikuje se od tablice po
razlučivosti (34,58 naspram 34,94) jer mjeri drugi komad putanje; vidi
`docs/METRICS.md` o prozoru u vremenu scene.

### Ablacije (60 fps, 1080p Quality)

Proceduralna scena ima kutije koje kruže i vrte se pored stupova; na njoj game
vektori opisuju i gibanje objekata, a ne samo kamere.

| Konfiguracija | Sponza | min | proceduralna | min |
|---|---|---|---|---|
| sve zadano | **34,94** | 27,56 | **34,07** | 33,73 |
| samo game vektori | 34,80 | 26,69 | 33,68 | 33,34 |
| samo optical flow | 33,74 | 25,88 | 33,53 | 32,79 |
| bez vektora (= blend) | 26,11 | 21,39 | 31,61 | 31,13 |
| bez maski disokluzije | 34,85 | 27,38 | 34,10 | 33,75 |
| bez dilatiranih vektora | 34,93 | 27,64 | — | — |
| piramida: najbliži umjesto pozadine | 34,94 | 27,57 | 34,07 | 33,73 |
| s bojom u prioritetu scattera | 34,94 | 27,56 | 34,07 | 33,73 |

Redak „bez vektora” reproducira stupac blenda do stotinke na obje scene, što
je provjera samog mjernog puta.

**Dva polja se nadopunjuju.** Nijedno samo ne doseže kombinaciju. Na Sponzi,
gdje su game vektori egzaktni, tok gotovo ne mijenja srednju vrijednost
(+0,14 dB), ali najgori okvir podigne za 0,87 dB. Na proceduralnoj sceni
dodaje +0,39 dB srednje.

**Maske** donose +0,09 dB srednje i +0,18 dB u najgorem okviru na Sponzi, a na
proceduralnoj sceni −0,03 dB. Učinak je malen jer su pri 60 fps disokluzije
trake široke nekoliko piksela; debug prikaz (`--debug-view 10`) ih pokazuje
točno uz siluete. Zadržane su, a razlika je iskazana ovakva kakva jest.

**Dilatacija, izbor djeteta u piramidi i tolerancija dubine** nemaju mjerljiv
učinak na ovim scenama (unutar ±0,02 dB). Razlog je isti za sve tri: rupe u
polju na razini 0 široke su jedan do dva texela, pa u 2×2 redukciji gotovo
uvijek postoji susjed s iste površine i izbor ne mijenja vektor. Mehanizmi
postoje kao u FSR3; na ovim scenama nemaju što popraviti, i to je rezultat.

**Boja u prioritetu scattera** ne mijenja ništa ni na jednoj sceni, a košta
0,37 ms (4,69 naspram 4,33 ms okvira): to je jedini dio scattera koji čita
slike u prikaznoj razlučivosti, a presuđuje samo izjednačenja koja su primarni
bit i dubina već riješili. Zato je po zadanom isključena.

### Parametri

| Parametar | vrijednosti | Sponza PSNR (min) | proceduralna |
|---|---|---|---|
| težina toka uz game vektor | 1 / **0,5** / 0,25 / 0,1 | 34,79 (27,58) / **34,94 (27,56)** / 34,99 (27,46) / 34,97 (27,26) | 34,09 / **34,07** / 34,03 / 33,97 |
| razine piramide polja | 1 / 3 / 5 / **7** | 34,60 / 34,80 / 34,90 / **34,94** | — |
| oštrina slaganja boja | 0 / 6 / **24** / 96 | 34,83 (27,77) / 34,92 (27,70) / **34,94 (27,56)** / 34,91 (27,36) | — |
| tolerancija dubine | 0,005 / **0,02** / 0,08 | 34,93 / **34,94** / 34,95 | — |

**Težina toka** postoji zbog kvara koji slaganje boja ne može vidjeti. Na
periodičnoj teksturi (prugasti stup, popločan pod) vektor pomaknut za točno
jedan period stavi uzorak sam na sebe, pa se njegova dva warpa slažu jednako
dobro kao i ispravna. Figura na proceduralnoj sceni pokazivala je sive mrlje na
gornjim dijelovima tankih prugastih stupova koje dolaze isključivo iz polja
toka: bez njega ih nema, a samo s tokom su izraženije. Game vektori tu grešku
ne mogu napraviti, pa izjednačenje ide geometriji. S težinom 0,5 mrlje su
bitno blijeđe, ali ne i nestale (`captures/framegen/proc/fg_error.png`): gdje
game vektor na tom mjestu nema bolje slaganje boja, tok i dalje dobije pola
glasa. Potpuno rješenje traži provjeru periodičnosti u samom block matchingu i
ostaje otvoreno. Vrijednost 0,5 je
kompromis: niže još malo diže srednju vrijednost Sponze, ali gubi njezin
najgori okvir i srednju vrijednost proceduralne scene.

**Oštrina slaganja** mijenja srednju vrijednost za najgori okvir: oštriji izbor
između polja bolje prati prosječan piksel, a mekši bolje podnosi okvir u kojem
su oba polja podjednako loša.

## Inpainting slike (M8)

Prolaz 7 u alfa kanal piše **pokrivenost**: koliko piksela dolazi iz uzorka
koji je warp smio uzeti. Strana warpa čiji uzorak pada izvan ekrana
(`--fg-bounds`) ili koju maska odbaci ne doprinosi joj. Prolaz 8 gradi piramidu
slike množene pokrivenošću (od pola prikazne razlučivosti naviše), pa rupe ne
ulaze u vlastitu ispunu; prolaz 9 svaki piksel s pokrivenošću < 1 miješa s
najfinijom razinom čija srednja pokrivenost prelazi prag. Potpuno pokriven
piksel ostaje bit-identičan.

| Konfiguracija | 60 fps | min | proceduralna | 30 fps | 20 fps | min |
|---|---|---|---|---|---|---|
| zadano (M8) | **35,00** | 27,56 | 34,06 | **32,85** | 30,95 | 20,23 |
| bez inpaintinga slike | 34,98 | 27,57 | 34,07 | — | 30,99 | 20,15 |
| bez provjere granica | 34,97 | 27,66 | 34,06 | — | 31,05 | 20,44 |
| kao M7 (oboje isključeno) | 34,94 | 27,56 | 34,07 | 32,80 | **31,09** | 20,33 |
| pokrivenost bez maski | 35,00 | 27,56 | 34,07 | 32,85 | 30,95 | 20,23 |
| prag pokrivenosti 0,1 / 0,3 / 0,6 | 35,00 / 35,00 / 34,99 | | | | | |

Retci „bez inpaintinga” i „bez provjere granica” pri 20 fps i redak „pokrivenost
bez maski” mjereni su izvan `run_metrics.py`, s istim zastavicama i istim
prozorom kao `fg-speed`.

**Učinak je malen i nije jednoznačan.** Pri 60 i 30 fps oba mehanizma zajedno
donose +0,05–0,06 dB, na proceduralnoj sceni ništa (−0,01), a pri 20 fps
**gube** 0,14 dB, većinom zbog provjere granica. Razlog je u prolazu 4: piramida
polja već svakom pikselu da vektor, pa rupe u slici ostaju samo gdje warp izađe
iz ekrana ili ga obje maske odbace — traka uz rub kadra koju kamera upravo
otkriva i nekoliko piksela uz siluete. Na toj traci uzorak ruba ekrana
(clamp-to-edge) često je nastavak iste teksture i nije gori od prosjeka iz
piramide, a pri 20 fps, gdje je traka najšira, izbacivanje jedne strane warpa
gubi i usrednjavanje dviju strana koje inače smanjuje pogrešku resamplinga.
Pokrivenost s maskama ili bez njih daje isto do stotinke — dvostruko maskiranih
piksela je premalo da bi se vidjeli u prosjeku.

Prolazi ostaju uključeni jer su dio FSR3 lanca, pri preporučenoj baznoj brzini
(≥ 60 fps) ne štete, a koštaju 0,235 ms; `--fg-inpaint 0 --fg-bounds 0` vraća
M7 ponašanje. Rezultat je iskazan kakav jest: na ovim scenama inpainting slike
nema što popraviti, a na vrlo niskoj baznoj brzini malo odmaže.

## Cijena

GPU vrijeme u ms na RX 580, orbita 60 fps, 120 okvira, bez validacije.
Tablica je stanje M7; M8 generiranju dodaje 0,235 ms za prolaze 8–9 (1,593 →
1,829 ms na 1080p Quality), a pravi prikaz s generiranjem ima dva prolaza
kompozicije umjesto jednog (0,255 naspram 0,127 ms).
„Pipeline” je okvir bez generiranja (G-buffer, tonemap, FSR, present).

| Konfiguracija | pipeline | optical flow | generiranje | generiranje bez toka | ukupno s generiranjem |
|---|---|---|---|---|---|
| 1080p Quality | 2,165 | 0,616 | **1,593** | 1,242 | 4,342 |
| 1080p native | 2,902 | 0,578 | 2,820 | 2,118 | 6,269 |
| 1080p Performance | 1,783 | 0,592 | 1,132 | 0,924 | 3,480 |
| 720p Quality | 1,103 | 0,318 | 0,713 | 0,555 | 2,148 |

Po prolazu, 1080p Quality (polja 1280×720):

| Prolaz | ms |
|---|---|
| 1 setup | 0,093 |
| 2 dubina međuokvira | 0,083 |
| 3+4 game polje (scatter, razrješenje, piramida) | 0,304 |
| 5+4 polje toka (scatter, razrješenje, piramida) | 0,250 |
| 6 maske disokluzije | 0,160 |
| 7 interpolacija (prikazna razlučivost) | 0,539 |
| kopija okvira za idući par | ≈0,16 |

Prolazi nad poljima skaliraju s render razlučivošću, interpolacija s prikaznom:
zato Performance štedi na poljima (0,17 umjesto 0,30 ms), a interpolacija ostaje
gotovo ista.

**Isplati li se.** Međuokvir s tokom košta 1,59 + 0,62 = 2,21 ms, a pravi okvir
ovog pipelinea 2,17 ms. Na ovoj testnoj sceni generiranje, dakle, **ne donosi
FPS**: G-buffer Sponze traje 0,9 ms, dok je u stvarnoj igri okvir 10–30 ms.
Cijena generiranja ne ovisi o složenosti scene nego samo o razlučivosti, pa je
prag isplativosti na 1080p Quality oko 2,2 ms rendera s tokom, odnosno 1,2 ms
bez njega (uz 0,14 dB manje i najgori okvir niži za 0,87 dB). Svaka scena teža
od ove je iznad praga. Stvarni prikazani FPS, ravnomjernost vremena prikaza i
latencija izmjereni su u M8 (`docs/PACING.md`): na neopterećenoj sceni FG
gubi (313 → 308 fps), uz sintetsko opterećenje od ×4 naviše prikazani FPS raste
1,36–1,58×.

## Figure

- `captures/framegen/fg_compare.png`: t−1, t, pravi međuokvir, interpolirani,
  blend i maske (Sponza, 30 fps, okvir 40). Blend udvostručuje svaki rub, a
  interpolirani okvir ne; greška koja ostaje koncentrirana je na visokofrekventnoj
  tkanini zastora i na lišću.
- `captures/framegen/fg_error.png`: |interpolirani − referenca| i
  |blend − referenca|, pojačano 4×.
- `captures/framegen/proc/`: isto na proceduralnoj sceni.
- `captures/framegen/proc-noflow/`, `proc-nogame/`: dijagnoza mrlja na stupovima,
  snimljena s težinom toka 1 — bez polja toka mrlja nema, samo s tokom su jače.

## Reprodukcija

```
python3 scripts/run_metrics.py --group fg fg-ablation fg-speed fg-hud
scripts/fg_debug.py                  # captures/framegen/fg_compare.png, fg_error.png
```
