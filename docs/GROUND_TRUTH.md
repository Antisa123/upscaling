# Ground truth: referentni okviri za upscaling i generiranje okvira

Kvaliteta u ovom radu nije stvar dojma nego brojke, a brojka postoji samo ako
postoji referenca. Vlastiti renderer to omogućuje: istu scenu možemo renderirati
i u punoj rezoluciji (referenca za upscaler) i u trenutku *između* dva okvira
(referenca za generiranje okvira). Gotov engine to ne bi dopustio.

## Snimanje

```bash
./build/fsr3lite \
    --width 1280 --height 720 --scale 2.0 \
    --scene assets/sponza/Sponza.gltf --scene-fit 30 \
    --scripted --fixed-dt 0.016667 --frames 120 \
    --capture-gt captures/gt
```

`--scripted` i `--fixed-dt` su obavezni: bez njih kamera ovisi o stvarnom
proteklom vremenu pa dva pokretanja ne daju istu sekvencu, a referenca koja se
ne može ponoviti nije referenca.

Nastaje po okviru:

| Datoteka | Što je | Za što je referenca |
|---|---|---|
| `gt_%04d.png` | puna rezolucija, **bez jittera**, trenutak `t` | ono što bi upscaler trebao proizvesti |
| `mid_%04d.png` | puna rezolucija, bez jittera, trenutak `t - dt/2` | ono što generiranje okvira mora pogoditi |
| `lr_%04d.png` | render rezolucija, onakva kakva ulazi u upscaler | ulaz, radi vizualne usporedbe |
| `manifest.csv` | `index,time_s,mid_time_s,render_res,display_res` | vremenska os snimke |

Prva dva okvira se preskaču (`kGtWarmup = 2`): među-okvir treba prethodnika
između kojeg će ležati, a povijest prvog okvira je rez, ne gibanje.

## Kako se među-okvir dobiva

Scena se pomakne na `sceneTime - 0.5 * dt`, renderira, pa vrati natrag. Pomak
ide kroz `Scene::update(t, /*recordHistory=*/false)` — bez toga bi se
`prevModel` svake instance prepisao među-vremenom i motion vektori idućeg
*pravog* okvira bili bi upola prekratki. Kamera se kopira (`midCamera`) iz istog
razloga.

Referentni okviri se renderiraju bez jittera (`GBuffer::render(..., useJitter=false)`):
referenca ne smije imati sub-pixel pomak koji bi se poslije morao poništavati.

Trošak: drugi G-buffer u punoj rezoluciji, pa se stvara samo kad se traži. Na
1080p su to dva dodatna passa po okviru (~0,38 ms svaki na RX 580) i tri PNG-a.

## Provjera

```bash
scripts/check_gt.py captures/gt
```

Skripta provjerava ono što mora vrijediti ako je među-okvir stvarno na pola puta:
da je podjednako udaljen od oba susjeda i da je bližoj svakom od njih nego što su
susjedi jedan drugome. Izlaz sa Sponze (720p, dt = 1/60 s):

```
 idx    gt-gt  mid-gt0  mid-gt1    asim    blend  ocjena
   1    25.03    26.80    26.82    0.01    28.85  ok
   ...
5/5 trojaca zadovoljava uvjete
```

Asimetrija od 0,01 dB znači da među-okvir leži točno na sredini.

Stupac `blend` je PSNR naivnog 50/50 blenda dva susjeda naspram pravog
među-okvira — **28,85 dB je donja granica koju generiranje okvira (M7) mora
nadmašiti** da bi uopće imalo smisla. Blend je usporediv s reprojekcijom jer i
jedno i drugo koristi isti par okvira; razlika je što blend ne zna ništa o
gibanju.
