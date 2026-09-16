# Metrike

## Motion vectori: reprojekcija prethodnog okvira

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR reproj. (dB) | SSIM reproj. | PSNR bez reproj. (dB) | SSIM bez reproj. | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Native 1920x1080 | 104 | 2.29 | 0.13 | 30.93 | 0.9736 | 25.53 | 0.8882 | 0.118 | 7929 |
| Native 960x540 | 104 | 1.67 | 0.04 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.037 | 26301 |
| 1080p, render 1.5x manji | 104 | 1.86 | 0.08 | 30.62 | 0.9674 | 25.87 | 0.8917 | 0.078 | 12476 |
| 1080p, render 2.0x manji | 104 | 1.63 | 0.07 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.065 | 13704 |
| 960x540, sporo gibanje (dt/4) | 104 | 1.63 | 0.05 | 35.04 | 0.9859 | 32.93 | 0.9782 | 0.037 | 22168 |
| 960x540, brzo gibanje (dt*2) | 104 | 1.65 | 0.04 | 27.67 | 0.9507 | 22.74 | 0.8046 | 0.038 | 22765 |
| 960x540, bez filtriranja uzorka | 104 | 1.60 | 0.04 | 27.90 | 0.9259 | 24.59 | 0.8563 | 0.037 | 27546 |

## Cijena renderiranja bez upscalera (referentne brojke za ubrzanje)

| Konfiguracija | Frameovi | CPU ms | GPU ms | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|
| Sponza: native 1920x1080 | 104 | 0.24 | 0.38 | 0.636 | 2624 |
| Sponza: samo render 1280x720 | 104 | 0.24 | 0.21 | 0.372 | 4841 |
| Sponza: samo render 960x540 | 104 | 0.24 | 0.15 | 0.424 | 6483 |
| Sponza: native 3840x2160 | 104 | 0.26 | 1.35 | 1.590 | 743 |
| Sponza: samo render 1920x1080 | 104 | 0.26 | 0.48 | 0.728 | 2072 |

## Prostorni upscaleri (M3)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Nearest, Quality 1.5x | 104 | 2.45 | 0.10 | 30.09 | 0.9439 | 27.53 | 30.49 | 0.103 | 10119 |
| Sponza: Nearest, Quality 1.5x | 104 | 4.88 | 0.23 | 32.15 | 0.8957 | 29.75 | 34.05 | 0.363 | 4422 |
| Bilinear, Quality 1.5x | 104 | 2.44 | 0.11 | 33.29 | 0.9678 | 30.02 | 30.49 | 0.313 | 9496 |
| Sponza: Bilinear, Quality 1.5x | 104 | 4.91 | 0.22 | 35.27 | 0.9339 | 33.76 | 34.05 | 0.227 | 4496 |
| Bicubic, Quality 1.5x | 104 | 2.53 | 0.12 | 32.84 | 0.9662 | 29.05 | 30.49 | 0.326 | 8338 |
| Sponza: Bicubic, Quality 1.5x | 104 | 4.92 | 0.25 | 35.38 | 0.9422 | 32.46 | 34.05 | 0.446 | 3961 |
| FSR1 EASU+RCAS, Quality 1.5x | 104 | 2.53 | 0.19 | 31.13 | 0.9549 | 27.56 | 30.49 | 0.407 | 5316 |
| Sponza: FSR1 EASU+RCAS, Quality 1.5x | 104 | 4.99 | 0.30 | 33.05 | 0.9106 | 29.99 | 34.05 | 0.427 | 3344 |
| Nearest, Performance 2.0x | 104 | 2.44 | 0.07 | 29.30 | 0.9323 | 26.94 | 30.44 | 0.071 | 13893 |
| Sponza: Nearest, Performance 2.0x | 104 | 4.80 | 0.17 | 31.56 | 0.8786 | 29.00 | 33.99 | 0.168 | 5920 |
| Bilinear, Performance 2.0x | 104 | 2.46 | 0.08 | 31.27 | 0.9479 | 29.83 | 30.44 | 0.071 | 13109 |
| Sponza: Bilinear, Performance 2.0x | 104 | 4.90 | 0.17 | 33.11 | 0.8897 | 33.05 | 33.99 | 0.184 | 5788 |
| Bicubic, Performance 2.0x | 104 | 2.52 | 0.10 | 30.83 | 0.9467 | 28.54 | 30.44 | 0.090 | 10488 |
| Sponza: Bicubic, Performance 2.0x | 104 | 4.84 | 0.20 | 33.12 | 0.8994 | 31.36 | 33.99 | 0.380 | 5028 |
| FSR1 EASU+RCAS, Performance 2.0x | 104 | 2.51 | 0.15 | 30.05 | 0.9441 | 27.33 | 30.44 | 0.146 | 6518 |
| Sponza: FSR1 EASU+RCAS, Performance 2.0x | 104 | 4.93 | 0.24 | 31.88 | 0.8771 | 29.36 | 33.99 | 0.243 | 4181 |
| Sponza 4K: Nearest, Performance 2.0x | 104 | 14.07 | 0.54 | 33.82 | 0.9193 | 30.35 | 34.19 | 0.714 | 1859 |
| Sponza 4K: Bilinear, Performance 2.0x | 104 | 14.15 | 0.57 | 35.57 | 0.9337 | 33.90 | 34.19 | 0.733 | 1767 |
| Sponza 4K: Bicubic, Performance 2.0x | 104 | 14.14 | 0.64 | 35.73 | 0.9419 | 32.53 | 34.19 | 0.811 | 1562 |
| Sponza 4K: FSR1 EASU+RCAS, Performance 2.0x | 104 | 14.50 | 0.93 | 34.32 | 0.9232 | 30.74 | 34.19 | 1.133 | 1077 |

## RCAS: ablacija ostrine (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR1, sharpness 0 | 104 | 5.04 | 0.31 | 30.85 | 0.8658 | 28.55 | 34.05 | 0.505 | 3187 |
| FSR1, sharpness 0.25 | 104 | 5.04 | 0.30 | 33.05 | 0.9106 | 29.99 | 34.05 | 0.457 | 3308 |
| FSR1, sharpness 0.5 | 104 | 4.95 | 0.30 | 34.06 | 0.9267 | 30.74 | 34.05 | 0.313 | 3379 |
| FSR1, sharpness 1 | 104 | 5.06 | 0.29 | 34.87 | 0.9371 | 31.51 | 34.05 | 0.300 | 3421 |
| FSR1, sharpness 2 | 104 | 4.97 | 0.31 | 35.28 | 0.9407 | 32.12 | 34.05 | 0.523 | 3276 |
| FSR1, sharpness 4 | 104 | 5.00 | 0.31 | 35.40 | 0.9410 | 32.45 | 34.05 | 0.539 | 3230 |
| FSR1, EASU bez RCAS-a | 104 | 4.99 | 0.32 | 35.42 | 0.9408 | 32.53 | 34.05 | 0.514 | 3086 |

## Temporalni upscaler (M4): TAAU

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, Quality 1.5x | 104 | 2.60 | 0.18 | 32.53 | 0.9638 | 32.10 | 30.48 | 0.372 | 5422 |
| Sponza: TAAU, Quality 1.5x | 104 | 5.01 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.514 | 2907 |
| TAAU + RCAS, Quality 1.5x | 104 | 2.77 | 0.22 | 32.43 | 0.9643 | 31.65 | 30.48 | 0.442 | 4636 |
| Sponza: TAAU + RCAS, Quality 1.5x | 104 | 5.12 | 0.36 | 35.26 | 0.9508 | 35.99 | 33.99 | 0.525 | 2770 |
| TAAU, Performance 2.0x | 104 | 2.54 | 0.17 | 31.43 | 0.9526 | 32.21 | 30.45 | 0.386 | 6026 |
| Sponza: TAAU, Performance 2.0x | 104 | 4.98 | 0.27 | 33.82 | 0.9207 | 36.90 | 33.95 | 0.275 | 3672 |
| TAAU + RCAS, Performance 2.0x | 104 | 2.60 | 0.19 | 31.35 | 0.9531 | 31.72 | 30.45 | 0.354 | 5211 |
| Sponza: TAAU + RCAS, Performance 2.0x | 104 | 5.03 | 0.31 | 33.95 | 0.9274 | 35.99 | 33.95 | 0.486 | 3198 |

## TAAU: ablacije (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, sve zadano | 104 | 4.98 | 0.32 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.344 | 3104 |
| bez jittera (nema sto akumulirati) | 104 | 5.04 | 0.33 | 34.91 | 0.9443 | 37.29 | 34.04 | 0.492 | 3052 |
| akumulacija max 1 okvira | 104 | 5.01 | 0.33 | 35.25 | 0.9466 | 36.72 | 33.99 | 0.534 | 2992 |
| akumulacija max 2 okvira | 104 | 5.10 | 0.34 | 35.24 | 0.9465 | 36.78 | 33.99 | 0.511 | 2961 |
| akumulacija max 4 okvira | 104 | 5.04 | 0.33 | 35.21 | 0.9461 | 36.85 | 33.99 | 0.494 | 3055 |
| akumulacija max 8 okvira | 104 | 4.97 | 0.33 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.483 | 2986 |
| akumulacija max 16 okvira | 104 | 5.00 | 0.34 | 35.04 | 0.9442 | 37.02 | 33.99 | 0.544 | 2965 |
| akumulacija max 32 okvira | 104 | 5.08 | 0.33 | 34.90 | 0.9426 | 37.10 | 33.99 | 0.503 | 3033 |
| clamp 0.5 sigma | 104 | 5.00 | 0.33 | 34.59 | 0.9217 | 36.24 | 33.99 | 0.486 | 3060 |
| clamp 1 sigma | 104 | 5.03 | 0.34 | 35.30 | 0.9416 | 36.71 | 33.99 | 0.547 | 2947 |
| clamp 1.5 sigma | 104 | 5.06 | 0.34 | 35.23 | 0.9453 | 36.88 | 33.99 | 0.507 | 2975 |
| clamp 2 sigma | 104 | 5.04 | 0.33 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.534 | 2991 |
| clamp 4 sigma | 104 | 5.07 | 0.33 | 35.09 | 0.9452 | 36.95 | 33.99 | 0.501 | 3012 |
| rekonstrukcija 1/sigma^2 = 2 | 104 | 5.00 | 0.33 | 34.60 | 0.9248 | 37.97 | 33.99 | 0.493 | 3066 |
| rekonstrukcija 1/sigma^2 = 4 | 104 | 5.07 | 0.35 | 35.26 | 0.9433 | 37.08 | 33.99 | 0.531 | 2898 |
| rekonstrukcija 1/sigma^2 = 8 | 104 | 4.95 | 0.33 | 34.91 | 0.9444 | 37.03 | 33.99 | 0.478 | 3049 |
| rekonstrukcija 1/sigma^2 = 16 | 104 | 5.03 | 0.35 | 33.97 | 0.9346 | 37.50 | 33.99 | 0.581 | 2886 |
| skracivanje povijesti 0/px | 104 | 5.04 | 0.35 | 32.64 | 0.9073 | 38.58 | 33.99 | 0.579 | 2897 |
| skracivanje povijesti 0.4/px | 104 | 5.09 | 0.33 | 34.57 | 0.9390 | 37.55 | 33.99 | 0.485 | 3019 |
| skracivanje povijesti 0.8/px | 104 | 5.04 | 0.33 | 34.94 | 0.9432 | 37.19 | 33.99 | 0.503 | 2987 |
| skracivanje povijesti 1.6/px | 104 | 5.10 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.540 | 2959 |
| skracivanje povijesti 3.2/px | 104 | 5.03 | 0.34 | 35.23 | 0.9463 | 36.79 | 33.99 | 0.581 | 2930 |
| mip bias 0 | 104 | 4.94 | 0.30 | 34.57 | 0.9294 | 37.40 | 34.00 | 0.451 | 3374 |
| mip bias -0.585 | 104 | 4.95 | 0.30 | 35.09 | 0.9425 | 37.21 | 34.00 | 0.444 | 3296 |
| mip bias -1.585 | 104 | 5.06 | 0.33 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.472 | 3037 |
| TAAU + RCAS, ostrina 0.4 | 104 | 5.09 | 0.36 | 34.60 | 0.9440 | 34.58 | 33.99 | 0.574 | 2774 |
| TAAU + RCAS, ostrina 0.8 | 104 | 5.12 | 0.37 | 35.15 | 0.9504 | 35.50 | 33.99 | 0.567 | 2722 |
| TAAU + RCAS, ostrina 1.0 | 104 | 5.08 | 0.37 | 35.23 | 0.9509 | 35.78 | 33.99 | 0.565 | 2676 |
| TAAU + RCAS, ostrina 1.2 | 104 | 5.04 | 0.36 | 35.26 | 0.9508 | 35.99 | 33.99 | 0.548 | 2751 |
| TAAU + RCAS, ostrina 1.5 | 104 | 5.08 | 0.36 | 35.27 | 0.9502 | 36.22 | 33.99 | 0.528 | 2760 |
| TAAU + RCAS, ostrina 2.0 | 104 | 5.06 | 0.38 | 35.25 | 0.9490 | 36.47 | 33.99 | 0.587 | 2622 |

## TAAU: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| bicubic, mirna kamera | 104 | 4.80 | 0.23 | 35.83 | 0.9444 | 84.65 | 84.56 | 0.385 | 4281 |
| TAAU, mirna kamera | 104 | 4.94 | 0.32 | 39.41 | 0.9731 | 57.34 | 84.56 | 0.489 | 3161 |
| TAAU + RCAS, mirna kamera | 104 | 4.98 | 0.35 | 40.45 | 0.9825 | 55.16 | 84.56 | 0.542 | 2826 |
| bicubic, 120 fps | 104 | 5.01 | 0.25 | 37.19 | 0.9599 | 35.58 | 37.70 | 0.435 | 4014 |
| TAAU, 120 fps | 104 | 5.04 | 0.34 | 37.51 | 0.9646 | 41.23 | 37.61 | 0.535 | 2973 |
| TAAU + RCAS, 120 fps | 104 | 5.06 | 0.36 | 37.72 | 0.9692 | 40.03 | 37.61 | 0.545 | 2763 |
| bicubic, 60 fps | 104 | 4.89 | 0.24 | 35.38 | 0.9422 | 32.46 | 34.05 | 0.402 | 4100 |
| TAAU, 60 fps | 104 | 5.02 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.511 | 2968 |
| TAAU + RCAS, 60 fps | 104 | 5.00 | 0.37 | 35.26 | 0.9508 | 35.99 | 33.99 | 0.534 | 2737 |
| bicubic, 30 fps | 104 | 4.86 | 0.25 | 35.28 | 0.9394 | 30.00 | 30.81 | 0.437 | 4057 |
| TAAU, 30 fps | 104 | 4.95 | 0.32 | 34.36 | 0.9376 | 32.87 | 30.78 | 0.476 | 3096 |
| TAAU + RCAS, 30 fps | 104 | 5.01 | 0.35 | 34.40 | 0.9424 | 32.35 | 30.78 | 0.533 | 2847 |

## Puni upscaler (M5): dilatacija, depth clip, lockovi, Lanczos

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Sponza: FSR, NativeAA 1.0x | 104 | 5.28 | 0.67 | 38.14 | 0.9744 | 34.86 | 34.18 | 0.871 | 1497 |
| Sponza: FSR + RCAS, NativeAA 1.0x | 104 | 5.34 | 0.72 | 37.59 | 0.9713 | 34.43 | 34.18 | 0.926 | 1380 |
| Sponza: FSR, Quality 1.5x | 104 | 5.09 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.599 | 2332 |
| Sponza: FSR + RCAS, Quality 1.5x | 104 | 5.20 | 0.46 | 35.02 | 0.9408 | 33.71 | 33.99 | 0.625 | 2186 |
| Sponza: FSR, Balanced 1.7x | 104 | 5.07 | 0.40 | 34.69 | 0.9335 | 34.00 | 33.98 | 0.588 | 2474 |
| Sponza: FSR + RCAS, Balanced 1.7x | 104 | 5.12 | 0.42 | 34.35 | 0.9294 | 33.59 | 33.98 | 0.585 | 2380 |
| Sponza: FSR, Performance 2.0x | 104 | 5.01 | 0.36 | 33.72 | 0.9144 | 33.78 | 33.95 | 0.534 | 2780 |
| Sponza: FSR + RCAS, Performance 2.0x | 104 | 5.15 | 0.38 | 33.47 | 0.9112 | 33.41 | 33.95 | 0.553 | 2614 |
| Sponza: FSR, Ultra Performance 3.0x | 104 | 5.00 | 0.31 | 31.40 | 0.8530 | 32.98 | 33.89 | 0.485 | 3224 |
| Sponza: FSR + RCAS, Ultra Performance 3.0x | 104 | 5.00 | 0.34 | 31.29 | 0.8512 | 32.73 | 33.89 | 0.514 | 2962 |
| Sponza 4K: FSR, Performance 2.0x | 104 | 14.77 | 1.24 | 36.10 | 0.9491 | 34.17 | 34.16 | 1.463 | 805 |
| Sponza 4K: FSR + RCAS, Performance 2.0x | 104 | 14.84 | 1.43 | 35.81 | 0.9464 | 33.90 | 34.16 | 1.721 | 698 |

## FSR: ablacije (Sponza, Quality 1.5x, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 5.11 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.662 | 2301 |
| bez dilatacije vektora | 104 | 5.11 | 0.42 | 35.28 | 0.9450 | 33.19 | 33.99 | 0.581 | 2383 |
| bez depth clipa (samo clamp boje) | 104 | 5.12 | 0.44 | 35.34 | 0.9457 | 34.38 | 33.99 | 0.635 | 2264 |
| bez lockova | 104 | 5.23 | 0.46 | 35.53 | 0.9454 | 34.11 | 33.99 | 0.661 | 2164 |
| lock siri clamp 1x | 104 | 5.11 | 0.47 | 35.49 | 0.9455 | 34.12 | 33.99 | 0.688 | 2134 |
| lock siri clamp 2x | 104 | 5.15 | 0.44 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.632 | 2256 |
| lock siri clamp 4x | 104 | 5.12 | 0.45 | 35.46 | 0.9458 | 34.15 | 33.99 | 0.623 | 2209 |
| lock siri clamp 8x | 104 | 5.13 | 0.45 | 35.46 | 0.9458 | 34.15 | 33.99 | 0.617 | 2205 |
| lock prag kontrasta 0.2 | 104 | 5.17 | 0.44 | 35.40 | 0.9454 | 34.17 | 33.99 | 0.661 | 2256 |
| lock prag kontrasta 0.35 | 104 | 5.15 | 0.43 | 35.43 | 0.9456 | 34.16 | 33.99 | 0.591 | 2313 |
| lock prag kontrasta 0.5 | 104 | 5.11 | 0.45 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.661 | 2205 |
| lock prag kontrasta 0.7 | 104 | 5.25 | 0.44 | 35.50 | 0.9458 | 34.12 | 33.99 | 0.598 | 2283 |
| lock traje 2 okvira | 104 | 5.13 | 0.44 | 35.49 | 0.9458 | 34.14 | 33.99 | 0.653 | 2272 |
| lock traje 4 okvira | 104 | 5.21 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.599 | 2342 |
| lock traje 8 okvira | 104 | 5.16 | 0.43 | 35.45 | 0.9456 | 34.15 | 33.99 | 0.619 | 2307 |
| Lanczos u gibanju 0.75 | 104 | 5.09 | 0.44 | 35.21 | 0.9408 | 34.40 | 33.99 | 0.683 | 2251 |
| Lanczos u gibanju 1.0 | 104 | 5.11 | 0.44 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.668 | 2277 |
| Lanczos u gibanju 1.25 | 104 | 5.20 | 0.44 | 35.46 | 0.9466 | 33.95 | 33.99 | 0.699 | 2276 |
| Lanczos u gibanju 1.5 | 104 | 5.12 | 0.42 | 35.38 | 0.9462 | 33.88 | 33.99 | 0.601 | 2356 |
| Lanczos na miru 1.0 | 104 | 5.11 | 0.44 | 35.45 | 0.9456 | 34.15 | 33.99 | 0.656 | 2270 |
| Lanczos na miru 1.5 | 104 | 5.13 | 0.45 | 35.46 | 0.9457 | 34.15 | 33.99 | 0.675 | 2198 |
| Lanczos na miru 2.0 | 104 | 5.14 | 0.41 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.562 | 2415 |
| Lanczos na miru 2.5 | 104 | 5.15 | 0.43 | 35.46 | 0.9458 | 34.14 | 33.99 | 0.652 | 2324 |
| tolerancija dubine 0.005 | 104 | 5.09 | 0.42 | 35.46 | 0.9457 | 34.13 | 33.99 | 0.581 | 2390 |
| tolerancija dubine 0.02 | 104 | 5.22 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.576 | 2353 |
| tolerancija dubine 0.1 | 104 | 5.13 | 0.42 | 35.47 | 0.9458 | 34.17 | 33.99 | 0.587 | 2404 |
| reactive maska 0 | 104 | 5.16 | 0.44 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.630 | 2267 |
| reactive maska 0.3 | 104 | 5.14 | 0.43 | 35.43 | 0.9456 | 34.11 | 33.99 | 0.590 | 2330 |
| reactive maska 1.0 | 104 | 5.20 | 0.44 | 35.22 | 0.9448 | 33.86 | 33.99 | 0.663 | 2249 |

## FSR: ablacije s mirnom kamerom (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 5.09 | 0.41 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.625 | 2416 |
| bez dilatacije vektora | 104 | 5.08 | 0.41 | 37.57 | 0.9785 | 37.68 | 84.56 | 0.651 | 2444 |
| bez depth clipa (samo clamp boje) | 104 | 5.10 | 0.42 | 40.11 | 0.9831 | 53.08 | 84.56 | 0.590 | 2389 |
| bez lockova | 104 | 5.07 | 0.40 | 39.58 | 0.9767 | 51.30 | 84.56 | 0.527 | 2492 |
| lock siri clamp 1x | 104 | 5.02 | 0.41 | 39.88 | 0.9804 | 51.60 | 84.56 | 0.614 | 2434 |
| lock siri clamp 2x | 104 | 4.99 | 0.40 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.442 | 2483 |
| lock siri clamp 4x | 104 | 5.01 | 0.42 | 40.10 | 0.9836 | 51.79 | 84.56 | 0.652 | 2381 |
| lock siri clamp 8x | 104 | 5.07 | 0.43 | 40.11 | 0.9837 | 51.81 | 84.56 | 0.617 | 2329 |
| lock prag kontrasta 0.2 | 104 | 5.08 | 0.42 | 40.22 | 0.9845 | 52.25 | 84.56 | 0.654 | 2381 |
| lock prag kontrasta 0.35 | 104 | 5.09 | 0.41 | 40.15 | 0.9838 | 51.98 | 84.56 | 0.613 | 2423 |
| lock prag kontrasta 0.5 | 104 | 5.07 | 0.44 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.665 | 2299 |
| lock prag kontrasta 0.7 | 104 | 4.99 | 0.41 | 39.91 | 0.9809 | 51.49 | 84.56 | 0.557 | 2464 |
| Lanczos na miru 1.0 | 104 | 5.08 | 0.42 | 38.61 | 0.9720 | 52.03 | 84.56 | 0.585 | 2407 |
| Lanczos na miru 1.5 | 104 | 5.05 | 0.42 | 39.52 | 0.9795 | 51.56 | 84.56 | 0.621 | 2405 |
| Lanczos na miru 2.0 | 104 | 5.08 | 0.41 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.587 | 2422 |
| Lanczos na miru 2.5 | 104 | 5.03 | 0.41 | 38.92 | 0.9809 | 48.98 | 84.56 | 0.584 | 2430 |
| M4 TAAU, za usporedbu | 104 | 4.90 | 0.32 | 39.41 | 0.9731 | 57.34 | 84.56 | 0.544 | 3160 |

## FSR: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU (M4), mirna kamera | 104 | 4.93 | 0.33 | 39.41 | 0.9731 | 57.34 | 84.56 | 0.488 | 3058 |
| FSR (M5), mirna kamera | 104 | 5.11 | 0.41 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.578 | 2451 |
| FSR + RCAS, mirna kamera | 104 | 5.08 | 0.45 | 39.72 | 0.9810 | 50.64 | 84.56 | 0.681 | 2242 |
| TAAU (M4), 120 fps | 104 | 5.11 | 0.33 | 37.51 | 0.9646 | 41.23 | 37.61 | 0.500 | 3006 |
| FSR (M5), 120 fps | 104 | 5.24 | 0.45 | 37.53 | 0.9636 | 38.11 | 37.61 | 0.625 | 2236 |
| FSR + RCAS, 120 fps | 104 | 5.21 | 0.45 | 37.08 | 0.9598 | 37.57 | 37.61 | 0.627 | 2212 |
| TAAU (M4), 60 fps | 104 | 4.98 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.512 | 2942 |
| FSR (M5), 60 fps | 104 | 5.11 | 0.44 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.667 | 2257 |
| FSR + RCAS, 60 fps | 104 | 5.14 | 0.47 | 35.02 | 0.9408 | 33.71 | 33.99 | 0.698 | 2108 |
| TAAU (M4), 30 fps | 104 | 5.04 | 0.34 | 34.36 | 0.9376 | 32.87 | 30.78 | 0.484 | 2983 |
| FSR (M5), 30 fps | 104 | 5.13 | 0.43 | 34.93 | 0.9384 | 30.86 | 30.78 | 0.594 | 2315 |
| FSR + RCAS, 30 fps | 104 | 5.08 | 0.49 | 34.50 | 0.9330 | 30.59 | 30.78 | 0.716 | 2059 |

## Optical flow (M6): tocnost po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 1.13 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.022 | 1369 |
| 1080p native (bez upscalinga) | 104 | 1.30 | 0.90 | 7.17 | 1.40 | 33.30 | 61.6 | 85.0 | 1.128 | 1108 |
| 1080p, render 960x540, FSR | 104 | 1.05 | 0.65 | 7.10 | 1.42 | 34.68 | 61.0 | 87.0 | 0.869 | 1537 |
| 1280x720, render 854x480, FSR | 104 | 0.82 | 0.43 | 4.43 | 1.13 | 20.04 | 65.0 | 88.5 | 0.661 | 2329 |
| 4K, render 1920x1080, FSR | 104 | 2.34 | 2.02 | 13.71 | 1.62 | 73.40 | 59.6 | 85.6 | 2.255 | 495 |

## Optical flow: ablacije i pretrage parametara (Sponza, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 1.14 | 0.74 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.940 | 1360 |
| bez kandidata iz proslog okvira | 104 | 1.11 | 0.72 | 10.86 | 1.86 | 49.06 | 59.9 | 83.0 | 0.960 | 1398 |
| bez medijan filtra | 104 | 1.10 | 0.71 | 8.02 | 2.41 | 34.36 | 56.3 | 79.5 | 0.930 | 1412 |
| bez izbora kandidata pri prosirenju | 104 | 1.07 | 0.69 | 12.69 | 1.19 | 56.96 | 61.3 | 84.4 | 0.944 | 1444 |
| bez detekcije reza | 104 | 1.10 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.937 | 1367 |
| 3 razina piramide | 104 | 1.05 | 0.66 | 18.93 | 1.44 | 140.00 | 61.1 | 83.2 | 0.924 | 1515 |
| 4 razina piramide | 104 | 1.07 | 0.68 | 18.17 | 1.34 | 135.51 | 61.3 | 83.5 | 0.901 | 1470 |
| 5 razina piramide | 104 | 1.08 | 0.68 | 10.72 | 1.36 | 73.68 | 62.2 | 85.5 | 0.911 | 1462 |
| 6 razina piramide | 104 | 1.09 | 0.71 | 7.02 | 1.36 | 33.85 | 62.6 | 86.4 | 0.932 | 1400 |
| 7 razina piramide | 104 | 1.12 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.948 | 1376 |
| radijus pretrage 2 texela | 104 | 1.06 | 0.67 | 8.42 | 1.45 | 36.72 | 60.6 | 83.6 | 0.894 | 1503 |
| radijus pretrage 4 texela | 104 | 1.10 | 0.72 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.933 | 1397 |
| radijus pretrage 6 texela | 104 | 1.17 | 0.76 | 6.40 | 1.34 | 30.53 | 63.0 | 87.0 | 1.008 | 1320 |
| radijus pretrage 8 texela | 104 | 1.25 | 0.86 | 6.53 | 1.33 | 30.70 | 63.2 | 87.3 | 1.071 | 1167 |
| glatkoca 0/texel | 104 | 1.14 | 0.73 | 7.10 | 1.42 | 31.07 | 63.4 | 86.6 | 0.953 | 1362 |
| glatkoca 0.0002/texel | 104 | 1.12 | 0.76 | 6.70 | 1.37 | 31.18 | 63.5 | 87.0 | 0.950 | 1324 |
| glatkoca 0.0005/texel | 104 | 1.09 | 0.72 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.934 | 1390 |
| glatkoca 0.002/texel | 104 | 1.12 | 0.72 | 8.26 | 1.43 | 42.81 | 57.4 | 81.4 | 0.952 | 1379 |
| glatkoca 0.01/texel | 104 | 1.12 | 0.73 | 17.09 | 2.71 | 107.03 | 40.2 | 68.1 | 0.948 | 1366 |
| novelty 0 | 104 | 1.11 | 0.74 | 7.20 | 1.44 | 35.94 | 63.0 | 86.5 | 0.943 | 1345 |
| novelty 0.0005 | 104 | 1.12 | 0.71 | 7.20 | 1.41 | 34.23 | 62.4 | 86.1 | 0.953 | 1404 |
| novelty 0.001 | 104 | 1.11 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.936 | 1371 |
| novelty 0.004 | 104 | 1.14 | 0.74 | 8.18 | 1.25 | 34.63 | 61.9 | 85.5 | 0.960 | 1352 |
| novelty 0.01 | 104 | 1.11 | 0.71 | 10.85 | 1.21 | 45.00 | 61.7 | 85.2 | 0.947 | 1399 |
| novelty 1 | 104 | 1.13 | 0.76 | 11.76 | 1.23 | 47.66 | 61.5 | 84.9 | 0.941 | 1323 |
| rez: statistika maksimum | 104 | 1.13 | 0.74 | 17.28 | 1.36 | 129.04 | 58.4 | 80.2 | 0.951 | 1355 |
| rez: statistika srednja | 104 | 1.12 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.927 | 1366 |
| rez: statistika medijan | 104 | 1.11 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.938 | 1374 |

## Optical flow: tocnost u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 1.14 | 0.73 | 0.00 | 0.00 | 0.00 | 100.0 | 100.0 | 1.030 | 1377 |
| 120 fps | 120 | 1.09 | 0.71 | 2.85 | 1.44 | 7.63 | 65.6 | 88.9 | 0.938 | 1408 |
| 60 fps | 60 | 1.14 | 0.76 | 11.58 | 3.79 | 37.05 | 56.7 | 80.0 | 1.091 | 1307 |
| 30 fps | 30 | 1.11 | 0.74 | 36.62 | 9.07 | 109.81 | 47.3 | 70.1 | 0.941 | 1355 |

## Generiranje okvira (M7): kvaliteta po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 5.70 | 1.09 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.248 | 916 |
| 1080p native (FSR 1.0x) | 104 | 6.04 | 1.46 | 36.51 | 27.90 | 0.9630 | 26.12 | 0.6802 | 1.750 | 683 |
| 1080p, render 960x540, FSR | 104 | 5.57 | 0.94 | 33.54 | 27.34 | 0.9164 | 26.14 | 0.6806 | 1.215 | 1068 |
| 1280x720, render 854x480, FSR | 104 | 3.45 | 0.64 | 33.93 | 27.40 | 0.9287 | 26.64 | 0.6967 | 0.850 | 1552 |

## Generiranje okvira: ablacije (Sponza, 60 fps orbita, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 5.78 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.244 | 925 |
| samo game vektori | 104 | 5.40 | 0.73 | 34.85 | 26.68 | 0.9449 | 26.09 | 0.6765 | 0.881 | 1379 |
| samo optical flow | 104 | 5.67 | 0.99 | 33.70 | 25.82 | 0.9252 | 26.09 | 0.6765 | 1.277 | 1014 |
| bez vektora (= blend) | 104 | 5.31 | 0.66 | 26.09 | 21.39 | 0.6765 | 26.09 | 0.6765 | 0.797 | 1525 |
| bez maski disokluzije | 104 | 5.68 | 1.01 | 34.93 | 27.42 | 0.9445 | 26.09 | 0.6765 | 1.155 | 989 |
| bez dilatiranih vektora | 104 | 5.67 | 1.06 | 34.98 | 27.65 | 0.9444 | 26.09 | 0.6765 | 1.242 | 948 |
| piramida: najblizi umjesto pozadine | 104 | 5.67 | 1.05 | 34.99 | 27.58 | 0.9442 | 26.09 | 0.6765 | 1.243 | 952 |
| s bojom u prioritetu scattera | 104 | 5.66 | 1.07 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.344 | 931 |
| tezina toka uz game vektor 1 | 104 | 5.76 | 1.07 | 34.83 | 27.61 | 0.9420 | 26.09 | 0.6765 | 1.319 | 935 |
| tezina toka uz game vektor 0.5 | 104 | 5.70 | 1.04 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.171 | 963 |
| tezina toka uz game vektor 0.25 | 104 | 5.70 | 1.06 | 35.04 | 27.42 | 0.9451 | 26.09 | 0.6765 | 1.258 | 944 |
| tezina toka uz game vektor 0.1 | 104 | 5.70 | 1.07 | 35.02 | 27.20 | 0.9453 | 26.09 | 0.6765 | 1.283 | 932 |
| 1 razina piramide polja | 104 | 5.67 | 1.01 | 34.70 | 27.95 | 0.9418 | 26.09 | 0.6765 | 1.197 | 988 |
| 3 razina piramide polja | 104 | 5.70 | 1.02 | 34.83 | 27.65 | 0.9432 | 26.09 | 0.6765 | 1.193 | 985 |
| 5 razina piramide polja | 104 | 5.71 | 1.04 | 34.94 | 27.50 | 0.9439 | 26.09 | 0.6765 | 1.251 | 959 |
| 7 razina piramide polja | 104 | 5.70 | 1.06 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.249 | 947 |
| ostrina slaganja boja 0 | 104 | 5.72 | 1.06 | 34.88 | 27.74 | 0.9432 | 26.09 | 0.6765 | 1.246 | 948 |
| ostrina slaganja boja 6 | 104 | 5.76 | 1.06 | 34.97 | 27.70 | 0.9438 | 26.09 | 0.6765 | 1.286 | 944 |
| ostrina slaganja boja 24 | 104 | 5.73 | 1.05 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.260 | 950 |
| ostrina slaganja boja 96 | 104 | 5.68 | 1.06 | 35.01 | 27.61 | 0.9446 | 26.09 | 0.6765 | 1.257 | 944 |
| tolerancija dubine 0.005 | 104 | 5.67 | 1.06 | 34.98 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.238 | 943 |
| tolerancija dubine 0.02 | 104 | 5.74 | 1.07 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.262 | 934 |
| tolerancija dubine 0.08 | 104 | 5.74 | 1.05 | 35.01 | 27.55 | 0.9443 | 26.09 | 0.6765 | 1.201 | 952 |
| bez inpaintinga slike (prolazi 8-9) | 104 | 5.71 | 1.02 | 34.97 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.262 | 976 |
| bez odbacivanja uzoraka izvan ekrana | 104 | 5.72 | 1.07 | 34.96 | 27.65 | 0.9439 | 26.09 | 0.6765 | 1.269 | 933 |
| kao M7: bez inpaintinga i provjere granica | 104 | 5.73 | 1.02 | 34.94 | 27.56 | 0.9438 | 26.09 | 0.6765 | 1.198 | 984 |
| prag pokrivenosti inpaintinga 0.1 | 104 | 5.67 | 1.09 | 34.99 | 27.54 | 0.9443 | 26.09 | 0.6765 | 1.296 | 915 |
| prag pokrivenosti inpaintinga 0.3 | 104 | 5.70 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.332 | 929 |
| prag pokrivenosti inpaintinga 0.6 | 104 | 5.68 | 1.06 | 34.98 | 27.60 | 0.9442 | 26.09 | 0.6765 | 1.280 | 944 |
| proceduralna scena: Sve zadano | 104 | 3.26 | 0.82 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 0.938 | 1222 |
| proceduralna scena: samo game vektori | 104 | 2.92 | 0.53 | 33.62 | 33.29 | 0.9674 | 31.56 | 0.9467 | 0.698 | 1895 |
| proceduralna scena: samo optical flow | 104 | 3.21 | 0.76 | 33.47 | 32.66 | 0.9661 | 31.56 | 0.9467 | 0.967 | 1313 |
| proceduralna scena: bez vektora (= blend) | 104 | 2.88 | 0.47 | 31.56 | 31.10 | 0.9467 | 31.56 | 0.9467 | 0.605 | 2135 |
| proceduralna scena: bez maski disokluzije | 104 | 3.22 | 0.82 | 34.01 | 33.67 | 0.9700 | 31.56 | 0.9467 | 1.016 | 1213 |
| proceduralna scena: piramida: najblizi umjesto pozadine | 104 | 3.25 | 0.84 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 1.048 | 1194 |
| proceduralna scena: s bojom u prioritetu scattera | 104 | 3.28 | 0.84 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 1.043 | 1185 |
| proceduralna scena: bez inpaintinga slike (prolazi 8-9) | 104 | 3.21 | 0.79 | 33.98 | 33.65 | 0.9698 | 31.56 | 0.9467 | 1.029 | 1268 |
| proceduralna scena: bez odbacivanja uzoraka izvan ekrana | 104 | 3.26 | 0.84 | 33.97 | 33.65 | 0.9698 | 31.56 | 0.9467 | 1.027 | 1190 |
| proceduralna scena: kao M7: bez inpaintinga i provjere granica | 104 | 3.30 | 0.78 | 33.98 | 33.65 | 0.9698 | 31.56 | 0.9467 | 0.985 | 1278 |
| proceduralna scena: tezina toka uz game vektor 1 | 104 | 3.26 | 0.84 | 33.98 | 33.65 | 0.9700 | 31.56 | 0.9467 | 1.039 | 1188 |
| proceduralna scena: tezina toka uz game vektor 0.5 | 104 | 3.29 | 0.81 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 0.941 | 1238 |
| proceduralna scena: tezina toka uz game vektor 0.25 | 104 | 3.30 | 0.85 | 33.93 | 33.63 | 0.9694 | 31.56 | 0.9467 | 1.044 | 1176 |
| proceduralna scena: tezina toka uz game vektor 0.1 | 104 | 3.25 | 0.83 | 33.88 | 33.57 | 0.9690 | 31.56 | 0.9467 | 1.021 | 1203 |

## Generiranje okvira: kvaliteta u ovisnosti o brzini kamere (Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 5.64 | 1.00 | 40.15 | 40.04 | 0.9834 | 40.16 | 0.9834 | 1.207 | 996 |
| 120 fps | 120 | 5.73 | 1.04 | 35.94 | 31.20 | 0.9553 | 27.16 | 0.7479 | 1.242 | 960 |
| 60 fps | 60 | 5.73 | 1.08 | 34.63 | 27.49 | 0.9471 | 25.55 | 0.6918 | 1.327 | 929 |
| 30 fps | 30 | 5.79 | 1.06 | 32.84 | 24.11 | 0.9307 | 24.05 | 0.6453 | 1.171 | 942 |
| 30 fps, kao M7 | 30 | 5.75 | 1.01 | 32.79 | 23.98 | 0.9297 | 24.05 | 0.6453 | 1.226 | 989 |
| 20 fps | 20 | 5.84 | 1.09 | 30.96 | 20.22 | 0.9120 | 23.07 | 0.6177 | 1.286 | 921 |
| 20 fps, kao M7 | 20 | 5.67 | 0.98 | 31.09 | 20.32 | 0.9109 | 23.07 | 0.6177 | 1.138 | 1016 |

## UI kompozicija (M8): HUD nakon generiranja okvira ili upečen prije njega

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | PSNR HUD (dB) | SSIM HUD | PSNR HUD blend (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| HUD nakon generiranja (kompozicija) | 104 | 8.56 | 1.09 | 35.03 | 27.57 | 0.9448 | 26.17 | 0.6843 | 52.36 | 0.9980 | 40.06 | 1.318 | 921 |
| HUD upečen prije generiranja | 104 | 8.44 | 1.11 | 31.52 | 26.01 | 0.9395 | 26.17 | 0.6843 | 20.73 | 0.8739 | 40.06 | 1.304 | 905 |
| HUD upečen, 20 fps | 20 | 8.52 | 1.04 | 27.53 | 18.91 | 0.8998 | 23.15 | 0.6251 | 16.98 | 0.6961 | 36.70 | 1.244 | 959 |
| HUD nakon generiranja, 20 fps | 20 | 8.51 | 1.05 | 31.00 | 20.23 | 0.9138 | 23.15 | 0.6251 | 48.21 | 0.9882 | 36.70 | 1.202 | 957 |

## Naucena mjesavina (M9) naspram heuristike, putanje izvan skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p Quality: heuristika | 104 | 5.73 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.267 | 930 |
| 1080p Quality: naucena mjesavina | 104 | 6.64 | 2.20 | 35.09 | 27.57 | 0.9459 | 26.09 | 0.6765 | 2.512 | 455 |
| 1080p native: heuristika | 104 | 6.11 | 1.44 | 36.51 | 27.90 | 0.9630 | 26.12 | 0.6802 | 1.726 | 693 |
| 1080p native: naucena mjesavina | 104 | 7.07 | 2.62 | 36.69 | 27.15 | 0.9653 | 26.12 | 0.6802 | 2.914 | 381 |
| 1080p Performance: heuristika | 104 | 5.62 | 0.93 | 33.54 | 27.34 | 0.9164 | 26.14 | 0.6806 | 1.067 | 1076 |
| 1080p Performance: naucena mjesavina | 104 | 6.60 | 1.97 | 33.57 | 27.33 | 0.9174 | 26.14 | 0.6806 | 2.358 | 507 |
| 720p Quality: heuristika | 104 | 3.46 | 0.63 | 33.93 | 27.40 | 0.9287 | 26.64 | 0.6967 | 0.843 | 1583 |
| 720p Quality: naucena mjesavina | 104 | 3.97 | 1.13 | 33.97 | 27.62 | 0.9302 | 26.64 | 0.6967 | 1.265 | 887 |
| proceduralna scena: heuristika | 104 | 3.29 | 0.85 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 0.969 | 1172 |
| proceduralna scena: naucena mjesavina | 104 | 4.36 | 1.82 | 34.15 | 33.76 | 0.9711 | 31.56 | 0.9467 | 2.055 | 551 |
| Quality, 120 fps: heuristika | 120 | 5.74 | 1.05 | 35.94 | 31.20 | 0.9553 | 27.16 | 0.7479 | 1.233 | 954 |
| Quality, 120 fps: naucena mjesavina | 120 | 6.70 | 2.19 | 36.02 | 30.26 | 0.9568 | 27.16 | 0.7479 | 2.556 | 456 |
| Quality, 30 fps: heuristika | 30 | 5.86 | 1.04 | 32.84 | 24.11 | 0.9307 | 24.05 | 0.6453 | 1.207 | 962 |
| Quality, 30 fps: naucena mjesavina | 30 | 6.77 | 2.22 | 32.81 | 23.70 | 0.9334 | 24.05 | 0.6453 | 2.521 | 451 |
| Quality, 20 fps: heuristika | 20 | 5.76 | 1.11 | 30.96 | 20.22 | 0.9120 | 23.07 | 0.6177 | 1.514 | 897 |
| Quality, 20 fps: naucena mjesavina | 20 | 6.85 | 2.26 | 31.09 | 20.72 | 0.9176 | 23.07 | 0.6177 | 2.977 | 442 |

## Naucena mjesavina: velicina mreze i sastav skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| blend-c12-c24, Sponza Quality | 104 | 7.78 | 3.31 | 35.17 | 28.92 | 0.9463 | 26.09 | 0.6765 | 3.677 | 302 |
| blend-c12-c24, proceduralna scena | 104 | 5.36 | 3.01 | 34.24 | 33.83 | 0.9718 | 31.56 | 0.9467 | 3.384 | 332 |
| blend-c4-c8, Sponza Quality | 104 | 6.15 | 1.52 | 34.91 | 25.26 | 0.9456 | 26.09 | 0.6765 | 1.759 | 658 |
| blend-c4-c8, proceduralna scena | 104 | 3.77 | 1.27 | 34.04 | 33.63 | 0.9704 | 31.56 | 0.9467 | 1.542 | 788 |
| blend-c8-c16-s2, Sponza Quality | 104 | 6.67 | 2.22 | 35.19 | 25.98 | 0.9470 | 26.09 | 0.6765 | 2.548 | 450 |
| blend-c8-c16-s2, proceduralna scena | 104 | 4.39 | 1.84 | 34.16 | 33.76 | 0.9713 | 31.56 | 0.9467 | 2.126 | 544 |
| blend-c8-c16-s3, Sponza Quality | 104 | 6.66 | 2.16 | 35.12 | 25.68 | 0.9467 | 26.09 | 0.6765 | 2.496 | 463 |
| blend-c8-c16-s3, proceduralna scena | 104 | 4.41 | 1.84 | 34.27 | 33.86 | 0.9719 | 31.56 | 0.9467 | 2.058 | 543 |
| blend-c8-c16-sponza, Sponza Quality | 104 | 6.67 | 2.20 | 35.05 | 25.99 | 0.9460 | 26.09 | 0.6765 | 2.560 | 455 |
| blend-c8-c16-sponza, proceduralna scena | 104 | 4.40 | 1.80 | 33.95 | 33.43 | 0.9697 | 31.56 | 0.9467 | 2.072 | 556 |
| blend-c8-c16, Sponza Quality | 104 | 6.67 | 2.19 | 35.09 | 27.57 | 0.9459 | 26.09 | 0.6765 | 2.504 | 456 |
| blend-c8-c16, proceduralna scena | 104 | 4.41 | 1.83 | 34.15 | 33.76 | 0.9711 | 31.56 | 0.9467 | 2.098 | 546 |
