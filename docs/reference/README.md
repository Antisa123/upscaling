# Referentna dokumentacija (nije u repozitoriju)

Ovaj direktorij lokalno drži preuzetu AMD FidelityFX dokumentaciju koja se
čita uz implementaciju. Nije naš tekst, pa se ne redistribuira ovdje — sadržaj
direktorija je u `.gitignore`, ostaje samo ovaj pokazivač na izvornik.

Datoteke koje docs/*.md ponegdje citiraju, iz
[GPUOpen-LibrariesAndSDKs/FidelityFX-SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK)
(`docs/techniques/`):

| Lokalno ime | Izvornik |
|---|---|
| `super-resolution-upscaler.md` | `super-resolution-upscaler.md` (FSR3 upscaler) |
| `super-resolution-interpolation.md` | `super-resolution-interpolation.md` (FSR3 interpolacija) |
| `super-resolution-temporal.md` | `super-resolution-temporal.md` (FSR2) |
| `optical-flow.md` | `optical-flow.md` |
| `frame-interpolation.md` | `frame-interpolation.md` |

Iste stranice stoje i na [GPUOpen](https://gpuopen.com/fidelityfx-superresolution-3/).

Kako je zapisano u `PLAN.md`: ti su passevi pseudokod koji se čita, a ne kod
koji se kopira — implementacija u `src/` i `shaders/` je vlastita.
