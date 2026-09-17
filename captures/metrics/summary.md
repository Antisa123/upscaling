# Metrike

## Motion vectori: reprojekcija prethodnog okvira

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR reproj. (dB) | SSIM reproj. | PSNR bez reproj. (dB) | SSIM bez reproj. | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Native 1920x1080 | 104 | 2.17 | 0.12 | 30.93 | 0.9736 | 25.53 | 0.8882 | 0.117 | 8490 |
| Native 960x540 | 104 | 1.51 | 0.04 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.038 | 22980 |
| 1080p, render 1.5x manji | 104 | 1.76 | 0.08 | 30.62 | 0.9674 | 25.87 | 0.8917 | 0.078 | 13159 |
| 1080p, render 2.0x manji | 104 | 1.82 | 0.06 | 30.46 | 0.9641 | 26.17 | 0.8976 | 0.062 | 16274 |
| 960x540, sporo gibanje (dt/4) | 104 | 1.60 | 0.04 | 35.04 | 0.9859 | 32.93 | 0.9782 | 0.037 | 24915 |
| 960x540, brzo gibanje (dt*2) | 104 | 1.96 | 0.04 | 27.67 | 0.9507 | 22.74 | 0.8046 | 0.039 | 23855 |
| 960x540, bez filtriranja uzorka | 104 | 1.82 | 0.04 | 27.90 | 0.9259 | 24.59 | 0.8563 | 0.038 | 23685 |

## Cijena renderiranja bez upscalera (referentne brojke za ubrzanje)

| Konfiguracija | Frameovi | CPU ms | GPU ms | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|
| Sponza: native 1920x1080 | 104 | 0.27 | 0.33 | 0.350 | 3061 |
| Sponza: samo render 1280x720 | 104 | 0.26 | 0.19 | 0.201 | 5289 |
| Sponza: samo render 960x540 | 104 | 0.27 | 0.14 | 0.143 | 7262 |
| Sponza: native 3840x2160 | 104 | 0.17 | 1.31 | 1.582 | 765 |
| Sponza: samo render 1920x1080 | 104 | 0.24 | 0.47 | 0.719 | 2147 |

## Prostorni upscaleri (M3)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Nearest, Quality 1.5x | 104 | 2.49 | 0.09 | 30.09 | 0.9439 | 27.53 | 30.49 | 0.093 | 10542 |
| Sponza: Nearest, Quality 1.5x | 104 | 4.98 | 0.24 | 32.15 | 0.8957 | 29.75 | 34.05 | 0.418 | 4250 |
| Bilinear, Quality 1.5x | 104 | 2.43 | 0.11 | 33.29 | 0.9678 | 30.02 | 30.49 | 0.318 | 9306 |
| Sponza: Bilinear, Quality 1.5x | 104 | 4.82 | 0.23 | 35.27 | 0.9339 | 33.76 | 34.05 | 0.475 | 4280 |
| Bicubic, Quality 1.5x | 104 | 2.45 | 0.12 | 32.84 | 0.9662 | 29.05 | 30.49 | 0.354 | 8130 |
| Sponza: Bicubic, Quality 1.5x | 104 | 4.80 | 0.24 | 35.38 | 0.9422 | 32.46 | 34.05 | 0.300 | 4146 |
| FSR1 EASU+RCAS, Quality 1.5x | 104 | 2.51 | 0.18 | 31.13 | 0.9549 | 27.56 | 30.49 | 0.407 | 5550 |
| Sponza: FSR1 EASU+RCAS, Quality 1.5x | 104 | 4.85 | 0.30 | 33.05 | 0.9106 | 29.99 | 34.05 | 0.476 | 3310 |
| Nearest, Performance 2.0x | 104 | 2.39 | 0.07 | 29.30 | 0.9323 | 26.94 | 30.44 | 0.069 | 13825 |
| Sponza: Nearest, Performance 2.0x | 104 | 4.80 | 0.18 | 31.56 | 0.8786 | 29.00 | 33.99 | 0.374 | 5599 |
| Bilinear, Performance 2.0x | 104 | 2.40 | 0.08 | 31.27 | 0.9479 | 29.83 | 30.44 | 0.072 | 12810 |
| Sponza: Bilinear, Performance 2.0x | 104 | 4.70 | 0.18 | 33.11 | 0.8897 | 33.05 | 33.99 | 0.378 | 5571 |
| Bicubic, Performance 2.0x | 104 | 2.41 | 0.09 | 30.83 | 0.9467 | 28.54 | 30.44 | 0.089 | 10578 |
| Sponza: Bicubic, Performance 2.0x | 104 | 4.72 | 0.19 | 33.12 | 0.8994 | 31.36 | 33.99 | 0.338 | 5234 |
| FSR1 EASU+RCAS, Performance 2.0x | 104 | 2.47 | 0.17 | 30.05 | 0.9441 | 27.33 | 30.44 | 0.331 | 6027 |
| Sponza: FSR1 EASU+RCAS, Performance 2.0x | 104 | 4.87 | 0.24 | 31.88 | 0.8771 | 29.36 | 33.99 | 0.241 | 4152 |
| Sponza 4K: Nearest, Performance 2.0x | 104 | 13.77 | 0.57 | 33.82 | 0.9193 | 30.35 | 34.19 | 0.738 | 1759 |
| Sponza 4K: Bilinear, Performance 2.0x | 104 | 14.05 | 0.59 | 35.57 | 0.9337 | 33.90 | 34.19 | 0.807 | 1709 |
| Sponza 4K: Bicubic, Performance 2.0x | 104 | 14.05 | 0.68 | 35.73 | 0.9419 | 32.53 | 34.19 | 0.995 | 1479 |
| Sponza 4K: FSR1 EASU+RCAS, Performance 2.0x | 104 | 14.37 | 0.94 | 34.32 | 0.9232 | 30.74 | 34.19 | 1.082 | 1063 |

## RCAS: ablacija ostrine (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR1, sharpness 0 | 104 | 4.85 | 0.32 | 30.85 | 0.8658 | 28.55 | 34.05 | 0.555 | 3147 |
| FSR1, sharpness 0.25 | 104 | 4.98 | 0.33 | 33.05 | 0.9106 | 29.99 | 34.05 | 0.549 | 3037 |
| FSR1, sharpness 0.5 | 104 | 5.00 | 0.32 | 34.06 | 0.9267 | 30.74 | 34.05 | 0.513 | 3139 |
| FSR1, sharpness 1 | 104 | 4.85 | 0.31 | 34.87 | 0.9371 | 31.51 | 34.05 | 0.526 | 3192 |
| FSR1, sharpness 2 | 104 | 4.97 | 0.31 | 35.28 | 0.9407 | 32.12 | 34.05 | 0.511 | 3273 |
| FSR1, sharpness 4 | 104 | 5.08 | 0.32 | 35.40 | 0.9410 | 32.45 | 34.05 | 0.539 | 3161 |
| FSR1, EASU bez RCAS-a | 104 | 4.86 | 0.31 | 35.42 | 0.9408 | 32.53 | 34.05 | 0.522 | 3245 |

## Temporalni upscaler (M4): TAAU

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, Quality 1.5x | 104 | 2.58 | 0.20 | 32.53 | 0.9638 | 32.10 | 30.48 | 0.443 | 4984 |
| Sponza: TAAU, Quality 1.5x | 104 | 5.02 | 0.33 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.511 | 2996 |
| TAAU + RCAS, Quality 1.5x | 104 | 2.65 | 0.22 | 32.43 | 0.9643 | 31.65 | 30.48 | 0.445 | 4618 |
| Sponza: TAAU + RCAS, Quality 1.5x | 104 | 5.03 | 0.36 | 35.26 | 0.9508 | 35.99 | 33.99 | 0.589 | 2758 |
| TAAU, Performance 2.0x | 104 | 2.51 | 0.16 | 31.43 | 0.9526 | 32.21 | 30.45 | 0.384 | 6282 |
| Sponza: TAAU, Performance 2.0x | 104 | 4.93 | 0.31 | 33.82 | 0.9207 | 36.90 | 33.95 | 0.511 | 3246 |
| TAAU + RCAS, Performance 2.0x | 104 | 2.60 | 0.17 | 31.35 | 0.9531 | 31.72 | 30.45 | 0.168 | 5854 |
| Sponza: TAAU + RCAS, Performance 2.0x | 104 | 4.92 | 0.31 | 33.95 | 0.9274 | 35.99 | 33.95 | 0.327 | 3269 |

## TAAU: ablacije (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU, sve zadano | 104 | 5.04 | 0.37 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.606 | 2735 |
| bez jittera (nema sto akumulirati) | 104 | 4.93 | 0.36 | 34.91 | 0.9443 | 37.29 | 34.04 | 0.567 | 2811 |
| akumulacija max 1 okvira | 104 | 5.07 | 0.34 | 35.25 | 0.9466 | 36.72 | 33.99 | 0.508 | 2964 |
| akumulacija max 2 okvira | 104 | 4.95 | 0.33 | 35.24 | 0.9465 | 36.78 | 33.99 | 0.508 | 3055 |
| akumulacija max 4 okvira | 104 | 4.93 | 0.34 | 35.21 | 0.9461 | 36.85 | 33.99 | 0.558 | 2947 |
| akumulacija max 8 okvira | 104 | 5.03 | 0.36 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.541 | 2812 |
| akumulacija max 16 okvira | 104 | 4.85 | 0.33 | 35.04 | 0.9442 | 37.02 | 33.99 | 0.508 | 3050 |
| akumulacija max 32 okvira | 104 | 5.03 | 0.34 | 34.90 | 0.9426 | 37.10 | 33.99 | 0.555 | 2956 |
| clamp 0.5 sigma | 104 | 5.03 | 0.34 | 34.59 | 0.9217 | 36.24 | 33.99 | 0.565 | 2955 |
| clamp 1 sigma | 104 | 4.90 | 0.33 | 35.30 | 0.9416 | 36.71 | 33.99 | 0.476 | 3069 |
| clamp 1.5 sigma | 104 | 4.95 | 0.34 | 35.23 | 0.9453 | 36.88 | 33.99 | 0.572 | 2958 |
| clamp 2 sigma | 104 | 5.07 | 0.35 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.557 | 2888 |
| clamp 4 sigma | 104 | 4.90 | 0.32 | 35.09 | 0.9452 | 36.95 | 33.99 | 0.329 | 3125 |
| rekonstrukcija 1/sigma^2 = 2 | 104 | 4.94 | 0.35 | 34.60 | 0.9248 | 37.97 | 33.99 | 0.575 | 2884 |
| rekonstrukcija 1/sigma^2 = 4 | 104 | 5.00 | 0.34 | 35.26 | 0.9433 | 37.08 | 33.99 | 0.512 | 2934 |
| rekonstrukcija 1/sigma^2 = 8 | 104 | 4.91 | 0.34 | 34.91 | 0.9444 | 37.03 | 33.99 | 0.547 | 2947 |
| rekonstrukcija 1/sigma^2 = 16 | 104 | 4.97 | 0.33 | 33.97 | 0.9346 | 37.50 | 33.99 | 0.551 | 2997 |
| skracivanje povijesti 0/px | 104 | 5.05 | 0.34 | 32.64 | 0.9073 | 38.58 | 33.99 | 0.503 | 2981 |
| skracivanje povijesti 0.4/px | 104 | 4.87 | 0.33 | 34.57 | 0.9390 | 37.55 | 33.99 | 0.488 | 3007 |
| skracivanje povijesti 0.8/px | 104 | 5.04 | 0.34 | 34.94 | 0.9432 | 37.19 | 33.99 | 0.544 | 2943 |
| skracivanje povijesti 1.6/px | 104 | 5.04 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.571 | 2926 |
| skracivanje povijesti 3.2/px | 104 | 4.91 | 0.33 | 35.23 | 0.9463 | 36.79 | 33.99 | 0.499 | 3039 |
| mip bias 0 | 104 | 4.98 | 0.31 | 34.57 | 0.9294 | 37.40 | 34.00 | 0.532 | 3223 |
| mip bias -0.585 | 104 | 5.04 | 0.31 | 35.09 | 0.9425 | 37.21 | 34.00 | 0.505 | 3259 |
| mip bias -1.585 | 104 | 4.86 | 0.33 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.508 | 3018 |
| TAAU + RCAS, ostrina 0.4 | 104 | 5.03 | 0.36 | 34.60 | 0.9440 | 34.58 | 33.99 | 0.535 | 2789 |
| TAAU + RCAS, ostrina 0.8 | 104 | 5.13 | 0.39 | 35.15 | 0.9504 | 35.50 | 33.99 | 0.612 | 2588 |
| TAAU + RCAS, ostrina 1.0 | 104 | 4.93 | 0.36 | 35.23 | 0.9509 | 35.78 | 33.99 | 0.616 | 2744 |
| TAAU + RCAS, ostrina 1.2 | 104 | 4.99 | 0.37 | 35.26 | 0.9508 | 35.99 | 33.99 | 0.539 | 2712 |
| TAAU + RCAS, ostrina 1.5 | 104 | 5.04 | 0.38 | 35.27 | 0.9502 | 36.22 | 33.99 | 0.603 | 2604 |
| TAAU + RCAS, ostrina 2.0 | 104 | 4.93 | 0.37 | 35.25 | 0.9490 | 36.47 | 33.99 | 0.565 | 2729 |

## TAAU: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| bicubic, mirna kamera | 104 | 4.77 | 0.25 | 35.83 | 0.9444 | 84.65 | 84.56 | 0.451 | 4049 |
| TAAU, mirna kamera | 104 | 5.04 | 0.34 | 39.41 | 0.9731 | 57.34 | 84.56 | 0.561 | 2981 |
| TAAU + RCAS, mirna kamera | 104 | 4.88 | 0.35 | 40.45 | 0.9825 | 55.16 | 84.56 | 0.553 | 2859 |
| bicubic, 120 fps | 104 | 4.97 | 0.25 | 37.19 | 0.9599 | 35.58 | 37.70 | 0.418 | 4040 |
| TAAU, 120 fps | 104 | 5.14 | 0.34 | 37.51 | 0.9646 | 41.23 | 37.61 | 0.547 | 2922 |
| TAAU + RCAS, 120 fps | 104 | 4.99 | 0.37 | 37.72 | 0.9692 | 40.03 | 37.61 | 0.559 | 2725 |
| bicubic, 60 fps | 104 | 4.81 | 0.26 | 35.38 | 0.9422 | 32.46 | 34.05 | 0.479 | 3910 |
| TAAU, 60 fps | 104 | 5.12 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.546 | 2906 |
| TAAU + RCAS, 60 fps | 104 | 4.90 | 0.37 | 35.26 | 0.9508 | 35.99 | 33.99 | 0.606 | 2728 |
| bicubic, 30 fps | 104 | 4.79 | 0.25 | 35.28 | 0.9394 | 30.00 | 30.81 | 0.463 | 3980 |
| TAAU, 30 fps | 104 | 5.03 | 0.33 | 34.36 | 0.9376 | 32.87 | 30.78 | 0.489 | 3009 |
| TAAU + RCAS, 30 fps | 104 | 4.95 | 0.36 | 34.40 | 0.9424 | 32.35 | 30.78 | 0.560 | 2799 |

## Puni upscaler (M5): dilatacija, depth clip, lockovi, Lanczos

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| Sponza: FSR, NativeAA 1.0x | 104 | 5.29 | 0.69 | 38.14 | 0.9744 | 34.86 | 34.18 | 0.865 | 1459 |
| Sponza: FSR + RCAS, NativeAA 1.0x | 104 | 5.44 | 0.74 | 37.59 | 0.9713 | 34.43 | 34.18 | 0.926 | 1345 |
| Sponza: FSR, Quality 1.5x | 104 | 4.99 | 0.45 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.632 | 2232 |
| Sponza: FSR + RCAS, Quality 1.5x | 104 | 5.03 | 0.47 | 35.02 | 0.9408 | 33.71 | 33.99 | 0.705 | 2144 |
| Sponza: FSR, Balanced 1.7x | 104 | 5.07 | 0.40 | 34.69 | 0.9335 | 34.00 | 33.98 | 0.567 | 2496 |
| Sponza: FSR + RCAS, Balanced 1.7x | 104 | 4.98 | 0.43 | 34.35 | 0.9294 | 33.59 | 33.98 | 0.642 | 2330 |
| Sponza: FSR, Performance 2.0x | 104 | 4.96 | 0.36 | 33.72 | 0.9144 | 33.78 | 33.95 | 0.569 | 2788 |
| Sponza: FSR + RCAS, Performance 2.0x | 104 | 5.13 | 0.46 | 33.47 | 0.9112 | 33.41 | 33.95 | 0.618 | 2160 |
| Sponza: FSR, Ultra Performance 3.0x | 104 | 4.87 | 0.32 | 31.40 | 0.8530 | 32.98 | 33.89 | 0.508 | 3167 |
| Sponza: FSR + RCAS, Ultra Performance 3.0x | 104 | 4.96 | 0.34 | 31.29 | 0.8512 | 32.73 | 33.89 | 0.530 | 2949 |
| Sponza 4K: FSR, Performance 2.0x | 104 | 14.74 | 1.28 | 36.10 | 0.9491 | 34.17 | 34.16 | 1.537 | 779 |
| Sponza 4K: FSR + RCAS, Performance 2.0x | 104 | 15.00 | 1.47 | 35.81 | 0.9464 | 33.90 | 34.16 | 2.021 | 681 |

## FSR: ablacije (Sponza, Quality 1.5x, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 5.02 | 0.44 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.668 | 2286 |
| bez dilatacije vektora | 104 | 5.04 | 0.44 | 35.28 | 0.9450 | 33.19 | 33.99 | 0.633 | 2296 |
| bez depth clipa (samo clamp boje) | 104 | 5.14 | 0.44 | 35.34 | 0.9457 | 34.38 | 33.99 | 0.599 | 2290 |
| bez lockova | 104 | 4.99 | 0.44 | 35.53 | 0.9454 | 34.11 | 33.99 | 0.659 | 2248 |
| lock siri clamp 1x | 104 | 5.08 | 0.46 | 35.49 | 0.9455 | 34.12 | 33.99 | 0.658 | 2191 |
| lock siri clamp 2x | 104 | 5.16 | 0.52 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.647 | 1929 |
| lock siri clamp 4x | 104 | 4.99 | 0.44 | 35.46 | 0.9458 | 34.15 | 33.99 | 0.685 | 2299 |
| lock siri clamp 8x | 104 | 5.03 | 0.43 | 35.46 | 0.9458 | 34.15 | 33.99 | 0.644 | 2304 |
| lock prag kontrasta 0.2 | 104 | 5.11 | 0.42 | 35.40 | 0.9454 | 34.17 | 33.99 | 0.596 | 2357 |
| lock prag kontrasta 0.35 | 104 | 5.02 | 0.45 | 35.43 | 0.9456 | 34.16 | 33.99 | 0.691 | 2233 |
| lock prag kontrasta 0.5 | 104 | 5.14 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.601 | 2311 |
| lock prag kontrasta 0.7 | 104 | 5.13 | 0.43 | 35.50 | 0.9458 | 34.12 | 33.99 | 0.645 | 2314 |
| lock traje 2 okvira | 104 | 5.02 | 0.44 | 35.49 | 0.9458 | 34.14 | 33.99 | 0.634 | 2286 |
| lock traje 4 okvira | 104 | 5.08 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.617 | 2308 |
| lock traje 8 okvira | 104 | 5.14 | 0.42 | 35.45 | 0.9456 | 34.15 | 33.99 | 0.607 | 2361 |
| Lanczos u gibanju 0.75 | 104 | 4.97 | 0.43 | 35.21 | 0.9408 | 34.40 | 33.99 | 0.635 | 2305 |
| Lanczos u gibanju 1.0 | 104 | 5.08 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.602 | 2346 |
| Lanczos u gibanju 1.25 | 104 | 5.16 | 0.44 | 35.46 | 0.9466 | 33.95 | 33.99 | 0.632 | 2287 |
| Lanczos u gibanju 1.5 | 104 | 4.98 | 0.43 | 35.38 | 0.9462 | 33.88 | 33.99 | 0.643 | 2341 |
| Lanczos na miru 1.0 | 104 | 5.02 | 0.43 | 35.45 | 0.9456 | 34.15 | 33.99 | 0.646 | 2310 |
| Lanczos na miru 1.5 | 104 | 5.18 | 0.44 | 35.46 | 0.9457 | 34.15 | 33.99 | 0.658 | 2290 |
| Lanczos na miru 2.0 | 104 | 5.00 | 0.45 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.694 | 2236 |
| Lanczos na miru 2.5 | 104 | 5.03 | 0.42 | 35.46 | 0.9458 | 34.14 | 33.99 | 0.602 | 2377 |
| tolerancija dubine 0.005 | 104 | 5.21 | 0.43 | 35.46 | 0.9457 | 34.13 | 33.99 | 0.632 | 2301 |
| tolerancija dubine 0.02 | 104 | 5.03 | 0.42 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.600 | 2353 |
| tolerancija dubine 0.1 | 104 | 5.05 | 0.44 | 35.47 | 0.9458 | 34.17 | 33.99 | 0.641 | 2262 |
| reactive maska 0 | 104 | 5.11 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.605 | 2344 |
| reactive maska 0.3 | 104 | 4.98 | 0.43 | 35.43 | 0.9456 | 34.11 | 33.99 | 0.621 | 2326 |
| reactive maska 1.0 | 104 | 5.08 | 0.44 | 35.22 | 0.9448 | 33.86 | 33.99 | 0.610 | 2281 |

## FSR: ablacije s mirnom kamerom (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| FSR, sve zadano | 104 | 5.09 | 0.43 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.652 | 2311 |
| bez dilatacije vektora | 104 | 4.89 | 0.40 | 37.57 | 0.9785 | 37.68 | 84.56 | 0.614 | 2483 |
| bez depth clipa (samo clamp boje) | 104 | 5.00 | 0.42 | 40.11 | 0.9831 | 53.08 | 84.56 | 0.642 | 2358 |
| bez lockova | 104 | 5.00 | 0.42 | 39.58 | 0.9767 | 51.30 | 84.56 | 0.650 | 2399 |
| lock siri clamp 1x | 104 | 4.95 | 0.44 | 39.88 | 0.9804 | 51.60 | 84.56 | 0.681 | 2291 |
| lock siri clamp 2x | 104 | 4.99 | 0.41 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.617 | 2423 |
| lock siri clamp 4x | 104 | 5.11 | 0.42 | 40.10 | 0.9836 | 51.79 | 84.56 | 0.628 | 2383 |
| lock siri clamp 8x | 104 | 4.93 | 0.42 | 40.11 | 0.9837 | 51.81 | 84.56 | 0.623 | 2390 |
| lock prag kontrasta 0.2 | 104 | 5.02 | 0.44 | 40.22 | 0.9845 | 52.25 | 84.56 | 0.666 | 2275 |
| lock prag kontrasta 0.35 | 104 | 5.10 | 0.48 | 40.15 | 0.9838 | 51.98 | 84.56 | 0.635 | 2088 |
| lock prag kontrasta 0.5 | 104 | 4.91 | 0.43 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.660 | 2324 |
| lock prag kontrasta 0.7 | 104 | 5.08 | 0.42 | 39.91 | 0.9809 | 51.49 | 84.56 | 0.622 | 2380 |
| Lanczos na miru 1.0 | 104 | 5.16 | 0.43 | 38.61 | 0.9720 | 52.03 | 84.56 | 0.650 | 2329 |
| Lanczos na miru 1.5 | 104 | 4.93 | 0.41 | 39.52 | 0.9795 | 51.56 | 84.56 | 0.618 | 2413 |
| Lanczos na miru 2.0 | 104 | 4.97 | 0.42 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.626 | 2401 |
| Lanczos na miru 2.5 | 104 | 5.06 | 0.42 | 38.92 | 0.9809 | 48.98 | 84.56 | 0.655 | 2387 |
| M4 TAAU, za usporedbu | 104 | 4.80 | 0.32 | 39.41 | 0.9731 | 57.34 | 84.56 | 0.547 | 3112 |

## FSR: kvaliteta u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR vs. native (dB) | SSIM vs. native | Stabilnost (dB) | Stabilnost ref. (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|
| TAAU (M4), mirna kamera | 104 | 4.89 | 0.33 | 39.41 | 0.9731 | 57.34 | 84.56 | 0.526 | 3045 |
| FSR (M5), mirna kamera | 104 | 5.08 | 0.42 | 40.07 | 0.9830 | 51.74 | 84.56 | 0.651 | 2372 |
| FSR + RCAS, mirna kamera | 104 | 4.97 | 0.44 | 39.72 | 0.9810 | 50.64 | 84.56 | 0.633 | 2264 |
| TAAU (M4), 120 fps | 104 | 5.10 | 0.35 | 37.51 | 0.9646 | 41.23 | 37.61 | 0.551 | 2861 |
| FSR (M5), 120 fps | 104 | 5.31 | 0.43 | 37.53 | 0.9636 | 38.11 | 37.61 | 0.570 | 2337 |
| FSR + RCAS, 120 fps | 104 | 5.11 | 0.45 | 37.08 | 0.9598 | 37.57 | 37.61 | 0.683 | 2216 |
| TAAU (M4), 60 fps | 104 | 4.95 | 0.34 | 35.14 | 0.9454 | 36.94 | 33.99 | 0.520 | 2949 |
| FSR (M5), 60 fps | 104 | 5.09 | 0.43 | 35.47 | 0.9458 | 34.14 | 33.99 | 0.605 | 2324 |
| FSR + RCAS, 60 fps | 104 | 5.00 | 0.46 | 35.02 | 0.9408 | 33.71 | 33.99 | 0.629 | 2178 |
| TAAU (M4), 30 fps | 104 | 4.92 | 0.35 | 34.36 | 0.9376 | 32.87 | 30.78 | 0.584 | 2883 |
| FSR (M5), 30 fps | 104 | 5.10 | 0.45 | 34.93 | 0.9384 | 30.86 | 30.78 | 0.645 | 2217 |
| FSR + RCAS, 30 fps | 104 | 4.99 | 0.47 | 34.50 | 0.9330 | 30.59 | 30.78 | 0.660 | 2137 |

## Optical flow (M6): tocnost po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 1.16 | 0.71 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.919 | 1413 |
| 1080p native (bez upscalinga) | 104 | 1.30 | 0.91 | 7.17 | 1.40 | 33.30 | 61.6 | 85.0 | 1.139 | 1094 |
| 1080p, render 960x540, FSR | 104 | 1.02 | 0.63 | 7.10 | 1.42 | 34.68 | 61.0 | 87.0 | 0.887 | 1576 |
| 1280x720, render 854x480, FSR | 104 | 0.79 | 0.40 | 4.43 | 1.13 | 20.04 | 65.0 | 88.5 | 0.640 | 2522 |
| 4K, render 1920x1080, FSR | 104 | 2.48 | 2.10 | 13.71 | 1.62 | 73.40 | 59.6 | 85.6 | 2.304 | 476 |

## Optical flow: ablacije i pretrage parametara (Sponza, 60 fps orbita)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 1.11 | 0.72 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.022 | 1383 |
| bez kandidata iz proslog okvira | 104 | 1.10 | 0.71 | 10.86 | 1.86 | 49.06 | 59.9 | 83.0 | 0.946 | 1403 |
| bez medijan filtra | 104 | 1.08 | 0.72 | 8.02 | 2.41 | 34.36 | 56.3 | 79.5 | 1.007 | 1383 |
| bez izbora kandidata pri prosirenju | 104 | 1.08 | 0.71 | 12.69 | 1.19 | 56.96 | 61.3 | 84.4 | 0.992 | 1418 |
| bez detekcije reza | 104 | 1.11 | 0.71 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.967 | 1399 |
| 3 razina piramide | 104 | 1.03 | 0.65 | 18.93 | 1.44 | 140.00 | 61.1 | 83.2 | 0.941 | 1528 |
| 4 razina piramide | 104 | 1.07 | 0.68 | 18.17 | 1.34 | 135.51 | 61.3 | 83.5 | 0.964 | 1462 |
| 5 razina piramide | 104 | 1.07 | 0.70 | 10.72 | 1.36 | 73.68 | 62.2 | 85.5 | 0.975 | 1429 |
| 6 razina piramide | 104 | 1.09 | 0.70 | 7.02 | 1.36 | 33.85 | 62.6 | 86.4 | 0.978 | 1436 |
| 7 razina piramide | 104 | 1.09 | 0.72 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.004 | 1388 |
| radijus pretrage 2 texela | 104 | 1.04 | 0.65 | 8.42 | 1.45 | 36.72 | 60.6 | 83.6 | 0.898 | 1535 |
| radijus pretrage 4 texela | 104 | 1.13 | 0.72 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 0.957 | 1393 |
| radijus pretrage 6 texela | 104 | 1.17 | 0.77 | 6.40 | 1.34 | 30.53 | 63.0 | 87.0 | 1.017 | 1291 |
| radijus pretrage 8 texela | 104 | 1.24 | 0.87 | 6.53 | 1.33 | 30.70 | 63.2 | 87.3 | 1.155 | 1153 |
| glatkoca 0/texel | 104 | 1.12 | 0.75 | 7.10 | 1.42 | 31.07 | 63.4 | 86.6 | 0.993 | 1328 |
| glatkoca 0.0002/texel | 104 | 1.12 | 0.74 | 6.70 | 1.37 | 31.18 | 63.5 | 87.0 | 1.014 | 1356 |
| glatkoca 0.0005/texel | 104 | 1.11 | 0.73 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.024 | 1375 |
| glatkoca 0.002/texel | 104 | 1.11 | 0.72 | 8.26 | 1.43 | 42.81 | 57.4 | 81.4 | 0.940 | 1383 |
| glatkoca 0.01/texel | 104 | 1.09 | 0.72 | 17.09 | 2.71 | 107.03 | 40.2 | 68.1 | 1.002 | 1395 |
| novelty 0 | 104 | 1.10 | 0.73 | 7.20 | 1.44 | 35.94 | 63.0 | 86.5 | 1.010 | 1374 |
| novelty 0.0005 | 104 | 1.14 | 0.75 | 7.20 | 1.41 | 34.23 | 62.4 | 86.1 | 1.042 | 1337 |
| novelty 0.001 | 104 | 1.14 | 0.76 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.025 | 1311 |
| novelty 0.004 | 104 | 1.12 | 0.74 | 8.18 | 1.25 | 34.63 | 61.9 | 85.5 | 1.019 | 1346 |
| novelty 0.01 | 104 | 1.10 | 0.73 | 10.85 | 1.21 | 45.00 | 61.7 | 85.2 | 1.010 | 1379 |
| novelty 1 | 104 | 1.10 | 0.73 | 11.76 | 1.23 | 47.66 | 61.5 | 84.9 | 1.012 | 1370 |
| rez: statistika maksimum | 104 | 1.13 | 0.75 | 17.28 | 1.36 | 129.04 | 58.4 | 80.2 | 1.047 | 1340 |
| rez: statistika srednja | 104 | 1.12 | 0.72 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.002 | 1384 |
| rez: statistika medijan | 104 | 1.11 | 0.74 | 7.20 | 1.36 | 33.30 | 62.3 | 85.9 | 1.011 | 1356 |

## Optical flow: tocnost u ovisnosti o brzini kamere (Sponza, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | EPE sred. (px) | EPE medijan (px) | EPE p95 (px) | unutar 1 px (%) | unutar 2 px (%) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 1.11 | 0.72 | 0.00 | 0.00 | 0.00 | 100.0 | 100.0 | 0.991 | 1390 |
| 120 fps | 120 | 1.09 | 0.72 | 2.85 | 1.44 | 7.63 | 65.6 | 88.9 | 1.014 | 1392 |
| 60 fps | 60 | 1.13 | 0.75 | 11.58 | 3.79 | 37.05 | 56.7 | 80.0 | 1.044 | 1339 |
| 30 fps | 30 | 1.14 | 0.75 | 36.62 | 9.07 | 109.81 | 47.3 | 70.1 | 0.998 | 1340 |

## Generiranje okvira (M7): kvaliteta po razlucivosti i cijena

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p, render 1280x720, FSR | 104 | 5.69 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.248 | 927 |
| 1080p native (FSR 1.0x) | 104 | 5.95 | 1.47 | 36.51 | 27.90 | 0.9630 | 26.12 | 0.6802 | 1.761 | 680 |
| 1080p, render 960x540, FSR | 104 | 5.45 | 0.92 | 33.54 | 27.34 | 0.9164 | 26.14 | 0.6806 | 1.130 | 1089 |
| 1280x720, render 854x480, FSR | 104 | 3.37 | 0.61 | 33.93 | 27.40 | 0.9287 | 26.64 | 0.6967 | 0.848 | 1636 |

## Generiranje okvira: ablacije (Sponza, 60 fps orbita, Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| Sve zadano | 104 | 5.57 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.266 | 923 |
| samo game vektori | 104 | 5.24 | 0.70 | 34.85 | 26.68 | 0.9449 | 26.09 | 0.6765 | 0.867 | 1427 |
| samo optical flow | 104 | 5.47 | 0.96 | 33.70 | 25.82 | 0.9252 | 26.09 | 0.6765 | 1.170 | 1045 |
| bez vektora (= blend) | 104 | 5.18 | 0.63 | 26.09 | 21.39 | 0.6765 | 26.09 | 0.6765 | 0.795 | 1599 |
| bez maski disokluzije | 104 | 5.53 | 1.00 | 34.93 | 27.42 | 0.9445 | 26.09 | 0.6765 | 1.228 | 997 |
| bez dilatiranih vektora | 104 | 5.59 | 1.10 | 34.98 | 27.65 | 0.9444 | 26.09 | 0.6765 | 1.364 | 913 |
| piramida: najblizi umjesto pozadine | 104 | 5.56 | 1.07 | 34.99 | 27.58 | 0.9442 | 26.09 | 0.6765 | 1.239 | 931 |
| s bojom u prioritetu scattera | 104 | 5.58 | 1.10 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.385 | 911 |
| tezina toka uz game vektor 1 | 104 | 5.54 | 1.07 | 34.83 | 27.61 | 0.9420 | 26.09 | 0.6765 | 1.243 | 931 |
| tezina toka uz game vektor 0.5 | 104 | 5.54 | 1.06 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.258 | 939 |
| tezina toka uz game vektor 0.25 | 104 | 5.56 | 1.08 | 35.04 | 27.42 | 0.9451 | 26.09 | 0.6765 | 1.248 | 927 |
| tezina toka uz game vektor 0.1 | 104 | 5.57 | 1.08 | 35.02 | 27.20 | 0.9453 | 26.09 | 0.6765 | 1.394 | 923 |
| 1 razina piramide polja | 104 | 5.50 | 0.99 | 34.70 | 27.95 | 0.9418 | 26.09 | 0.6765 | 1.193 | 1007 |
| 3 razina piramide polja | 104 | 5.52 | 1.03 | 34.83 | 27.65 | 0.9432 | 26.09 | 0.6765 | 1.220 | 967 |
| 5 razina piramide polja | 104 | 5.55 | 1.05 | 34.94 | 27.50 | 0.9439 | 26.09 | 0.6765 | 1.237 | 949 |
| 7 razina piramide polja | 104 | 5.56 | 1.07 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.241 | 938 |
| ostrina slaganja boja 0 | 104 | 5.58 | 1.08 | 34.88 | 27.74 | 0.9432 | 26.09 | 0.6765 | 1.237 | 929 |
| ostrina slaganja boja 6 | 104 | 5.58 | 1.08 | 34.97 | 27.70 | 0.9438 | 26.09 | 0.6765 | 1.243 | 926 |
| ostrina slaganja boja 24 | 104 | 5.53 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.314 | 925 |
| ostrina slaganja boja 96 | 104 | 5.51 | 1.07 | 35.01 | 27.61 | 0.9446 | 26.09 | 0.6765 | 1.239 | 939 |
| tolerancija dubine 0.005 | 104 | 5.58 | 1.10 | 34.98 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.369 | 910 |
| tolerancija dubine 0.02 | 104 | 5.55 | 1.08 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.246 | 930 |
| tolerancija dubine 0.08 | 104 | 5.54 | 1.08 | 35.01 | 27.55 | 0.9443 | 26.09 | 0.6765 | 1.258 | 923 |
| bez inpaintinga slike (prolazi 8-9) | 104 | 5.51 | 1.03 | 34.97 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.272 | 974 |
| bez odbacivanja uzoraka izvan ekrana | 104 | 5.53 | 1.05 | 34.96 | 27.65 | 0.9439 | 26.09 | 0.6765 | 1.244 | 949 |
| kao M7: bez inpaintinga i provjere granica | 104 | 5.57 | 0.99 | 34.94 | 27.56 | 0.9438 | 26.09 | 0.6765 | 1.197 | 1011 |
| prag pokrivenosti inpaintinga 0.1 | 104 | 5.55 | 1.09 | 34.99 | 27.54 | 0.9443 | 26.09 | 0.6765 | 1.256 | 921 |
| prag pokrivenosti inpaintinga 0.3 | 104 | 5.57 | 1.09 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.276 | 921 |
| prag pokrivenosti inpaintinga 0.6 | 104 | 5.57 | 1.07 | 34.98 | 27.60 | 0.9442 | 26.09 | 0.6765 | 1.249 | 934 |
| proceduralna scena: Sve zadano | 104 | 3.18 | 0.84 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 1.043 | 1186 |
| proceduralna scena: samo game vektori | 104 | 2.87 | 0.52 | 33.62 | 33.29 | 0.9674 | 31.56 | 0.9467 | 0.659 | 1934 |
| proceduralna scena: samo optical flow | 104 | 3.13 | 0.75 | 33.47 | 32.66 | 0.9661 | 31.56 | 0.9467 | 0.889 | 1333 |
| proceduralna scena: bez vektora (= blend) | 104 | 2.82 | 0.47 | 31.56 | 31.10 | 0.9467 | 31.56 | 0.9467 | 0.654 | 2120 |
| proceduralna scena: bez maski disokluzije | 104 | 3.17 | 0.79 | 34.01 | 33.67 | 0.9700 | 31.56 | 0.9467 | 0.941 | 1266 |
| proceduralna scena: piramida: najblizi umjesto pozadine | 104 | 3.25 | 0.83 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 1.027 | 1204 |
| proceduralna scena: s bojom u prioritetu scattera | 104 | 3.27 | 0.83 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 1.037 | 1207 |
| proceduralna scena: bez inpaintinga slike (prolazi 8-9) | 104 | 3.15 | 0.78 | 33.98 | 33.65 | 0.9698 | 31.56 | 0.9467 | 0.985 | 1278 |
| proceduralna scena: bez odbacivanja uzoraka izvan ekrana | 104 | 3.24 | 0.83 | 33.97 | 33.65 | 0.9698 | 31.56 | 0.9467 | 0.998 | 1207 |
| proceduralna scena: kao M7: bez inpaintinga i provjere granica | 104 | 3.16 | 0.79 | 33.98 | 33.65 | 0.9698 | 31.56 | 0.9467 | 0.982 | 1270 |
| proceduralna scena: tezina toka uz game vektor 1 | 104 | 3.22 | 0.82 | 33.98 | 33.65 | 0.9700 | 31.56 | 0.9467 | 0.993 | 1219 |
| proceduralna scena: tezina toka uz game vektor 0.5 | 104 | 3.19 | 0.83 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 1.026 | 1208 |
| proceduralna scena: tezina toka uz game vektor 0.25 | 104 | 3.20 | 0.84 | 33.93 | 33.63 | 0.9694 | 31.56 | 0.9467 | 1.034 | 1187 |
| proceduralna scena: tezina toka uz game vektor 0.1 | 104 | 3.21 | 0.83 | 33.88 | 33.57 | 0.9690 | 31.56 | 0.9467 | 1.034 | 1201 |

## Generiranje okvira: kvaliteta u ovisnosti o brzini kamere (Quality 1.5x)

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| mirna kamera | 60 | 5.52 | 1.03 | 40.15 | 40.04 | 0.9834 | 40.16 | 0.9834 | 1.316 | 971 |
| 120 fps | 120 | 5.55 | 1.07 | 35.94 | 31.20 | 0.9553 | 27.16 | 0.7479 | 1.260 | 934 |
| 60 fps | 60 | 5.59 | 1.07 | 34.63 | 27.49 | 0.9471 | 25.55 | 0.6918 | 1.371 | 937 |
| 30 fps | 30 | 5.59 | 1.04 | 32.84 | 24.11 | 0.9307 | 24.05 | 0.6453 | 1.162 | 959 |
| 30 fps, kao M7 | 30 | 5.87 | 1.02 | 32.79 | 23.98 | 0.9297 | 24.05 | 0.6453 | 1.208 | 984 |
| 20 fps | 20 | 5.69 | 1.07 | 30.96 | 20.22 | 0.9120 | 23.07 | 0.6177 | 1.451 | 932 |
| 20 fps, kao M7 | 20 | 5.60 | 0.96 | 31.09 | 20.32 | 0.9109 | 23.07 | 0.6177 | 1.379 | 1038 |

## UI kompozicija (M8): HUD nakon generiranja okvira ili upečen prije njega

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | PSNR HUD (dB) | SSIM HUD | PSNR HUD blend (dB) | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| HUD nakon generiranja (kompozicija) | 104 | 8.42 | 1.08 | 35.03 | 27.57 | 0.9448 | 26.17 | 0.6843 | 52.36 | 0.9980 | 40.06 | 1.318 | 925 |
| HUD upečen prije generiranja | 104 | 8.27 | 1.10 | 31.52 | 26.01 | 0.9395 | 26.17 | 0.6843 | 20.73 | 0.8739 | 40.06 | 1.288 | 907 |
| HUD upečen, 20 fps | 20 | 8.32 | 1.06 | 27.53 | 18.91 | 0.8998 | 23.15 | 0.6251 | 16.98 | 0.6961 | 36.70 | 1.284 | 947 |
| HUD nakon generiranja, 20 fps | 20 | 8.81 | 1.09 | 31.00 | 20.23 | 0.9138 | 23.15 | 0.6251 | 48.21 | 0.9882 | 36.70 | 2.149 | 913 |

## Naucena mjesavina (M9) naspram heuristike, putanje izvan skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1080p Quality: heuristika | 104 | 5.60 | 1.06 | 35.00 | 27.56 | 0.9442 | 26.09 | 0.6765 | 1.244 | 941 |
| 1080p Quality: naucena mjesavina | 104 | 6.47 | 2.12 | 35.09 | 27.57 | 0.9459 | 26.09 | 0.6765 | 2.404 | 472 |
| 1080p native: heuristika | 104 | 5.92 | 1.46 | 36.51 | 27.90 | 0.9630 | 26.12 | 0.6802 | 1.678 | 683 |
| 1080p native: naucena mjesavina | 104 | 6.83 | 2.55 | 36.69 | 27.15 | 0.9653 | 26.12 | 0.6802 | 2.855 | 392 |
| 1080p Performance: heuristika | 104 | 5.43 | 0.92 | 33.54 | 27.34 | 0.9164 | 26.14 | 0.6806 | 1.157 | 1089 |
| 1080p Performance: naucena mjesavina | 104 | 6.36 | 1.98 | 33.57 | 27.33 | 0.9174 | 26.14 | 0.6806 | 2.292 | 506 |
| 720p Quality: heuristika | 104 | 3.36 | 0.61 | 33.93 | 27.40 | 0.9287 | 26.64 | 0.6967 | 0.830 | 1646 |
| 720p Quality: naucena mjesavina | 104 | 3.92 | 1.18 | 33.97 | 27.62 | 0.9302 | 26.64 | 0.6967 | 1.392 | 848 |
| proceduralna scena: heuristika | 104 | 3.33 | 0.82 | 33.97 | 33.66 | 0.9698 | 31.56 | 0.9467 | 0.993 | 1215 |
| proceduralna scena: naucena mjesavina | 104 | 4.34 | 1.84 | 34.15 | 33.76 | 0.9711 | 31.56 | 0.9467 | 2.148 | 543 |
| Quality, 120 fps: heuristika | 120 | 5.59 | 1.08 | 35.94 | 31.20 | 0.9553 | 27.16 | 0.7479 | 1.349 | 928 |
| Quality, 120 fps: naucena mjesavina | 120 | 6.50 | 2.10 | 36.02 | 30.26 | 0.9568 | 27.16 | 0.7479 | 2.462 | 476 |
| Quality, 30 fps: heuristika | 30 | 5.67 | 1.04 | 32.84 | 24.11 | 0.9307 | 24.05 | 0.6453 | 1.168 | 966 |
| Quality, 30 fps: naucena mjesavina | 30 | 6.87 | 2.20 | 32.81 | 23.70 | 0.9334 | 24.05 | 0.6453 | 2.528 | 454 |
| Quality, 20 fps: heuristika | 20 | 5.68 | 1.08 | 30.96 | 20.22 | 0.9120 | 23.07 | 0.6177 | 1.178 | 929 |
| Quality, 20 fps: naucena mjesavina | 20 | 6.65 | 2.19 | 31.09 | 20.72 | 0.9176 | 23.07 | 0.6177 | 2.443 | 456 |

## Naucena mjesavina: velicina mreze i sastav skupa za ucenje

| Konfiguracija | Frameovi | CPU ms | GPU ms | PSNR interp. (dB) | PSNR interp. min (dB) | SSIM interp. | PSNR blend (dB) | SSIM blend | GPU p95 (ms) | FPS (GPU) |
|---|---|---|---|---|---|---|---|---|---|---|
| blend-c12-c24, Sponza Quality | 104 | 7.55 | 3.21 | 35.17 | 28.92 | 0.9463 | 26.09 | 0.6765 | 3.464 | 311 |
| blend-c12-c24, proceduralna scena | 104 | 5.27 | 3.00 | 34.24 | 33.83 | 0.9718 | 31.56 | 0.9467 | 3.247 | 333 |
| blend-c4-c8, Sponza Quality | 104 | 5.94 | 1.52 | 34.91 | 25.26 | 0.9456 | 26.09 | 0.6765 | 1.727 | 658 |
| blend-c4-c8, proceduralna scena | 104 | 3.65 | 1.25 | 34.04 | 33.63 | 0.9704 | 31.56 | 0.9467 | 1.470 | 800 |
| blend-c8-c16-s2, Sponza Quality | 104 | 6.53 | 2.15 | 35.19 | 25.98 | 0.9470 | 26.09 | 0.6765 | 2.412 | 464 |
| blend-c8-c16-s2, proceduralna scena | 104 | 4.31 | 1.86 | 34.16 | 33.76 | 0.9713 | 31.56 | 0.9467 | 2.109 | 539 |
| blend-c8-c16-s3, Sponza Quality | 104 | 6.55 | 2.14 | 35.12 | 25.68 | 0.9467 | 26.09 | 0.6765 | 2.406 | 467 |
| blend-c8-c16-s3, proceduralna scena | 104 | 4.25 | 1.82 | 34.27 | 33.86 | 0.9719 | 31.56 | 0.9467 | 1.992 | 549 |
| blend-c8-c16-sponza, Sponza Quality | 104 | 6.47 | 2.16 | 35.05 | 25.99 | 0.9460 | 26.09 | 0.6765 | 2.432 | 463 |
| blend-c8-c16-sponza, proceduralna scena | 104 | 4.24 | 1.81 | 33.95 | 33.43 | 0.9697 | 31.56 | 0.9467 | 2.012 | 553 |
| blend-c8-c16, Sponza Quality | 104 | 6.58 | 2.15 | 35.09 | 27.57 | 0.9459 | 26.09 | 0.6765 | 2.464 | 464 |
| blend-c8-c16, proceduralna scena | 104 | 4.28 | 1.84 | 34.15 | 33.76 | 0.9711 | 31.56 | 0.9467 | 2.055 | 545 |
