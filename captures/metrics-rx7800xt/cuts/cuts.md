# Detekcija promjene scene (prag 0.25)

Najmanja udaljenost koju je proizveo pravi rez naspram najvece koju je proizvelo obicno gibanje kamere. Detektor je upotrebljiv kad je prvi broj veci od drugog; prag pripada izmedu njih.

| Orbita | Statistika | min(rez) | max(gibanje) | margina | detektirano rezova | laznih pozitiva |
|---|---|---|---|---|---|---|
| mjerna orbita (r=3, faza -0.6) | maksimum | 0.568 | 0.463 | +0.105 | 15/15 | 11/105 |
| mjerna orbita (r=3, faza -0.6) | srednja | 0.253 | 0.207 | +0.046 | 15/15 | 0/105 |
| mjerna orbita (r=3, faza -0.6) | medijan | 0.232 | 0.179 | +0.053 | 14/15 | 0/105 |
| siroka orbita (r=5) | maksimum | 0.963 | 0.830 | +0.133 | 15/15 | 9/105 |
| siroka orbita (r=5) | srednja | 0.691 | 0.306 | +0.385 | 15/15 | 4/105 |
| siroka orbita (r=5) | medijan | 0.609 | 0.366 | +0.244 | 15/15 | 2/105 |
| vanjska orbita (r=11) | maksimum | 0.974 | 0.026 | +0.948 | 15/15 | 0/105 |
| vanjska orbita (r=11) | srednja | 0.342 | 0.012 | +0.330 | 15/15 | 0/105 |
| vanjska orbita (r=11) | medijan | 0.211 | 0.010 | +0.202 | 12/15 | 0/105 |
