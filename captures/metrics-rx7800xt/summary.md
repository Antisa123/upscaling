# Metrike

## Motion vectori: reprojekcija prethodnog okvira

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR reproj. (dB) | SSIM reproj. | PSNR bez reproj. (dB) | SSIM bez reproj. | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Native 1920x1080 | 104 | 2.55 | 0.10 | 30.93 | 0.9736 | 25.53 | 0.8882 | 0.124 | 9950 |
| Native 960x540 | 104 | 1.29 | 0.03 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.036 | 33325 |
| 1080p, render 1.5x manji | 104 | 1.48 | 0.06 | 30.62 | 0.9674 | 25.87 | 0.8917 | 0.059 | 17288 |
| 1080p, render 2.0x manji | 104 | 1.31 | 0.05 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.065 | 21359 |
| 960x540, sporo gibanje (dt/4) | 104 | 1.25 | 0.03 | 35.04 | 0.9859 | 32.93 | 0.9782 | 0.031 | 34329 |
| 960x540, brzo gibanje (dt*2) | 104 | 1.23 | 0.03 | 27.67 | 0.9507 | 22.74 | 0.8046 | 0.031 | 34175 |
| 960x540, bez filtriranja uzorka | 104 | 1.37 | 0.03 | 27.90 | 0.9259 | 24.59 | 0.8563 | 0.030 | 34860 |

## Cijena renderiranja bez upscalera (referentne brojke za ubrzanje)

| Konfiguracija | Frameovi | CPU ms | GPU ms | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|
| Sponza: native 1920x1080 | 104 | 0.15 | 0.36 | 0.405 | 2744 |
| Sponza: samo render 1280x720 | 104 | 0.15 | 0.18 | 0.219 | 5483 |
| Sponza: samo render 960x540 | 104 | 0.15 | 0.12 | 0.144 | 8468 |
| Sponza: native 3840x2160 | 104 | 0.15 | 1.82 | 2.174 | 550 |
| Sponza: samo render 1920x1080 | 104 | 0.14 | 0.56 | 0.676 | 1790 |

## Prostorni upscaleri (M3)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Nearest, Quality 1.5x | 104 | 2.47 | 0.08 | 30.08 | 0.9438 | 27.47 | 30.49 | 0.120 | 12316 |
| Sponza: Nearest, Quality 1.5x | 104 | 3.98 | 0.21 | 32.29 | 0.8997 | 29.76 | 34.06 | 0.235 | 4797 |
| Bilinear, Quality 1.5x | 104 | 2.49 | 0.08 | 33.28 | 0.9677 | 30.02 | 30.49 | 0.134 | 11920 |
| Sponza: Bilinear, Quality 1.5x | 104 | 4.03 | 0.22 | 35.19 | 0.9320 | 34.06 | 34.06 | 0.270 | 4629 |
| Bicubic, Quality 1.5x | 104 | 2.48 | 0.09 | 32.84 | 0.9662 | 29.05 | 30.49 | 0.127 | 10856 |
| Sponza: Bicubic, Quality 1.5x | 104 | 3.98 | 0.23 | 35.41 | 0.9424 | 32.81 | 34.06 | 0.266 | 4375 |
| FSR1 EASU+RCAS, Quality 1.5x | 104 | 2.51 | 0.15 | 31.13 | 0.9548 | 27.56 | 30.49 | 0.184 | 6613 |
| Sponza: FSR1 EASU+RCAS, Quality 1.5x | 104 | 4.08 | 0.29 | 33.52 | 0.9211 | 30.49 | 34.06 | 0.366 | 3417 |
| Nearest, Performance 2.0x | 104 | 2.42 | 0.07 | 29.30 | 0.9323 | 26.94 | 30.44 | 0.097 | 15313 |
| Sponza: Nearest, Performance 2.0x | 104 | 3.91 | 0.17 | 31.84 | 0.8848 | 29.59 | 34.00 | 0.199 | 5825 |
| Bilinear, Performance 2.0x | 104 | 2.47 | 0.06 | 31.26 | 0.9479 | 29.83 | 30.44 | 0.064 | 15929 |
| Sponza: Bilinear, Performance 2.0x | 104 | 3.96 | 0.18 | 33.12 | 0.8886 | 33.68 | 34.00 | 0.205 | 5711 |
| Bicubic, Performance 2.0x | 104 | 2.43 | 0.07 | 30.83 | 0.9467 | 28.54 | 30.44 | 0.096 | 13474 |
| Sponza: Bicubic, Performance 2.0x | 104 | 3.90 | 0.19 | 33.30 | 0.9024 | 32.05 | 34.00 | 0.213 | 5187 |
| FSR1 EASU+RCAS, Performance 2.0x | 104 | 2.50 | 0.13 | 30.05 | 0.9441 | 27.33 | 30.44 | 0.175 | 7416 |
| Sponza: FSR1 EASU+RCAS, Performance 2.0x | 104 | 4.09 | 0.26 | 32.45 | 0.8920 | 30.17 | 34.00 | 0.332 | 3779 |
| Sponza 4K: Nearest, Performance 2.0x | 104 | 12.09 | 0.65 | 34.05 | 0.9231 | 30.78 | 34.20 | 0.695 | 1528 |
| Sponza 4K: Bilinear, Performance 2.0x | 104 | 12.14 | 0.68 | 35.58 | 0.9330 | 34.34 | 34.20 | 0.724 | 1467 |
| Sponza 4K: Bicubic, Performance 2.0x | 104 | 12.10 | 0.70 | 35.89 | 0.9435 | 33.03 | 34.20 | 0.745 | 1424 |
| Sponza 4K: FSR1 EASU+RCAS, Performance 2.0x | 104 | 12.47 | 1.10 | 34.84 | 0.9318 | 31.35 | 34.20 | 1.310 | 911 |

## RCAS: ablacija ostrine (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR1, sharpness 0 | 104 | 4.04 | 0.30 | 31.46 | 0.8830 | 29.13 | 34.06 | 0.373 | 3380 |
| FSR1, sharpness 0.25 | 104 | 4.05 | 0.30 | 33.52 | 0.9211 | 30.49 | 34.06 | 0.390 | 3387 |
| FSR1, sharpness 0.5 | 104 | 4.01 | 0.30 | 34.40 | 0.9336 | 31.19 | 34.06 | 0.351 | 3380 |
| FSR1, sharpness 1 | 104 | 4.04 | 0.29 | 35.06 | 0.9406 | 31.92 | 34.06 | 0.382 | 3401 |
| FSR1, sharpness 2 | 104 | 4.05 | 0.29 | 35.36 | 0.9419 | 32.49 | 34.06 | 0.353 | 3405 |
| FSR1, sharpness 4 | 104 | 4.01 | 0.29 | 35.44 | 0.9412 | 32.80 | 34.06 | 0.346 | 3448 |
| FSR1, EASU bez RCAS-a | 104 | 4.04 | 0.29 | 35.45 | 0.9409 | 32.88 | 34.06 | 0.368 | 3390 |

## Temporalni upscaler (M4): TAAU

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, Quality 1.5x | 104 | 2.54 | 0.14 | 32.66 | 0.9657 | 32.11 | 30.48 | 0.178 | 6959 |
| Sponza: TAAU, Quality 1.5x | 104 | 4.11 | 0.36 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.394 | 2758 |
| TAAU + RCAS, Quality 1.5x | 104 | 2.62 | 0.17 | 32.55 | 0.9662 | 31.67 | 30.48 | 0.214 | 5879 |
| Sponza: TAAU + RCAS, Quality 1.5x | 104 | 4.13 | 0.39 | 35.26 | 0.9492 | 36.36 | 34.01 | 0.457 | 2544 |
| TAAU, Performance 2.0x | 104 | 2.50 | 0.12 | 31.56 | 0.9551 | 32.22 | 30.45 | 0.158 | 8070 |
| Sponza: TAAU, Performance 2.0x | 104 | 4.06 | 0.33 | 33.63 | 0.9131 | 37.34 | 33.96 | 0.366 | 2998 |
| TAAU + RCAS, Performance 2.0x | 104 | 2.57 | 0.15 | 31.48 | 0.9558 | 31.75 | 30.45 | 0.185 | 6617 |
| Sponza: TAAU + RCAS, Performance 2.0x | 104 | 4.08 | 0.37 | 33.80 | 0.9209 | 36.53 | 33.96 | 0.435 | 2722 |

## TAAU: ablacije (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, sve zadano | 104 | 4.09 | 0.36 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.392 | 2764 |
| bez jittera (nema sto akumulirati) | 104 | 4.09 | 0.36 | 34.80 | 0.9403 | 37.57 | 34.05 | 0.395 | 2766 |
| akumulacija max 1 okvira | 104 | 4.09 | 0.37 | 35.16 | 0.9431 | 37.04 | 34.01 | 0.410 | 2713 |
| akumulacija max 2 okvira | 104 | 4.08 | 0.37 | 35.14 | 0.9429 | 37.08 | 34.01 | 0.424 | 2694 |
| akumulacija max 4 okvira | 104 | 4.08 | 0.36 | 35.11 | 0.9425 | 37.14 | 34.01 | 0.397 | 2757 |
| akumulacija max 8 okvira | 104 | 4.12 | 0.36 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.394 | 2782 |
| akumulacija max 16 okvira | 104 | 4.09 | 0.36 | 34.94 | 0.9405 | 37.26 | 34.01 | 0.392 | 2781 |
| akumulacija max 32 okvira | 104 | 4.08 | 0.37 | 34.81 | 0.9389 | 37.32 | 34.01 | 0.418 | 2722 |
| clamp 0.5 sigma | 104 | 4.07 | 0.36 | 34.51 | 0.9188 | 36.61 | 34.01 | 0.388 | 2752 |
| clamp 1 sigma | 104 | 4.09 | 0.37 | 35.19 | 0.9381 | 37.01 | 34.01 | 0.397 | 2735 |
| clamp 1.5 sigma | 104 | 4.11 | 0.36 | 35.12 | 0.9417 | 37.15 | 34.01 | 0.396 | 2748 |
| clamp 2 sigma | 104 | 4.08 | 0.37 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.428 | 2700 |
| clamp 4 sigma | 104 | 4.07 | 0.36 | 35.00 | 0.9415 | 37.22 | 34.01 | 0.379 | 2790 |
| rekonstrukcija 1/sigma^2 = 2 | 104 | 4.09 | 0.36 | 34.51 | 0.9214 | 38.21 | 34.01 | 0.392 | 2752 |
| rekonstrukcija 1/sigma^2 = 4 | 104 | 4.08 | 0.37 | 35.16 | 0.9396 | 37.37 | 34.01 | 0.426 | 2725 |
| rekonstrukcija 1/sigma^2 = 8 | 104 | 4.09 | 0.36 | 34.81 | 0.9406 | 37.26 | 34.01 | 0.390 | 2775 |
| rekonstrukcija 1/sigma^2 = 16 | 104 | 4.10 | 0.36 | 33.91 | 0.9307 | 37.62 | 34.01 | 0.413 | 2740 |
| skracivanje povijesti 0/px | 104 | 4.10 | 0.36 | 32.64 | 0.9042 | 38.53 | 34.01 | 0.385 | 2782 |
| skracivanje povijesti 0.4/px | 104 | 4.11 | 0.36 | 34.48 | 0.9349 | 37.67 | 34.01 | 0.400 | 2740 |
| skracivanje povijesti 0.8/px | 104 | 4.11 | 0.36 | 34.84 | 0.9393 | 37.39 | 34.01 | 0.401 | 2766 |
| skracivanje povijesti 1.6/px | 104 | 4.10 | 0.37 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.468 | 2721 |
| skracivanje povijesti 3.2/px | 104 | 4.08 | 0.36 | 35.13 | 0.9427 | 37.10 | 34.01 | 0.400 | 2743 |
| mip bias 0 | 104 | 4.04 | 0.26 | 34.54 | 0.9283 | 37.42 | 34.01 | 0.287 | 3833 |
| mip bias -0.585 | 104 | 4.05 | 0.28 | 34.93 | 0.9384 | 37.32 | 34.01 | 0.323 | 3526 |
| mip bias -1.585 | 104 | 4.09 | 0.37 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.400 | 2721 |
| TAAU + RCAS, ostrina 0.4 | 104 | 4.12 | 0.39 | 34.89 | 0.9482 | 35.11 | 34.01 | 0.451 | 2563 |
| TAAU + RCAS, ostrina 0.8 | 104 | 4.10 | 0.39 | 35.23 | 0.9505 | 35.93 | 34.01 | 0.450 | 2550 |
| TAAU + RCAS, ostrina 1.0 | 104 | 4.11 | 0.39 | 35.26 | 0.9499 | 36.17 | 34.01 | 0.455 | 2550 |
| TAAU + RCAS, ostrina 1.2 | 104 | 4.11 | 0.39 | 35.26 | 0.9492 | 36.36 | 34.01 | 0.446 | 2579 |
| TAAU + RCAS, ostrina 1.5 | 104 | 4.12 | 0.39 | 35.24 | 0.9479 | 36.57 | 34.01 | 0.467 | 2538 |
| TAAU + RCAS, ostrina 2.0 | 104 | 4.11 | 0.39 | 35.19 | 0.9461 | 36.79 | 34.01 | 0.455 | 2559 |

## TAAU: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| bicubic, mirna kamera | 104 | 3.73 | 0.22 | 35.90 | 0.9452 | 84.22 | 82.93 | 0.243 | 4632 |
| TAAU, mirna kamera | 104 | 4.40 | 0.35 | 38.71 | 0.9661 | 58.86 | 83.04 | 0.379 | 2817 |
| TAAU + RCAS, mirna kamera | 104 | 3.88 | 0.39 | 39.72 | 0.9770 | 56.80 | 83.20 | 0.467 | 2577 |
| bicubic, 120 fps | 104 | 4.07 | 0.23 | 37.25 | 0.9604 | 36.09 | 37.71 | 0.251 | 4385 |
| TAAU, 120 fps | 104 | 4.20 | 0.36 | 37.32 | 0.9614 | 41.63 | 37.63 | 0.400 | 2769 |
| TAAU + RCAS, 120 fps | 104 | 4.26 | 0.39 | 37.62 | 0.9673 | 40.55 | 37.63 | 0.462 | 2545 |
| bicubic, 60 fps | 104 | 3.97 | 0.22 | 35.41 | 0.9424 | 32.81 | 34.06 | 0.250 | 4484 |
| TAAU, 60 fps | 104 | 4.12 | 0.36 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.420 | 2753 |
| TAAU + RCAS, 60 fps | 104 | 4.16 | 0.39 | 35.26 | 0.9491 | 36.36 | 34.01 | 0.448 | 2554 |
| bicubic, 30 fps | 104 | 3.88 | 0.22 | 35.32 | 0.9396 | 30.20 | 30.82 | 0.250 | 4532 |
| TAAU, 30 fps | 104 | 3.98 | 0.36 | 34.33 | 0.9342 | 32.93 | 30.79 | 0.439 | 2755 |
| TAAU + RCAS, 30 fps | 104 | 4.04 | 0.39 | 34.47 | 0.9414 | 32.48 | 30.79 | 0.476 | 2561 |

## Puni upscaler (M5): dilatacija, depth clip, lockovi, Lanczos

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Sponza: FSR, NativeAA 1.0x | 104 | 4.37 | 0.66 | 38.17 | 0.9746 | 34.89 | 34.19 | 0.761 | 1504 |
| Sponza: FSR + RCAS, NativeAA 1.0x | 104 | 4.42 | 0.71 | 37.64 | 0.9717 | 34.47 | 34.19 | 0.822 | 1418 |
| Sponza: FSR, Quality 1.5x | 104 | 4.20 | 0.48 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.554 | 2084 |
| Sponza: FSR + RCAS, Quality 1.5x | 104 | 4.24 | 0.51 | 35.51 | 0.9477 | 34.25 | 34.01 | 0.577 | 1980 |
| Sponza: FSR, Balanced 1.7x | 104 | 4.18 | 0.45 | 35.06 | 0.9385 | 34.61 | 33.99 | 0.492 | 2247 |
| Sponza: FSR + RCAS, Balanced 1.7x | 104 | 4.21 | 0.47 | 34.82 | 0.9366 | 34.24 | 33.99 | 0.542 | 2108 |
| Sponza: FSR, Performance 2.0x | 104 | 4.11 | 0.42 | 34.08 | 0.9197 | 34.54 | 33.96 | 0.447 | 2408 |
| Sponza: FSR + RCAS, Performance 2.0x | 104 | 4.15 | 0.45 | 33.92 | 0.9185 | 34.19 | 33.96 | 0.519 | 2220 |
| Sponza: FSR, Ultra Performance 3.0x | 104 | 4.08 | 0.38 | 31.76 | 0.8581 | 34.10 | 33.90 | 0.417 | 2631 |
| Sponza: FSR + RCAS, Ultra Performance 3.0x | 104 | 4.16 | 0.41 | 31.69 | 0.8576 | 33.85 | 33.90 | 0.466 | 2457 |
| Sponza 4K: FSR, Performance 2.0x | 104 | 12.66 | 1.44 | 36.39 | 0.9515 | 34.57 | 34.17 | 1.682 | 694 |
| Sponza 4K: FSR + RCAS, Performance 2.0x | 104 | 12.83 | 1.69 | 36.17 | 0.9498 | 34.32 | 34.17 | 1.931 | 593 |

## FSR: ablacije (Sponza, Quality 1.5x, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 4.22 | 0.47 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.529 | 2118 |
| bez dilatacije vektora | 104 | 4.18 | 0.45 | 35.69 | 0.9499 | 33.69 | 34.01 | 0.489 | 2214 |
| bez depth clipa (samo clamp boje) | 104 | 4.21 | 0.47 | 35.70 | 0.9499 | 34.87 | 34.01 | 0.513 | 2122 |
| bez lockova | 104 | 4.20 | 0.47 | 35.87 | 0.9495 | 34.62 | 34.01 | 0.520 | 2115 |
| lock siri clamp 1x | 104 | 4.27 | 0.47 | 35.84 | 0.9499 | 34.63 | 34.01 | 0.553 | 2111 |
| lock siri clamp 2x | 104 | 4.27 | 0.48 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.587 | 2078 |
| lock siri clamp 4x | 104 | 4.21 | 0.47 | 35.83 | 0.9504 | 34.65 | 34.01 | 0.539 | 2111 |
| lock siri clamp 8x | 104 | 4.14 | 0.48 | 35.83 | 0.9504 | 34.65 | 34.01 | 0.560 | 2105 |
| lock prag kontrasta 0.2 | 104 | 4.23 | 0.48 | 35.78 | 0.9501 | 34.66 | 34.01 | 0.544 | 2097 |
| lock prag kontrasta 0.35 | 104 | 4.17 | 0.47 | 35.81 | 0.9503 | 34.65 | 34.01 | 0.541 | 2118 |
| lock prag kontrasta 0.5 | 104 | 4.16 | 0.47 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.516 | 2129 |
| lock prag kontrasta 0.7 | 104 | 4.17 | 0.47 | 35.86 | 0.9502 | 34.63 | 34.01 | 0.511 | 2106 |
| lock traje 2 okvira | 104 | 4.15 | 0.47 | 35.85 | 0.9503 | 34.64 | 34.01 | 0.530 | 2126 |
| lock traje 4 okvira | 104 | 4.16 | 0.47 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.533 | 2119 |
| lock traje 8 okvira | 104 | 4.15 | 0.47 | 35.82 | 0.9503 | 34.65 | 34.01 | 0.523 | 2115 |
| Lanczos u gibanju 0.75 | 104 | 4.14 | 0.47 | 35.59 | 0.9454 | 34.89 | 34.01 | 0.537 | 2148 |
| Lanczos u gibanju 1.0 | 104 | 4.14 | 0.46 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.524 | 2154 |
| Lanczos u gibanju 1.25 | 104 | 4.14 | 0.47 | 35.79 | 0.9509 | 34.45 | 34.01 | 0.512 | 2138 |
| Lanczos u gibanju 1.5 | 104 | 4.17 | 0.47 | 35.66 | 0.9499 | 34.34 | 34.01 | 0.522 | 2133 |
| Lanczos na miru 1.0 | 104 | 4.17 | 0.47 | 35.83 | 0.9502 | 34.66 | 34.01 | 0.517 | 2128 |
| Lanczos na miru 1.5 | 104 | 4.17 | 0.47 | 35.84 | 0.9503 | 34.65 | 34.01 | 0.541 | 2122 |
| Lanczos na miru 2.0 | 104 | 4.17 | 0.47 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.547 | 2121 |
| Lanczos na miru 2.5 | 104 | 4.18 | 0.48 | 35.83 | 0.9503 | 34.64 | 34.01 | 0.543 | 2104 |
| tolerancija dubine 0.005 | 104 | 4.17 | 0.47 | 35.84 | 0.9503 | 34.64 | 34.01 | 0.539 | 2118 |
| tolerancija dubine 0.02 | 104 | 4.18 | 0.47 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.524 | 2119 |
| tolerancija dubine 0.1 | 104 | 4.14 | 0.47 | 35.84 | 0.9504 | 34.68 | 34.01 | 0.525 | 2144 |
| reactive maska 0 | 104 | 4.12 | 0.48 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.666 | 2076 |
| reactive maska 0.3 | 104 | 4.19 | 0.47 | 35.80 | 0.9502 | 34.60 | 34.01 | 0.542 | 2112 |
| reactive maska 1.0 | 104 | 4.16 | 0.47 | 35.56 | 0.9493 | 34.27 | 34.01 | 0.528 | 2123 |

## FSR: ablacije s mirnom kamerom (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 3.91 | 0.47 | 39.62 | 0.9798 | 52.96 | 83.17 | 0.536 | 2150 |
| bez dilatacije vektora | 104 | 3.90 | 0.45 | 37.47 | 0.9758 | 38.15 | 83.00 | 0.641 | 2221 |
| bez depth clipa (samo clamp boje) | 104 | 3.89 | 0.46 | 39.64 | 0.9799 | 54.36 | 82.80 | 0.516 | 2179 |
| bez lockova | 104 | 3.87 | 0.46 | 39.06 | 0.9723 | 52.53 | 83.20 | 0.508 | 2170 |
| lock siri clamp 1x | 104 | 3.92 | 0.46 | 39.40 | 0.9768 | 52.83 | 83.20 | 0.534 | 2164 |
| lock siri clamp 2x | 104 | 4.10 | 0.46 | 39.62 | 0.9798 | 52.96 | 83.19 | 0.520 | 2167 |
| lock siri clamp 4x | 104 | 4.11 | 0.47 | 39.65 | 0.9804 | 53.01 | 83.20 | 0.547 | 2126 |
| lock siri clamp 8x | 104 | 4.18 | 0.46 | 39.66 | 0.9805 | 53.03 | 83.20 | 0.532 | 2153 |
| lock prag kontrasta 0.2 | 104 | 4.24 | 0.46 | 39.80 | 0.9816 | 53.39 | 83.20 | 0.519 | 2167 |
| lock prag kontrasta 0.35 | 104 | 4.17 | 0.47 | 39.72 | 0.9809 | 53.16 | 83.00 | 0.537 | 2112 |
| lock prag kontrasta 0.5 | 104 | 4.28 | 0.47 | 39.62 | 0.9798 | 52.96 | 83.20 | 0.558 | 2126 |
| lock prag kontrasta 0.7 | 104 | 4.06 | 0.47 | 39.41 | 0.9771 | 52.72 | 83.20 | 0.529 | 2145 |
| Lanczos na miru 1.0 | 104 | 3.97 | 0.46 | 38.53 | 0.9705 | 53.51 | 83.00 | 0.535 | 2175 |
| Lanczos na miru 1.5 | 104 | 4.07 | 0.46 | 39.26 | 0.9773 | 52.91 | 83.20 | 0.503 | 2182 |
| Lanczos na miru 2.0 | 104 | 3.99 | 0.46 | 39.62 | 0.9798 | 52.96 | 83.06 | 0.496 | 2189 |
| Lanczos na miru 2.5 | 104 | 4.06 | 0.47 | 38.59 | 0.9766 | 49.50 | 83.20 | 0.540 | 2136 |
| M4 TAAU, za usporedbu | 104 | 3.88 | 0.36 | 38.71 | 0.9661 | 58.86 | 83.08 | 0.397 | 2812 |

## FSR: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU (M4), mirna kamera | 104 | 3.82 | 0.35 | 38.71 | 0.9661 | 58.86 | 83.00 | 0.388 | 2826 |
| FSR (M5), mirna kamera | 104 | 3.94 | 0.46 | 39.62 | 0.9798 | 52.96 | 82.77 | 0.526 | 2172 |
| FSR + RCAS, mirna kamera | 104 | 3.99 | 0.49 | 39.58 | 0.9803 | 51.91 | 82.99 | 0.556 | 2048 |
| TAAU (M4), 120 fps | 104 | 4.16 | 0.37 | 37.32 | 0.9614 | 41.63 | 37.63 | 0.403 | 2724 |
| FSR (M5), 120 fps | 104 | 4.26 | 0.47 | 37.80 | 0.9657 | 38.75 | 37.63 | 0.546 | 2138 |
| FSR + RCAS, 120 fps | 104 | 4.30 | 0.49 | 37.49 | 0.9635 | 38.26 | 37.63 | 0.562 | 2039 |
| TAAU (M4), 60 fps | 104 | 4.06 | 0.37 | 35.04 | 0.9417 | 37.20 | 34.01 | 0.406 | 2737 |
| FSR (M5), 60 fps | 104 | 4.19 | 0.47 | 35.84 | 0.9504 | 34.65 | 34.01 | 0.543 | 2131 |
| FSR + RCAS, 60 fps | 104 | 4.20 | 0.49 | 35.52 | 0.9477 | 34.25 | 34.01 | 0.547 | 2040 |
| TAAU (M4), 30 fps | 104 | 3.95 | 0.36 | 34.33 | 0.9342 | 32.93 | 30.79 | 0.435 | 2809 |
| FSR (M5), 30 fps | 104 | 4.07 | 0.46 | 35.37 | 0.9447 | 31.15 | 30.79 | 0.562 | 2158 |
| FSR + RCAS, 30 fps | 104 | 4.12 | 0.49 | 35.07 | 0.9419 | 30.91 | 30.79 | 0.583 | 2046 |

## Optical flow (M6): tocnost po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 1.14 | 0.64 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.737 | 1569 |
| 1080p native (bez upscalinga) | 104 | 1.39 | 0.86 | 7.28 | 1.37 | 33.26 | 61.7 | 85.0 | 1.003 | 1158 |
| 1080p, render 960x540, FSR | 104 | 1.08 | 0.57 | 7.07 | 1.39 | 34.69 | 62.0 | 87.4 | 0.673 | 1741 |
| 1280x720, render 854x480, FSR | 104 | 0.86 | 0.36 | 4.40 | 1.08 | 19.97 | 65.9 | 88.8 | 0.405 | 2801 |
| 4K, render 1920x1080, FSR | 104 | 2.31 | 1.91 | 13.54 | 1.65 | 71.24 | 60.0 | 85.7 | 2.139 | 523 |

## Optical flow: ablacije i pretrage parametara (Sponza, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 1.14 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.748 | 1578 |
| bez kandidata iz proslog okvira | 104 | 1.15 | 0.62 | 11.05 | 1.83 | 50.13 | 60.4 | 83.3 | 0.729 | 1603 |
| bez medijan filtra | 104 | 1.14 | 0.62 | 7.91 | 2.36 | 34.91 | 57.0 | 80.3 | 0.720 | 1613 |
| bez izbora kandidata pri prosirenju | 104 | 1.11 | 0.61 | 12.69 | 1.18 | 57.03 | 61.8 | 84.7 | 0.718 | 1640 |
| bez detekcije reza | 104 | 1.17 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.728 | 1597 |
| 3 razina piramide | 104 | 1.08 | 0.58 | 18.99 | 1.40 | 141.04 | 61.6 | 83.5 | 0.677 | 1716 |
| 4 razina piramide | 104 | 1.11 | 0.60 | 17.88 | 1.35 | 133.29 | 61.7 | 83.8 | 0.702 | 1656 |
| 5 razina piramide | 104 | 1.12 | 0.61 | 10.86 | 1.36 | 77.22 | 62.6 | 85.7 | 0.691 | 1650 |
| 6 razina piramide | 104 | 1.14 | 0.63 | 6.98 | 1.36 | 32.97 | 63.0 | 86.7 | 0.741 | 1583 |
| 7 razina piramide | 104 | 1.14 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.718 | 1596 |
| radijus pretrage 2 texela | 104 | 1.13 | 0.60 | 8.13 | 1.47 | 37.17 | 61.1 | 84.1 | 0.708 | 1655 |
| radijus pretrage 4 texela | 104 | 1.14 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.732 | 1595 |
| radijus pretrage 6 texela | 104 | 1.18 | 0.66 | 6.33 | 1.33 | 30.22 | 63.5 | 87.4 | 0.773 | 1515 |
| radijus pretrage 8 texela | 104 | 1.23 | 0.70 | 6.41 | 1.33 | 30.20 | 63.7 | 87.6 | 0.792 | 1430 |
| glatkoca 0/texel | 104 | 1.15 | 0.64 | 6.99 | 1.40 | 32.00 | 64.0 | 87.1 | 0.743 | 1570 |
| glatkoca 0.0002/texel | 104 | 1.15 | 0.63 | 6.66 | 1.36 | 30.49 | 63.9 | 87.3 | 0.727 | 1591 |
| glatkoca 0.0005/texel | 104 | 1.15 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.732 | 1598 |
| glatkoca 0.002/texel | 104 | 1.15 | 0.63 | 8.29 | 1.42 | 43.13 | 57.4 | 81.6 | 0.722 | 1587 |
| glatkoca 0.01/texel | 104 | 1.16 | 0.63 | 16.22 | 2.72 | 101.97 | 39.8 | 67.5 | 0.765 | 1578 |
| novelty 0 | 104 | 1.15 | 0.62 | 7.27 | 1.41 | 35.99 | 63.4 | 86.7 | 0.722 | 1601 |
| novelty 0.0005 | 104 | 1.16 | 0.62 | 7.51 | 1.41 | 36.39 | 62.7 | 86.1 | 0.717 | 1610 |
| novelty 0.001 | 104 | 1.13 | 0.62 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.719 | 1601 |
| novelty 0.004 | 104 | 1.16 | 0.62 | 8.25 | 1.23 | 34.54 | 62.4 | 85.9 | 0.724 | 1602 |
| novelty 0.01 | 104 | 1.14 | 0.63 | 10.87 | 1.19 | 44.99 | 62.2 | 85.6 | 0.719 | 1591 |
| novelty 1 | 104 | 1.16 | 0.64 | 11.94 | 1.21 | 49.95 | 62.0 | 85.2 | 0.752 | 1559 |
| rez: statistika maksimum | 104 | 1.15 | 0.62 | 17.23 | 1.36 | 129.04 | 58.8 | 80.6 | 0.729 | 1603 |
| rez: statistika srednja | 104 | 1.15 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.726 | 1584 |
| rez: statistika medijan | 104 | 1.13 | 0.63 | 7.24 | 1.36 | 33.74 | 62.7 | 86.3 | 0.725 | 1586 |

## Optical flow: tocnost u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 1.15 | 0.62 | 0.00 | 0.00 | 0.00 | 100.0 | 100.0 | 0.732 | 1603 |
| 120 fps | 120 | 1.13 | 0.63 | 2.82 | 1.52 | 7.65 | 66.0 | 89.2 | 0.767 | 1591 |
| 60 fps | 60 | 1.14 | 0.63 | 11.73 | 3.64 | 37.02 | 57.1 | 80.2 | 0.742 | 1583 |
| 30 fps | 30 | 1.17 | 0.62 | 36.61 | 8.71 | 111.30 | 47.7 | 70.6 | 0.734 | 1601 |

## Generiranje okvira (M7): kvaliteta po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 5.45 | 1.13 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.237 | 883 |
| 1080p native (FSR 1.0x) | 104 | 5.97 | 1.75 | 36.51 | 27.73 | 0.9629 | 26.14 | 0.6816 | 1.840 | 572 |
| 1080p, render 960x540, FSR | 104 | 5.29 | 0.98 | 33.64 | 27.55 | 0.9172 | 26.26 | 0.6887 | 1.053 | 1019 |
| 1280x720, render 854x480, FSR | 104 | 2.84 | 0.56 | 34.03 | 27.49 | 0.9304 | 26.77 | 0.7049 | 0.621 | 1777 |

## Generiranje okvira: ablacije (Sponza, 60 fps orbita, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 5.40 | 1.14 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.242 | 873 |
| samo game vektori | 104 | 5.22 | 0.95 | 34.91 | 26.74 | 0.9452 | 26.17 | 0.6832 | 1.023 | 1058 |
| samo optical flow | 104 | 5.40 | 1.06 | 33.87 | 25.92 | 0.9283 | 26.17 | 0.6832 | 1.157 | 948 |
| bez vektora (= blend) | 104 | 5.17 | 0.93 | 26.17 | 21.42 | 0.6832 | 26.17 | 0.6832 | 1.012 | 1081 |
| bez maski disokluzije | 104 | 5.48 | 1.09 | 35.01 | 27.52 | 0.9452 | 26.17 | 0.6832 | 1.208 | 914 |
| bez dilatiranih vektora | 104 | 5.41 | 1.14 | 35.06 | 27.89 | 0.9451 | 26.17 | 0.6832 | 1.260 | 877 |
| piramida: najblizi umjesto pozadine | 104 | 5.45 | 1.13 | 35.07 | 27.81 | 0.9449 | 26.17 | 0.6832 | 1.228 | 886 |
| s bojom u prioritetu scattera | 104 | 5.59 | 1.20 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.307 | 831 |
| tezina toka uz game vektor 1 | 104 | 5.43 | 1.15 | 34.93 | 27.85 | 0.9431 | 26.17 | 0.6832 | 1.229 | 870 |
| tezina toka uz game vektor 0.5 | 104 | 5.44 | 1.14 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.274 | 876 |
| tezina toka uz game vektor 0.25 | 104 | 5.42 | 1.13 | 35.12 | 27.66 | 0.9456 | 26.17 | 0.6832 | 1.240 | 882 |
| tezina toka uz game vektor 0.1 | 104 | 5.42 | 1.16 | 35.09 | 27.43 | 0.9457 | 26.17 | 0.6832 | 1.265 | 865 |
| 1 razina piramide polja | 104 | 5.47 | 1.11 | 34.79 | 28.25 | 0.9426 | 26.17 | 0.6832 | 1.191 | 899 |
| 3 razina piramide polja | 104 | 5.52 | 1.13 | 34.91 | 27.91 | 0.9438 | 26.17 | 0.6832 | 1.215 | 882 |
| 5 razina piramide polja | 104 | 5.51 | 1.13 | 35.02 | 27.75 | 0.9446 | 26.17 | 0.6832 | 1.223 | 882 |
| 7 razina piramide polja | 104 | 5.51 | 1.14 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.235 | 876 |
| ostrina slaganja boja 0 | 104 | 5.51 | 1.13 | 34.97 | 27.85 | 0.9440 | 26.17 | 0.6832 | 1.236 | 882 |
| ostrina slaganja boja 6 | 104 | 5.52 | 1.14 | 35.06 | 27.91 | 0.9445 | 26.17 | 0.6832 | 1.230 | 874 |
| ostrina slaganja boja 24 | 104 | 5.51 | 1.13 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.218 | 885 |
| ostrina slaganja boja 96 | 104 | 5.55 | 1.14 | 35.09 | 27.90 | 0.9452 | 26.17 | 0.6832 | 1.247 | 874 |
| tolerancija dubine 0.005 | 104 | 5.45 | 1.15 | 35.07 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.250 | 873 |
| tolerancija dubine 0.02 | 104 | 5.52 | 1.12 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.282 | 891 |
| tolerancija dubine 0.08 | 104 | 5.48 | 1.21 | 35.09 | 27.79 | 0.9450 | 26.17 | 0.6832 | 1.427 | 824 |
| bez inpaintinga slike (prolazi 8-9) | 104 | 5.41 | 1.15 | 35.07 | 27.81 | 0.9449 | 26.17 | 0.6832 | 1.324 | 870 |
| bez odbacivanja uzoraka izvan ekrana | 104 | 5.41 | 1.14 | 35.05 | 27.90 | 0.9446 | 26.17 | 0.6832 | 1.234 | 878 |
| kao M7: bez inpaintinga i provjere granica | 104 | 5.40 | 1.10 | 35.03 | 27.80 | 0.9445 | 26.17 | 0.6832 | 1.194 | 912 |
| prag pokrivenosti inpaintinga 0.1 | 104 | 5.43 | 1.14 | 35.08 | 27.78 | 0.9450 | 26.17 | 0.6832 | 1.226 | 881 |
| prag pokrivenosti inpaintinga 0.3 | 104 | 5.45 | 1.14 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.223 | 879 |
| prag pokrivenosti inpaintinga 0.6 | 104 | 5.42 | 1.13 | 35.07 | 27.85 | 0.9449 | 26.17 | 0.6832 | 1.233 | 881 |
| proceduralna scena: Sve zadano | 104 | 3.75 | 0.91 | 34.06 | 33.74 | 0.9707 | 31.61 | 0.9475 | 1.053 | 1097 |
| proceduralna scena: samo game vektori | 104 | 3.46 | 0.71 | 33.68 | 33.35 | 0.9683 | 31.61 | 0.9475 | 0.816 | 1417 |
| proceduralna scena: samo optical flow | 104 | 3.72 | 0.86 | 33.50 | 32.88 | 0.9668 | 31.61 | 0.9475 | 1.013 | 1160 |
| proceduralna scena: bez vektora (= blend) | 104 | 3.41 | 0.69 | 31.61 | 31.13 | 0.9475 | 31.61 | 0.9475 | 0.905 | 1454 |
| proceduralna scena: bez maski disokluzije | 104 | 3.75 | 0.88 | 34.10 | 33.75 | 0.9710 | 31.61 | 0.9475 | 0.971 | 1142 |
| proceduralna scena: piramida: najblizi umjesto pozadine | 104 | 3.76 | 0.92 | 34.06 | 33.74 | 0.9707 | 31.61 | 0.9475 | 1.021 | 1088 |
| proceduralna scena: s bojom u prioritetu scattera | 104 | 3.84 | 0.94 | 34.06 | 33.74 | 0.9707 | 31.61 | 0.9475 | 1.066 | 1068 |
| proceduralna scena: bez inpaintinga slike (prolazi 8-9) | 104 | 3.75 | 0.88 | 34.07 | 33.73 | 0.9708 | 31.61 | 0.9475 | 0.970 | 1140 |
| proceduralna scena: bez odbacivanja uzoraka izvan ekrana | 104 | 3.77 | 0.92 | 34.06 | 33.73 | 0.9707 | 31.61 | 0.9475 | 1.034 | 1087 |
| proceduralna scena: kao M7: bez inpaintinga i provjere granica | 104 | 3.80 | 0.84 | 34.07 | 33.73 | 0.9708 | 31.61 | 0.9475 | 1.077 | 1184 |
| proceduralna scena: tezina toka uz game vektor 1 | 104 | 3.82 | 0.88 | 34.08 | 33.73 | 0.9710 | 31.61 | 0.9475 | 0.991 | 1142 |
| proceduralna scena: tezina toka uz game vektor 0.5 | 104 | 3.87 | 0.89 | 34.06 | 33.74 | 0.9707 | 31.61 | 0.9475 | 1.032 | 1127 |
| proceduralna scena: tezina toka uz game vektor 0.25 | 104 | 3.78 | 0.93 | 34.02 | 33.71 | 0.9704 | 31.61 | 0.9475 | 1.060 | 1081 |
| proceduralna scena: tezina toka uz game vektor 0.1 | 104 | 3.78 | 0.92 | 33.96 | 33.63 | 0.9699 | 31.61 | 0.9475 | 1.070 | 1086 |

## Generiranje okvira: kvaliteta u ovisnosti o brzini kamere (Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 5.24 | 1.13 | 39.67 | 39.56 | 0.9801 | 39.68 | 0.9801 | 1.278 | 885 |
| 120 fps | 120 | 5.47 | 1.13 | 35.97 | 31.21 | 0.9553 | 27.22 | 0.7522 | 1.293 | 885 |
| 60 fps | 60 | 5.46 | 1.13 | 34.72 | 27.78 | 0.9481 | 25.61 | 0.6980 | 1.255 | 887 |
| 30 fps | 30 | 5.49 | 1.13 | 32.95 | 24.11 | 0.9326 | 24.11 | 0.6529 | 1.316 | 883 |
| 30 fps, kao M7 | 30 | 5.51 | 1.10 | 32.90 | 23.96 | 0.9316 | 24.11 | 0.6529 | 1.214 | 913 |
| 20 fps | 20 | 5.58 | 1.13 | 31.03 | 20.25 | 0.9142 | 23.12 | 0.6260 | 1.379 | 883 |
| 20 fps, kao M7 | 20 | 5.49 | 1.08 | 31.17 | 20.34 | 0.9132 | 23.12 | 0.6260 | 1.440 | 929 |

## UI kompozicija (M8): HUD nakon generiranja okvira ili upečen prije njega

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | PSNR HUD (dB) | SSIM HUD | PSNR HUD blend (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| HUD nakon generiranja (kompozicija) | 104 | 6.92 | 1.15 | 35.11 | 27.81 | 0.9455 | 26.25 | 0.6908 | 52.49 | 0.9981 | 40.15 | 1.236 | 867 |
| HUD upečen prije generiranja | 104 | 6.88 | 1.20 | 31.56 | 26.02 | 0.9402 | 26.25 | 0.6908 | 20.72 | 0.8739 | 40.15 | 1.286 | 833 |
| HUD upečen, 20 fps | 20 | 7.17 | 1.14 | 27.55 | 18.93 | 0.9020 | 23.20 | 0.6333 | 16.97 | 0.6960 | 36.74 | 1.306 | 880 |
| HUD nakon generiranja, 20 fps | 20 | 7.20 | 1.12 | 31.08 | 20.25 | 0.9160 | 23.20 | 0.6333 | 48.34 | 0.9883 | 36.74 | 1.326 | 893 |

## Naucena mjesavina (M9) naspram heuristike, putanje izvan skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p Quality: heuristika | 104 | 5.42 | 1.14 | 35.08 | 27.80 | 0.9449 | 26.17 | 0.6832 | 1.246 | 879 |
| 1080p Quality: naucena mjesavina | 104 | 6.09 | 1.84 | 35.16 | 27.66 | 0.9464 | 26.17 | 0.6832 | 1.935 | 544 |
| 1080p native: heuristika | 104 | 5.97 | 1.73 | 36.51 | 27.73 | 0.9629 | 26.14 | 0.6816 | 1.884 | 577 |
| 1080p native: naucena mjesavina | 104 | 6.73 | 2.48 | 36.69 | 27.10 | 0.9651 | 26.14 | 0.6816 | 2.574 | 403 |
| 1080p Performance: heuristika | 104 | 5.27 | 0.99 | 33.64 | 27.55 | 0.9172 | 26.26 | 0.6887 | 1.074 | 1015 |
| 1080p Performance: naucena mjesavina | 104 | 5.89 | 1.67 | 33.65 | 27.39 | 0.9180 | 26.26 | 0.6887 | 1.801 | 599 |
| 720p Quality: heuristika | 104 | 2.84 | 0.56 | 34.03 | 27.49 | 0.9304 | 26.77 | 0.7049 | 0.624 | 1776 |
| 720p Quality: naucena mjesavina | 104 | 3.17 | 0.83 | 34.06 | 27.73 | 0.9317 | 26.77 | 0.7049 | 0.937 | 1204 |
| proceduralna scena: heuristika | 104 | 3.79 | 0.92 | 34.06 | 33.74 | 0.9707 | 31.61 | 0.9475 | 1.123 | 1089 |
| proceduralna scena: naucena mjesavina | 104 | 4.52 | 1.66 | 34.26 | 33.87 | 0.9722 | 31.61 | 0.9475 | 1.830 | 603 |
| Quality, 120 fps: heuristika | 120 | 5.60 | 1.11 | 35.97 | 31.21 | 0.9553 | 27.22 | 0.7522 | 1.339 | 902 |
| Quality, 120 fps: naucena mjesavina | 120 | 6.16 | 1.83 | 36.07 | 30.31 | 0.9568 | 27.22 | 0.7522 | 1.973 | 546 |
| Quality, 30 fps: heuristika | 30 | 5.64 | 1.11 | 32.95 | 24.11 | 0.9326 | 24.11 | 0.6529 | 1.323 | 899 |
| Quality, 30 fps: naucena mjesavina | 30 | 6.33 | 1.87 | 32.93 | 23.74 | 0.9354 | 24.11 | 0.6529 | 2.067 | 535 |
| Quality, 20 fps: heuristika | 20 | 5.76 | 1.11 | 31.03 | 20.25 | 0.9142 | 23.12 | 0.6260 | 1.220 | 898 |
| Quality, 20 fps: naucena mjesavina | 20 | 6.36 | 1.85 | 31.18 | 20.71 | 0.9200 | 23.12 | 0.6260 | 1.988 | 540 |

## Naucena mjesavina: velicina mreze i sastav skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| blend-c12-c24, Sponza Quality | 104 | 6.58 | 2.16 | 35.24 | 28.98 | 0.9468 | 26.17 | 0.6832 | 2.355 | 464 |
| blend-c12-c24, proceduralna scena | 104 | 4.88 | 1.94 | 34.35 | 33.91 | 0.9729 | 31.61 | 0.9475 | 2.218 | 515 |
| blend-c4-c8, Sponza Quality | 104 | 5.97 | 1.57 | 34.99 | 25.41 | 0.9464 | 26.17 | 0.6832 | 1.738 | 636 |
| blend-c4-c8, proceduralna scena | 104 | 4.28 | 1.32 | 34.15 | 33.70 | 0.9715 | 31.61 | 0.9475 | 1.491 | 757 |
| blend-c8-c16-s2, Sponza Quality | 104 | 6.26 | 1.84 | 35.26 | 26.08 | 0.9473 | 26.17 | 0.6832 | 1.970 | 544 |
| blend-c8-c16-s2, proceduralna scena | 104 | 4.51 | 1.66 | 34.27 | 33.85 | 0.9724 | 31.61 | 0.9475 | 1.811 | 601 |
| blend-c8-c16-s3, Sponza Quality | 104 | 6.27 | 1.85 | 35.21 | 26.28 | 0.9473 | 26.17 | 0.6832 | 2.041 | 542 |
| blend-c8-c16-s3, proceduralna scena | 104 | 4.50 | 1.67 | 34.37 | 33.93 | 0.9729 | 31.61 | 0.9475 | 1.794 | 599 |
| blend-c8-c16-sponza, Sponza Quality | 104 | 6.24 | 1.86 | 35.10 | 25.74 | 0.9462 | 26.17 | 0.6832 | 2.031 | 538 |
| blend-c8-c16-sponza, proceduralna scena | 104 | 4.52 | 1.68 | 34.05 | 33.55 | 0.9707 | 31.61 | 0.9475 | 1.892 | 594 |
| blend-c8-c16, Sponza Quality | 104 | 6.11 | 1.85 | 35.16 | 27.66 | 0.9464 | 26.17 | 0.6832 | 1.955 | 540 |
| blend-c8-c16, proceduralna scena | 104 | 4.53 | 1.66 | 34.26 | 33.87 | 0.9722 | 31.61 | 0.9475 | 1.858 | 604 |
