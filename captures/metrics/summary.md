# Metrike

## Motion vectori: reprojekcija prethodnog okvira

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR reproj. (dB) | SSIM reproj. | PSNR bez reproj. (dB) | SSIM bez reproj. | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Native 1920x1080 | 104 | 5.86 | 0.37 | 30.93 | 0.9736 | 25.53 | 0.8882 | 0.381 | 2690 |
| Native 960x540 | 104 | 2.14 | 0.15 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.154 | 6632 |
| 1080p, render 1.5x manji | 104 | 2.96 | 0.23 | 30.62 | 0.9674 | 25.87 | 0.8917 | 0.237 | 4300 |
| 1080p, render 2.0x manji | 104 | 1.99 | 0.19 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.192 | 5327 |
| 960x540, sporo gibanje (dt/4) | 104 | 1.97 | 0.15 | 35.04 | 0.9859 | 32.93 | 0.9782 | 0.155 | 6590 |
| 960x540, brzo gibanje (dt*2) | 104 | 2.21 | 0.15 | 27.67 | 0.9507 | 22.74 | 0.8046 | 0.155 | 6599 |
| 960x540, bez filtriranja uzorka | 104 | 2.09 | 0.15 | 27.90 | 0.9259 | 24.59 | 0.8563 | 0.151 | 6732 |

## Cijena renderiranja bez upscalera (referentne brojke za ubrzanje)

| Konfiguracija | Frameovi | CPU ms | GPU ms | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|
| Sponza: native 1920x1080 | 104 | 0.27 | 1.41 | 1.595 | 707 |
| Sponza: samo render 1280x720 | 104 | 0.35 | 0.73 | 0.819 | 1371 |
| Sponza: samo render 960x540 | 104 | 0.28 | 0.50 | 0.554 | 2001 |
| Sponza: native 3840x2160 | 104 | 0.25 | 4.82 | 5.604 | 207 |
| Sponza: samo render 1920x1080 | 104 | 0.26 | 1.58 | 1.745 | 633 |

## Prostorni upscaleri (M3)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Nearest, Quality 1.5x | 104 | 6.27 | 0.51 | 30.08 | 0.9438 | 27.47 | 30.49 | 0.513 | 1974 |
| Sponza: Nearest, Quality 1.5x | 104 | 10.94 | 1.07 | 32.15 | 0.8966 | 29.48 | 34.05 | 1.158 | 935 |
| Bilinear, Quality 1.5x | 104 | 6.22 | 0.51 | 33.29 | 0.9678 | 30.02 | 30.49 | 0.519 | 1952 |
| Sponza: Bilinear, Quality 1.5x | 104 | 10.85 | 1.07 | 35.25 | 0.9333 | 33.76 | 34.05 | 1.161 | 934 |
| Bicubic, Quality 1.5x | 104 | 6.27 | 0.56 | 32.84 | 0.9662 | 29.05 | 30.49 | 0.570 | 1774 |
| Sponza: Bicubic, Quality 1.5x | 104 | 10.82 | 1.13 | 35.37 | 0.9420 | 32.47 | 34.05 | 1.214 | 889 |
| FSR1 EASU+RCAS, Quality 1.5x | 104 | 6.61 | 0.88 | 31.13 | 0.9548 | 27.56 | 30.49 | 0.883 | 1140 |
| Sponza: FSR1 EASU+RCAS, Quality 1.5x | 104 | 11.16 | 1.46 | 33.09 | 0.9117 | 30.01 | 34.05 | 1.534 | 685 |
| Nearest, Performance 2.0x | 104 | 6.05 | 0.41 | 29.30 | 0.9323 | 26.94 | 30.44 | 0.412 | 2458 |
| Sponza: Nearest, Performance 2.0x | 104 | 10.51 | 0.84 | 31.59 | 0.8794 | 29.05 | 34.00 | 0.888 | 1195 |
| Bilinear, Performance 2.0x | 104 | 6.08 | 0.42 | 31.26 | 0.9479 | 29.83 | 30.44 | 0.421 | 2396 |
| Sponza: Bilinear, Performance 2.0x | 104 | 10.52 | 0.85 | 33.11 | 0.8895 | 33.08 | 34.00 | 0.897 | 1183 |
| Bicubic, Performance 2.0x | 104 | 6.13 | 0.47 | 30.83 | 0.9467 | 28.54 | 30.44 | 0.476 | 2127 |
| Sponza: Bicubic, Performance 2.0x | 104 | 10.57 | 0.90 | 33.13 | 0.8997 | 31.40 | 34.00 | 0.953 | 1109 |
| FSR1 EASU+RCAS, Performance 2.0x | 104 | 6.83 | 0.80 | 30.05 | 0.9441 | 27.33 | 30.44 | 0.802 | 1256 |
| Sponza: FSR1 EASU+RCAS, Performance 2.0x | 104 | 10.96 | 1.23 | 31.92 | 0.8785 | 29.40 | 34.00 | 1.282 | 812 |
| Sponza 4K: Nearest, Performance 2.0x | 104 | 35.66 | 2.72 | 33.81 | 0.9193 | 30.35 | 34.19 | 2.881 | 367 |
| Sponza 4K: Bilinear, Performance 2.0x | 104 | 35.51 | 2.82 | 35.55 | 0.9333 | 33.88 | 34.19 | 2.988 | 354 |
| Sponza 4K: Bicubic, Performance 2.0x | 104 | 35.69 | 2.99 | 35.71 | 0.9416 | 32.52 | 34.19 | 3.156 | 334 |
| Sponza 4K: FSR1 EASU+RCAS, Performance 2.0x | 104 | 36.90 | 4.34 | 34.30 | 0.9232 | 30.72 | 34.19 | 4.495 | 231 |

## RCAS: ablacija ostrine (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR1, sharpness 0 | 104 | 11.40 | 1.45 | 30.89 | 0.8675 | 28.57 | 34.05 | 1.533 | 690 |
| FSR1, sharpness 0.25 | 104 | 11.24 | 1.44 | 33.09 | 0.9117 | 30.01 | 34.05 | 1.529 | 692 |
| FSR1, sharpness 0.5 | 104 | 11.23 | 1.44 | 34.08 | 0.9274 | 30.75 | 34.05 | 1.531 | 693 |
| FSR1, sharpness 1 | 104 | 11.21 | 1.44 | 34.88 | 0.9374 | 31.52 | 34.05 | 1.532 | 694 |
| FSR1, sharpness 2 | 104 | 11.18 | 1.44 | 35.28 | 0.9407 | 32.13 | 34.05 | 1.528 | 695 |
| FSR1, sharpness 4 | 104 | 11.30 | 1.44 | 35.40 | 0.9408 | 32.46 | 34.05 | 1.529 | 695 |
| FSR1, EASU bez RCAS-a | 104 | 11.26 | 1.44 | 35.42 | 0.9406 | 32.54 | 34.05 | 1.524 | 695 |

## Temporalni upscaler (M4): TAAU

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, Quality 1.5x | 104 | 6.50 | 0.77 | 32.66 | 0.9657 | 32.11 | 30.48 | 0.776 | 1298 |
| Sponza: TAAU, Quality 1.5x | 104 | 11.35 | 1.61 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.693 | 623 |
| TAAU + RCAS, Quality 1.5x | 104 | 6.71 | 0.97 | 32.55 | 0.9662 | 31.67 | 30.48 | 0.975 | 1034 |
| Sponza: TAAU + RCAS, Quality 1.5x | 104 | 11.55 | 1.80 | 35.25 | 0.9505 | 35.99 | 34.00 | 1.895 | 555 |
| TAAU, Performance 2.0x | 104 | 6.37 | 0.69 | 31.56 | 0.9551 | 32.22 | 30.45 | 0.690 | 1459 |
| Sponza: TAAU, Performance 2.0x | 104 | 11.08 | 1.37 | 33.80 | 0.9203 | 36.90 | 33.96 | 1.413 | 731 |
| TAAU + RCAS, Performance 2.0x | 104 | 6.59 | 0.88 | 31.48 | 0.9558 | 31.75 | 30.45 | 0.888 | 1133 |
| Sponza: TAAU + RCAS, Performance 2.0x | 104 | 11.30 | 1.55 | 33.94 | 0.9271 | 36.00 | 33.96 | 1.608 | 645 |

## TAAU: ablacije (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, sve zadano | 104 | 11.43 | 1.60 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.697 | 623 |
| bez jittera (nema sto akumulirati) | 104 | 11.35 | 1.61 | 34.90 | 0.9438 | 37.29 | 34.05 | 1.694 | 623 |
| akumulacija max 1 okvira | 104 | 11.34 | 1.61 | 35.24 | 0.9461 | 36.72 | 34.00 | 1.694 | 623 |
| akumulacija max 2 okvira | 104 | 11.35 | 1.61 | 35.22 | 0.9460 | 36.78 | 34.00 | 1.694 | 623 |
| akumulacija max 4 okvira | 104 | 11.34 | 1.60 | 35.19 | 0.9457 | 36.86 | 34.00 | 1.694 | 623 |
| akumulacija max 8 okvira | 104 | 11.71 | 1.61 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.691 | 622 |
| akumulacija max 16 okvira | 104 | 11.36 | 1.61 | 35.03 | 0.9438 | 37.03 | 34.00 | 1.695 | 622 |
| akumulacija max 32 okvira | 104 | 11.37 | 1.60 | 34.89 | 0.9422 | 37.10 | 34.00 | 1.693 | 623 |
| clamp 0.5 sigma | 104 | 11.34 | 1.60 | 34.58 | 0.9212 | 36.24 | 34.00 | 1.697 | 623 |
| clamp 1 sigma | 104 | 11.37 | 1.60 | 35.29 | 0.9411 | 36.72 | 34.00 | 1.692 | 623 |
| clamp 1.5 sigma | 104 | 11.38 | 1.61 | 35.22 | 0.9448 | 36.88 | 34.00 | 1.693 | 623 |
| clamp 2 sigma | 104 | 11.38 | 1.60 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.693 | 623 |
| clamp 4 sigma | 104 | 11.39 | 1.61 | 35.08 | 0.9447 | 36.96 | 34.00 | 1.695 | 623 |
| rekonstrukcija 1/sigma^2 = 2 | 104 | 11.35 | 1.61 | 34.59 | 0.9244 | 37.97 | 34.00 | 1.696 | 622 |
| rekonstrukcija 1/sigma^2 = 4 | 104 | 11.39 | 1.61 | 35.25 | 0.9428 | 37.08 | 34.00 | 1.695 | 622 |
| rekonstrukcija 1/sigma^2 = 8 | 104 | 11.34 | 1.62 | 34.89 | 0.9440 | 37.03 | 34.00 | 1.696 | 618 |
| rekonstrukcija 1/sigma^2 = 16 | 104 | 11.38 | 1.61 | 33.96 | 0.9341 | 37.50 | 34.00 | 1.695 | 623 |
| skracivanje povijesti 0/px | 104 | 11.39 | 1.60 | 32.64 | 0.9071 | 38.58 | 34.00 | 1.696 | 623 |
| skracivanje povijesti 0.4/px | 104 | 11.36 | 1.60 | 34.56 | 0.9385 | 37.56 | 34.00 | 1.686 | 623 |
| skracivanje povijesti 0.8/px | 104 | 11.41 | 1.60 | 34.93 | 0.9428 | 37.19 | 34.00 | 1.694 | 623 |
| skracivanje povijesti 1.6/px | 104 | 11.37 | 1.62 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.699 | 618 |
| skracivanje povijesti 3.2/px | 104 | 11.38 | 1.61 | 35.21 | 0.9458 | 36.80 | 34.00 | 1.694 | 622 |
| mip bias 0 | 104 | 10.98 | 1.27 | 34.53 | 0.9284 | 37.42 | 34.01 | 1.355 | 790 |
| mip bias -0.585 | 104 | 11.10 | 1.34 | 35.07 | 0.9418 | 37.22 | 34.00 | 1.429 | 748 |
| mip bias -1.585 | 104 | 11.36 | 1.61 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.692 | 623 |
| TAAU + RCAS, ostrina 0.4 | 104 | 11.65 | 1.80 | 34.60 | 0.9440 | 34.58 | 34.00 | 1.890 | 554 |
| TAAU + RCAS, ostrina 0.8 | 104 | 11.58 | 1.80 | 35.14 | 0.9502 | 35.51 | 34.00 | 1.891 | 555 |
| TAAU + RCAS, ostrina 1.0 | 104 | 11.58 | 1.80 | 35.22 | 0.9506 | 35.78 | 34.00 | 1.891 | 555 |
| TAAU + RCAS, ostrina 1.2 | 104 | 11.59 | 1.80 | 35.25 | 0.9505 | 35.99 | 34.00 | 1.892 | 555 |
| TAAU + RCAS, ostrina 1.5 | 104 | 11.59 | 1.80 | 35.26 | 0.9498 | 36.23 | 34.00 | 1.891 | 554 |
| TAAU + RCAS, ostrina 2.0 | 104 | 11.61 | 1.80 | 35.24 | 0.9486 | 36.48 | 34.00 | 1.888 | 555 |

## TAAU: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| bicubic, mirna kamera | 104 | 10.82 | 1.14 | 35.83 | 0.9441 | 82.98 | 84.55 | 1.147 | 879 |
| TAAU, mirna kamera | 104 | 11.41 | 1.62 | 39.36 | 0.9723 | 57.31 | 84.55 | 1.639 | 616 |
| TAAU + RCAS, mirna kamera | 104 | 11.57 | 1.83 | 40.39 | 0.9819 | 55.11 | 84.55 | 1.842 | 548 |
| bicubic, 120 fps | 104 | 11.54 | 1.17 | 37.18 | 0.9596 | 35.57 | 37.70 | 1.217 | 854 |
| TAAU, 120 fps | 104 | 11.78 | 1.65 | 37.50 | 0.9643 | 41.23 | 37.61 | 1.698 | 604 |
| TAAU + RCAS, 120 fps | 104 | 12.05 | 1.85 | 37.71 | 0.9690 | 40.03 | 37.61 | 1.897 | 540 |
| bicubic, 60 fps | 104 | 10.84 | 1.14 | 35.37 | 0.9420 | 32.47 | 34.05 | 1.214 | 877 |
| TAAU, 60 fps | 104 | 11.44 | 1.61 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.692 | 623 |
| TAAU + RCAS, 60 fps | 104 | 11.60 | 1.80 | 35.25 | 0.9505 | 35.99 | 34.00 | 1.892 | 554 |
| bicubic, 30 fps | 104 | 10.39 | 1.08 | 35.27 | 0.9392 | 30.02 | 30.82 | 1.197 | 928 |
| TAAU, 30 fps | 104 | 10.85 | 1.54 | 34.36 | 0.9372 | 32.88 | 30.79 | 1.687 | 651 |
| TAAU + RCAS, 30 fps | 104 | 11.09 | 1.73 | 34.40 | 0.9421 | 32.36 | 30.79 | 1.878 | 577 |

## Puni upscaler (M5): dilatacija, depth clip, lockovi, Lanczos

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Sponza: FSR, NativeAA 1.0x | 104 | 12.84 | 2.95 | 38.16 | 0.9745 | 34.88 | 34.19 | 3.143 | 339 |
| Sponza: FSR + RCAS, NativeAA 1.0x | 104 | 12.95 | 3.15 | 37.63 | 0.9716 | 34.46 | 34.19 | 3.335 | 318 |
| Sponza: FSR, Quality 1.5x | 104 | 12.07 | 2.18 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.266 | 460 |
| Sponza: FSR + RCAS, Quality 1.5x | 104 | 12.20 | 2.37 | 35.04 | 0.9412 | 33.71 | 34.00 | 2.459 | 421 |
| Sponza: FSR, Balanced 1.7x | 104 | 11.82 | 2.01 | 34.71 | 0.9339 | 34.01 | 33.99 | 2.079 | 497 |
| Sponza: FSR + RCAS, Balanced 1.7x | 104 | 12.01 | 2.20 | 34.36 | 0.9298 | 33.60 | 33.99 | 2.267 | 455 |
| Sponza: FSR, Performance 2.0x | 104 | 11.61 | 1.79 | 33.74 | 0.9148 | 33.79 | 33.96 | 1.849 | 558 |
| Sponza: FSR + RCAS, Performance 2.0x | 104 | 11.80 | 1.99 | 33.49 | 0.9118 | 33.42 | 33.96 | 2.046 | 504 |
| Sponza: FSR, Ultra Performance 3.0x | 104 | 11.19 | 1.43 | 31.42 | 0.8535 | 33.00 | 33.90 | 1.459 | 701 |
| Sponza: FSR + RCAS, Ultra Performance 3.0x | 104 | 11.44 | 1.62 | 31.31 | 0.8518 | 32.74 | 33.90 | 1.653 | 617 |
| Sponza 4K: FSR, Performance 2.0x | 104 | 38.27 | 5.77 | 36.10 | 0.9491 | 34.16 | 34.16 | 5.958 | 173 |
| Sponza 4K: FSR + RCAS, Performance 2.0x | 104 | 39.11 | 6.57 | 35.80 | 0.9464 | 33.88 | 34.16 | 6.768 | 152 |

## FSR: ablacije (Sponza, Quality 1.5x, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 12.01 | 2.17 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.265 | 460 |
| bez dilatacije vektora | 104 | 11.94 | 2.08 | 35.29 | 0.9452 | 33.20 | 34.00 | 2.167 | 481 |
| bez depth clipa (samo clamp boje) | 104 | 12.00 | 2.19 | 35.35 | 0.9458 | 34.38 | 34.00 | 2.264 | 457 |
| bez lockova | 104 | 12.03 | 2.18 | 35.54 | 0.9455 | 34.11 | 34.00 | 2.266 | 460 |
| lock siri clamp 1x | 104 | 12.01 | 2.18 | 35.50 | 0.9457 | 34.13 | 34.00 | 2.267 | 459 |
| lock siri clamp 2x | 104 | 12.28 | 2.18 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.268 | 459 |
| lock siri clamp 4x | 104 | 12.06 | 2.18 | 35.47 | 0.9460 | 34.16 | 34.00 | 2.269 | 459 |
| lock siri clamp 8x | 104 | 11.97 | 2.17 | 35.47 | 0.9460 | 34.16 | 34.00 | 2.261 | 460 |
| lock prag kontrasta 0.2 | 104 | 11.99 | 2.18 | 35.41 | 0.9456 | 34.17 | 34.00 | 2.264 | 460 |
| lock prag kontrasta 0.35 | 104 | 11.98 | 2.18 | 35.45 | 0.9458 | 34.16 | 34.00 | 2.265 | 459 |
| lock prag kontrasta 0.5 | 104 | 12.03 | 2.18 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.262 | 459 |
| lock prag kontrasta 0.7 | 104 | 11.99 | 2.19 | 35.51 | 0.9460 | 34.13 | 34.00 | 2.265 | 457 |
| lock traje 2 okvira | 104 | 11.99 | 2.18 | 35.50 | 0.9460 | 34.14 | 34.00 | 2.256 | 460 |
| lock traje 4 okvira | 104 | 12.03 | 2.18 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.263 | 460 |
| lock traje 8 okvira | 104 | 11.98 | 2.18 | 35.46 | 0.9459 | 34.15 | 34.00 | 2.267 | 460 |
| Lanczos u gibanju 0.75 | 104 | 12.04 | 2.18 | 35.22 | 0.9408 | 34.40 | 34.00 | 2.262 | 460 |
| Lanczos u gibanju 1.0 | 104 | 12.04 | 2.19 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.262 | 456 |
| Lanczos u gibanju 1.25 | 104 | 11.99 | 2.19 | 35.47 | 0.9468 | 33.96 | 34.00 | 2.268 | 456 |
| Lanczos u gibanju 1.5 | 104 | 12.02 | 2.18 | 35.39 | 0.9465 | 33.89 | 34.00 | 2.261 | 459 |
| Lanczos na miru 1.0 | 104 | 12.02 | 2.18 | 35.47 | 0.9458 | 34.16 | 34.00 | 2.262 | 460 |
| Lanczos na miru 1.5 | 104 | 12.00 | 2.18 | 35.47 | 0.9459 | 34.15 | 34.00 | 2.263 | 459 |
| Lanczos na miru 2.0 | 104 | 12.02 | 2.19 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.265 | 457 |
| Lanczos na miru 2.5 | 104 | 12.03 | 2.19 | 35.47 | 0.9460 | 34.15 | 34.00 | 2.264 | 457 |
| tolerancija dubine 0.005 | 104 | 12.01 | 2.18 | 35.47 | 0.9459 | 34.14 | 34.00 | 2.264 | 460 |
| tolerancija dubine 0.02 | 104 | 12.07 | 2.18 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.268 | 460 |
| tolerancija dubine 0.1 | 104 | 11.99 | 2.18 | 35.48 | 0.9460 | 34.18 | 34.00 | 2.264 | 459 |
| reactive maska 0 | 104 | 11.98 | 2.19 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.270 | 457 |
| reactive maska 0.3 | 104 | 12.00 | 2.18 | 35.44 | 0.9459 | 34.11 | 34.00 | 2.264 | 459 |
| reactive maska 1.0 | 104 | 12.05 | 2.18 | 35.24 | 0.9450 | 33.86 | 34.00 | 2.268 | 459 |

## FSR: ablacije s mirnom kamerom (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 11.51 | 2.17 | 40.06 | 0.9828 | 51.72 | 84.55 | 2.177 | 461 |
| bez dilatacije vektora | 104 | 11.51 | 2.07 | 37.56 | 0.9783 | 37.69 | 84.55 | 2.080 | 483 |
| bez depth clipa (samo clamp boje) | 104 | 11.58 | 2.16 | 40.09 | 0.9829 | 53.07 | 84.55 | 2.181 | 463 |
| bez lockova | 104 | 12.11 | 2.16 | 39.55 | 0.9763 | 51.29 | 84.55 | 2.175 | 463 |
| lock siri clamp 1x | 104 | 11.58 | 2.16 | 39.86 | 0.9801 | 51.59 | 84.55 | 2.182 | 463 |
| lock siri clamp 2x | 104 | 11.59 | 2.17 | 40.06 | 0.9828 | 51.72 | 84.55 | 2.183 | 461 |
| lock siri clamp 4x | 104 | 11.55 | 2.17 | 40.09 | 0.9834 | 51.77 | 84.55 | 2.181 | 461 |
| lock siri clamp 8x | 104 | 11.61 | 2.18 | 40.10 | 0.9835 | 51.80 | 84.55 | 2.180 | 459 |
| lock prag kontrasta 0.2 | 104 | 11.59 | 2.16 | 40.22 | 0.9843 | 52.23 | 84.55 | 2.181 | 462 |
| lock prag kontrasta 0.35 | 104 | 11.63 | 2.16 | 40.14 | 0.9837 | 51.95 | 84.55 | 2.180 | 463 |
| lock prag kontrasta 0.5 | 104 | 11.62 | 2.17 | 40.06 | 0.9828 | 51.72 | 84.55 | 2.180 | 461 |
| lock prag kontrasta 0.7 | 104 | 11.55 | 2.17 | 39.88 | 0.9806 | 51.47 | 84.55 | 2.179 | 461 |
| Lanczos na miru 1.0 | 104 | 11.60 | 2.17 | 38.63 | 0.9720 | 51.98 | 84.55 | 2.178 | 461 |
| Lanczos na miru 1.5 | 104 | 11.56 | 2.16 | 39.53 | 0.9794 | 51.53 | 84.55 | 2.179 | 463 |
| Lanczos na miru 2.0 | 104 | 11.54 | 2.18 | 40.06 | 0.9828 | 51.72 | 84.55 | 2.186 | 458 |
| Lanczos na miru 2.5 | 104 | 11.58 | 2.16 | 38.90 | 0.9805 | 49.00 | 84.55 | 2.181 | 463 |
| M4 TAAU, za usporedbu | 104 | 10.98 | 1.60 | 39.36 | 0.9723 | 57.31 | 84.55 | 1.617 | 624 |

## FSR: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU (M4), mirna kamera | 104 | 11.41 | 1.64 | 39.36 | 0.9723 | 57.31 | 84.55 | 1.644 | 609 |
| FSR (M5), mirna kamera | 104 | 12.05 | 2.18 | 40.06 | 0.9828 | 51.72 | 84.55 | 2.198 | 458 |
| FSR + RCAS, mirna kamera | 104 | 12.23 | 2.40 | 39.73 | 0.9810 | 50.62 | 84.55 | 2.396 | 418 |
| TAAU (M4), 120 fps | 104 | 11.81 | 1.65 | 37.50 | 0.9643 | 41.23 | 37.61 | 1.698 | 605 |
| FSR (M5), 120 fps | 104 | 12.89 | 2.22 | 37.53 | 0.9636 | 38.10 | 37.61 | 2.266 | 450 |
| FSR + RCAS, 120 fps | 104 | 12.65 | 2.45 | 37.08 | 0.9598 | 37.56 | 37.61 | 2.466 | 409 |
| TAAU (M4), 60 fps | 104 | 11.38 | 1.60 | 35.13 | 0.9449 | 36.94 | 34.00 | 1.692 | 623 |
| FSR (M5), 60 fps | 104 | 12.01 | 2.17 | 35.48 | 0.9460 | 34.15 | 34.00 | 2.264 | 460 |
| FSR + RCAS, 60 fps | 104 | 12.25 | 2.37 | 35.04 | 0.9412 | 33.71 | 34.00 | 2.463 | 422 |
| TAAU (M4), 30 fps | 104 | 10.84 | 1.54 | 34.36 | 0.9372 | 32.88 | 30.79 | 1.690 | 651 |
| FSR (M5), 30 fps | 104 | 11.51 | 2.10 | 34.95 | 0.9388 | 30.88 | 30.79 | 2.264 | 475 |
| FSR + RCAS, 30 fps | 104 | 11.70 | 2.30 | 34.54 | 0.9336 | 30.61 | 30.79 | 2.465 | 434 |

## Optical flow (M6): tocnost po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 3.48 | 2.71 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.801 | 369 |
| 1080p native (bez upscalinga) | 104 | 4.26 | 3.47 | 7.37 | 1.37 | 33.35 | 61.6 | 84.9 | 3.659 | 288 |
| 1080p, render 960x540, FSR | 104 | 3.03 | 2.32 | 6.94 | 1.40 | 33.36 | 61.2 | 87.2 | 2.385 | 431 |
| 1280x720, render 854x480, FSR | 104 | 2.16 | 1.43 | 4.44 | 1.10 | 19.93 | 65.3 | 88.7 | 1.484 | 699 |
| 4K, render 1920x1080, FSR | 104 | 8.55 | 7.59 | 13.75 | 1.64 | 71.13 | 59.8 | 85.6 | 7.823 | 132 |

## Optical flow: ablacije i pretrage parametara (Sponza, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 3.42 | 2.71 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.809 | 369 |
| bez kandidata iz proslog okvira | 104 | 3.55 | 2.71 | 11.04 | 1.84 | 49.96 | 60.1 | 83.2 | 2.795 | 369 |
| bez medijan filtra | 104 | 3.47 | 2.67 | 7.95 | 2.39 | 34.64 | 56.5 | 79.9 | 2.764 | 374 |
| bez izbora kandidata pri prosirenju | 104 | 3.40 | 2.62 | 12.68 | 1.18 | 56.98 | 61.5 | 84.6 | 2.718 | 381 |
| bez detekcije reza | 104 | 3.65 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.790 | 371 |
| 3 razina piramide | 104 | 3.37 | 2.61 | 18.95 | 1.40 | 141.17 | 61.3 | 83.4 | 2.707 | 383 |
| 4 razina piramide | 104 | 3.38 | 2.64 | 18.20 | 1.35 | 137.16 | 61.5 | 83.7 | 2.724 | 379 |
| 5 razina piramide | 104 | 3.41 | 2.66 | 11.18 | 1.36 | 76.07 | 62.3 | 85.4 | 2.750 | 377 |
| 6 razina piramide | 104 | 3.44 | 2.68 | 6.90 | 1.36 | 33.21 | 62.9 | 86.7 | 2.764 | 373 |
| 7 razina piramide | 104 | 3.46 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.788 | 371 |
| radijus pretrage 2 texela | 104 | 3.37 | 2.61 | 8.48 | 1.47 | 37.43 | 60.7 | 83.7 | 2.695 | 384 |
| radijus pretrage 4 texela | 104 | 3.49 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.796 | 371 |
| radijus pretrage 6 texela | 104 | 3.61 | 2.83 | 6.67 | 1.33 | 30.60 | 63.1 | 87.0 | 2.924 | 353 |
| radijus pretrage 8 texela | 104 | 3.79 | 3.02 | 6.54 | 1.32 | 30.20 | 63.5 | 87.5 | 3.108 | 331 |
| glatkoca 0/texel | 104 | 3.47 | 2.70 | 7.08 | 1.41 | 30.90 | 63.7 | 86.9 | 2.788 | 371 |
| glatkoca 0.0002/texel | 104 | 3.45 | 2.70 | 6.68 | 1.36 | 31.09 | 63.6 | 87.1 | 2.789 | 371 |
| glatkoca 0.0005/texel | 104 | 3.47 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.786 | 371 |
| glatkoca 0.002/texel | 104 | 3.52 | 2.71 | 8.30 | 1.39 | 42.37 | 57.5 | 81.5 | 2.789 | 368 |
| glatkoca 0.01/texel | 104 | 3.47 | 2.70 | 17.14 | 2.71 | 107.05 | 39.9 | 67.8 | 2.795 | 370 |
| novelty 0 | 104 | 3.47 | 2.70 | 7.40 | 1.41 | 36.88 | 63.1 | 86.6 | 2.786 | 371 |
| novelty 0.0005 | 104 | 3.46 | 2.70 | 7.32 | 1.40 | 34.96 | 62.5 | 86.1 | 2.789 | 371 |
| novelty 0.001 | 104 | 3.46 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.788 | 371 |
| novelty 0.004 | 104 | 3.47 | 2.70 | 8.22 | 1.23 | 35.11 | 62.2 | 85.8 | 2.792 | 371 |
| novelty 0.01 | 104 | 3.47 | 2.70 | 10.95 | 1.20 | 45.09 | 62.0 | 85.5 | 2.790 | 371 |
| novelty 1 | 104 | 3.49 | 2.70 | 11.93 | 1.22 | 49.45 | 61.7 | 85.1 | 2.792 | 371 |
| rez: statistika maksimum | 104 | 3.44 | 2.67 | 17.27 | 1.36 | 129.04 | 58.6 | 80.5 | 2.796 | 375 |
| rez: statistika srednja | 104 | 3.51 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.791 | 370 |
| rez: statistika medijan | 104 | 3.46 | 2.70 | 7.17 | 1.36 | 33.50 | 62.5 | 86.1 | 2.788 | 371 |

## Optical flow: tocnost u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 3.54 | 2.71 | 0.00 | 0.00 | 0.00 | 100.0 | 100.0 | 2.729 | 369 |
| 120 fps | 120 | 3.29 | 2.69 | 2.82 | 1.49 | 7.62 | 65.7 | 89.0 | 2.788 | 372 |
| 60 fps | 60 | 3.48 | 2.71 | 11.63 | 3.66 | 36.81 | 56.9 | 80.2 | 2.794 | 370 |
| 30 fps | 30 | 3.76 | 2.70 | 35.79 | 8.83 | 106.72 | 47.5 | 70.4 | 2.807 | 371 |

## Generiranje okvira (M7): kvaliteta po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 16.57 | 4.64 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.764 | 215 |
| 1080p native (FSR 1.0x) | 104 | 20.78 | 6.68 | 36.50 | 27.88 | 0.9628 | 26.14 | 0.6814 | 6.857 | 150 |
| 1080p, render 960x540, FSR | 104 | 16.27 | 3.83 | 33.55 | 27.40 | 0.9166 | 26.16 | 0.6817 | 3.880 | 261 |
| 1280x720, render 854x480, FSR | 104 | 9.57 | 2.33 | 33.95 | 27.43 | 0.9293 | 26.67 | 0.6982 | 2.349 | 428 |

## Generiranje okvira: ablacije (Sponza, 60 fps orbita, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 16.50 | 4.64 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.760 | 215 |
| samo game vektori | 104 | 15.48 | 3.73 | 34.84 | 26.71 | 0.9447 | 26.11 | 0.6775 | 3.833 | 268 |
| samo optical flow | 104 | 15.97 | 4.07 | 33.76 | 25.87 | 0.9263 | 26.11 | 0.6775 | 4.174 | 246 |
| bez vektora (= blend) | 104 | 14.88 | 3.30 | 26.11 | 21.39 | 0.6775 | 26.11 | 0.6775 | 3.398 | 303 |
| bez maski disokluzije | 104 | 16.27 | 4.45 | 34.93 | 27.38 | 0.9445 | 26.11 | 0.6775 | 4.553 | 225 |
| bez dilatiranih vektora | 104 | 16.47 | 4.67 | 34.98 | 27.65 | 0.9444 | 26.11 | 0.6775 | 4.788 | 214 |
| piramida: najblizi umjesto pozadine | 104 | 17.22 | 4.66 | 34.99 | 27.57 | 0.9443 | 26.11 | 0.6775 | 4.773 | 214 |
| s bojom u prioritetu scattera | 104 | 16.75 | 5.02 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 5.134 | 199 |
| tezina toka uz game vektor 1 | 104 | 16.38 | 4.64 | 34.85 | 27.60 | 0.9422 | 26.11 | 0.6775 | 4.763 | 216 |
| tezina toka uz game vektor 0.5 | 104 | 16.45 | 4.64 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.755 | 215 |
| tezina toka uz game vektor 0.25 | 104 | 17.10 | 4.64 | 35.05 | 27.43 | 0.9451 | 26.11 | 0.6775 | 4.759 | 215 |
| tezina toka uz game vektor 0.1 | 104 | 16.36 | 4.65 | 35.02 | 27.22 | 0.9452 | 26.11 | 0.6775 | 4.763 | 215 |
| 1 razina piramide polja | 104 | 16.26 | 4.49 | 34.71 | 27.99 | 0.9419 | 26.11 | 0.6775 | 4.596 | 223 |
| 3 razina piramide polja | 104 | 16.45 | 4.62 | 34.84 | 27.67 | 0.9432 | 26.11 | 0.6775 | 4.756 | 216 |
| 5 razina piramide polja | 104 | 16.84 | 4.63 | 34.94 | 27.52 | 0.9439 | 26.11 | 0.6775 | 4.755 | 216 |
| 7 razina piramide polja | 104 | 16.39 | 4.64 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.770 | 216 |
| ostrina slaganja boja 0 | 104 | 16.32 | 4.64 | 34.89 | 27.76 | 0.9433 | 26.11 | 0.6775 | 4.760 | 216 |
| ostrina slaganja boja 6 | 104 | 16.43 | 4.64 | 34.98 | 27.71 | 0.9439 | 26.11 | 0.6775 | 4.767 | 215 |
| ostrina slaganja boja 24 | 104 | 16.43 | 4.65 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.762 | 215 |
| ostrina slaganja boja 96 | 104 | 16.53 | 4.64 | 35.02 | 27.62 | 0.9446 | 26.11 | 0.6775 | 4.766 | 215 |
| tolerancija dubine 0.005 | 104 | 16.43 | 4.64 | 34.99 | 27.56 | 0.9442 | 26.11 | 0.6775 | 4.752 | 215 |
| tolerancija dubine 0.02 | 104 | 16.44 | 4.65 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.766 | 215 |
| tolerancija dubine 0.08 | 104 | 16.40 | 4.64 | 35.01 | 27.55 | 0.9443 | 26.11 | 0.6775 | 4.764 | 216 |
| bez inpaintinga slike (prolazi 8-9) | 104 | 16.50 | 4.41 | 34.98 | 27.57 | 0.9442 | 26.11 | 0.6775 | 4.529 | 227 |
| bez odbacivanja uzoraka izvan ekrana | 104 | 16.41 | 4.65 | 34.97 | 27.66 | 0.9439 | 26.11 | 0.6775 | 4.774 | 215 |
| kao M7: bez inpaintinga i provjere granica | 104 | 16.26 | 4.41 | 34.94 | 27.56 | 0.9438 | 26.11 | 0.6775 | 4.528 | 227 |
| prag pokrivenosti inpaintinga 0.1 | 104 | 16.31 | 4.65 | 35.00 | 27.55 | 0.9443 | 26.11 | 0.6775 | 4.775 | 215 |
| prag pokrivenosti inpaintinga 0.3 | 104 | 17.15 | 4.65 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 4.768 | 215 |
| prag pokrivenosti inpaintinga 0.6 | 104 | 16.45 | 4.65 | 34.99 | 27.61 | 0.9442 | 26.11 | 0.6775 | 4.773 | 215 |
| proceduralna scena: Sve zadano | 104 | 11.65 | 3.77 | 34.06 | 33.74 | 0.9708 | 31.61 | 0.9475 | 3.791 | 265 |
| proceduralna scena: samo game vektori | 104 | 10.53 | 2.86 | 33.68 | 33.34 | 0.9683 | 31.61 | 0.9475 | 2.880 | 349 |
| proceduralna scena: samo optical flow | 104 | 11.10 | 3.26 | 33.53 | 32.79 | 0.9669 | 31.61 | 0.9475 | 3.251 | 307 |
| proceduralna scena: bez vektora (= blend) | 104 | 10.04 | 2.48 | 31.61 | 31.13 | 0.9475 | 31.61 | 0.9475 | 2.497 | 403 |
| proceduralna scena: bez maski disokluzije | 104 | 11.96 | 3.59 | 34.10 | 33.75 | 0.9710 | 31.61 | 0.9475 | 3.605 | 279 |
| proceduralna scena: piramida: najblizi umjesto pozadine | 104 | 11.73 | 3.77 | 34.06 | 33.74 | 0.9708 | 31.61 | 0.9475 | 3.789 | 265 |
| proceduralna scena: s bojom u prioritetu scattera | 104 | 12.08 | 4.14 | 34.06 | 33.74 | 0.9708 | 31.61 | 0.9475 | 4.152 | 241 |
| proceduralna scena: bez inpaintinga slike (prolazi 8-9) | 104 | 11.49 | 3.54 | 34.07 | 33.73 | 0.9708 | 31.61 | 0.9475 | 3.558 | 282 |
| proceduralna scena: bez odbacivanja uzoraka izvan ekrana | 104 | 11.71 | 3.77 | 34.06 | 33.73 | 0.9708 | 31.61 | 0.9475 | 3.790 | 265 |
| proceduralna scena: kao M7: bez inpaintinga i provjere granica | 104 | 11.48 | 3.54 | 34.07 | 33.73 | 0.9708 | 31.61 | 0.9475 | 3.556 | 283 |
| proceduralna scena: tezina toka uz game vektor 1 | 104 | 11.71 | 3.77 | 34.08 | 33.73 | 0.9710 | 31.61 | 0.9475 | 3.791 | 265 |
| proceduralna scena: tezina toka uz game vektor 0.5 | 104 | 12.48 | 3.78 | 34.06 | 33.74 | 0.9708 | 31.61 | 0.9475 | 3.788 | 265 |
| proceduralna scena: tezina toka uz game vektor 0.25 | 104 | 11.98 | 3.77 | 34.02 | 33.71 | 0.9704 | 31.61 | 0.9475 | 3.787 | 265 |
| proceduralna scena: tezina toka uz game vektor 0.1 | 104 | 11.71 | 3.77 | 33.96 | 33.63 | 0.9699 | 31.61 | 0.9475 | 3.790 | 265 |

## Generiranje okvira: kvaliteta u ovisnosti o brzini kamere (Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 16.45 | 4.60 | 40.14 | 40.02 | 0.9832 | 40.14 | 0.9832 | 4.625 | 218 |
| 120 fps | 120 | 16.29 | 4.64 | 35.94 | 31.23 | 0.9553 | 27.17 | 0.7489 | 4.747 | 215 |
| 60 fps | 60 | 18.15 | 4.65 | 34.65 | 27.60 | 0.9473 | 25.56 | 0.6927 | 4.779 | 215 |
| 30 fps | 30 | 17.07 | 4.66 | 32.85 | 24.11 | 0.9309 | 24.06 | 0.6461 | 4.768 | 215 |
| 30 fps, kao M7 | 30 | 16.81 | 4.47 | 32.80 | 24.01 | 0.9299 | 24.06 | 0.6461 | 4.594 | 224 |
| 20 fps | 20 | 16.95 | 4.61 | 30.95 | 20.23 | 0.9122 | 23.08 | 0.6186 | 4.852 | 217 |
| 20 fps, kao M7 | 20 | 16.52 | 4.37 | 31.09 | 20.33 | 0.9112 | 23.08 | 0.6186 | 4.609 | 229 |

## UI kompozicija (M8): HUD nakon generiranja okvira ili upečen prije njega

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | PSNR HUD (dB) | SSIM HUD | PSNR HUD blend (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| HUD nakon generiranja (kompozicija) | 104 | 19.51 | 4.74 | 35.04 | 27.57 | 0.9450 | 26.21 | 0.6869 | 52.01 | 0.9979 | 39.63 | 4.854 | 211 |
| HUD upečen prije generiranja | 104 | 20.74 | 4.88 | 30.65 | 25.34 | 0.9390 | 26.21 | 0.6869 | 20.02 | 0.8811 | 39.64 | 4.975 | 205 |
| HUD upečen, 20 fps | 20 | 19.62 | 4.82 | 26.77 | 18.42 | 0.8982 | 23.18 | 0.6275 | 16.43 | 0.7078 | 36.61 | 5.031 | 208 |
| HUD nakon generiranja, 20 fps | 20 | 19.71 | 4.69 | 31.01 | 20.24 | 0.9143 | 23.18 | 0.6275 | 48.26 | 0.9900 | 36.61 | 4.909 | 213 |

## Naucena mjesavina (M9) naspram heuristike, putanje izvan skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p Quality: heuristika | 104 | 18.72 | 4.71 | 35.00 | 27.56 | 0.9443 | 26.11 | 0.6775 | 5.054 | 212 |
| 1080p Quality: naucena mjesavina | 104 | 24.32 | 8.91 | 35.08 | 28.57 | 0.9457 | 26.11 | 0.6775 | 9.164 | 112 |
| 1080p native: heuristika | 104 | 21.60 | 6.69 | 36.50 | 27.88 | 0.9628 | 26.14 | 0.6814 | 6.893 | 150 |
| 1080p native: naucena mjesavina | 104 | 28.83 | 11.11 | 36.65 | 28.86 | 0.9650 | 26.14 | 0.6814 | 12.016 | 90 |
| 1080p Performance: heuristika | 104 | 20.03 | 3.86 | 33.55 | 27.40 | 0.9166 | 26.16 | 0.6817 | 3.880 | 259 |
| 1080p Performance: naucena mjesavina | 104 | 23.22 | 7.96 | 33.62 | 28.22 | 0.9176 | 26.16 | 0.6817 | 8.199 | 126 |
| 720p Quality: heuristika | 104 | 12.30 | 2.49 | 33.95 | 27.43 | 0.9293 | 26.67 | 0.6982 | 2.575 | 402 |
| 720p Quality: naucena mjesavina | 104 | 14.59 | 4.40 | 33.97 | 28.12 | 0.9307 | 26.67 | 0.6982 | 6.038 | 227 |
| proceduralna scena: heuristika | 104 | 14.05 | 3.77 | 34.06 | 33.74 | 0.9708 | 31.61 | 0.9475 | 3.795 | 265 |
| proceduralna scena: naucena mjesavina | 104 | 18.55 | 7.96 | 34.22 | 33.83 | 0.9721 | 31.61 | 0.9475 | 7.979 | 126 |
| Quality, 120 fps: heuristika | 120 | 19.57 | 4.67 | 35.94 | 31.23 | 0.9553 | 27.17 | 0.7489 | 4.881 | 214 |
| Quality, 120 fps: naucena mjesavina | 120 | 24.62 | 8.91 | 36.03 | 31.08 | 0.9568 | 27.17 | 0.7489 | 8.957 | 112 |
| Quality, 30 fps: heuristika | 30 | 18.89 | 4.67 | 32.85 | 24.11 | 0.9309 | 24.06 | 0.6461 | 4.813 | 214 |
| Quality, 30 fps: naucena mjesavina | 30 | 23.80 | 8.96 | 32.69 | 23.97 | 0.9333 | 24.06 | 0.6461 | 9.158 | 112 |
| Quality, 20 fps: heuristika | 20 | 18.91 | 4.73 | 30.95 | 20.23 | 0.9122 | 23.08 | 0.6186 | 6.213 | 211 |
| Quality, 20 fps: naucena mjesavina | 20 | 23.44 | 8.88 | 30.57 | 18.85 | 0.9112 | 23.08 | 0.6186 | 9.196 | 113 |

## Naucena mjesavina: velicina mreze i sastav skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| blend-c12-c24, Sponza Quality | 104 | 20.72 | 8.96 | 34.89 | 25.49 | 0.9449 | 26.11 | 0.6775 | 9.112 | 112 |
| blend-c12-c24, proceduralna scena | 104 | 16.67 | 8.07 | 34.28 | 33.83 | 0.9724 | 31.61 | 0.9475 | 8.078 | 124 |
| blend-c4-c8, Sponza Quality | 104 | 17.99 | 6.28 | 34.82 | 25.38 | 0.9444 | 26.11 | 0.6775 | 6.434 | 159 |
| blend-c4-c8, proceduralna scena | 104 | 13.41 | 5.38 | 34.22 | 33.85 | 0.9720 | 31.61 | 0.9475 | 5.406 | 186 |
| blend-c8-c16-sponza, Sponza Quality | 104 | 19.22 | 7.41 | 34.92 | 25.25 | 0.9452 | 26.11 | 0.6775 | 7.583 | 135 |
| blend-c8-c16-sponza, proceduralna scena | 104 | 14.46 | 6.52 | 33.89 | 33.33 | 0.9698 | 31.61 | 0.9475 | 6.535 | 153 |
| blend-c8-c16, Sponza Quality | 104 | 19.16 | 7.42 | 35.08 | 28.57 | 0.9457 | 26.11 | 0.6775 | 7.593 | 135 |
| blend-c8-c16, proceduralna scena | 104 | 14.42 | 6.52 | 34.22 | 33.83 | 0.9721 | 31.61 | 0.9475 | 6.538 | 153 |
