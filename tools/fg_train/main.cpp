// M9: trainer for the learned frame generation blend (src/ml/blend_net.h).
//
// Written from scratch instead of in a framework: the RX 580 has no ROCm, the
// project carries no Python dependencies beyond Pillow, and the model is small
// enough that forward and backward passes of its five operations are a few
// hundred lines of loops the compiler vectorises. It trains on the CPU in
// minutes. Owning both sides also makes them checkable against each other:
// `eval` on a capture recorded with --fg-ml compares this file's forward pass
// with the shader's output, pixel by pixel.
//
//   fg_train train --data a.bin,b.bin --val c.bin --out weights.bin
//                  [--c0 8] [--c1 16] [--steps 12000] [--batch 16] [--lr 0.002]
//                  [--margin 16] [--augment 1] [--seed 1] [--eval-every 1000]
//                  [--log train.csv] [--loss charbonnier|mse]
//   fg_train eval --weights weights.bin --data c.bin [--margin 16]
//   fg_train gradcheck

#include "ml/blend_net.h"

#include <omp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ml::BlendNet;
using ml::ConvLayer;

constexpr int K = ml::kCandidates;
constexpr int F = ml::kFeatures;
constexpr int kBaseChannels = F + 3 * K + 3;
constexpr double kCharbonnierEps2 = 1e-6;
// --loss mse trains on squared error, which is what PSNR scores; the default
// Charbonnier is a smooth L1, which forgives the rare large error PSNR punishes.
bool gMseLoss = false;

float gHalf[65536];

// ---------------------------------------------------------------------------
// Tensors and the five operations

struct Tensor {
    int c = 0, h = 0, w = 0;
    std::vector<float> v;

    void resize(int channels, int height, int width) {
        c = channels;
        h = height;
        w = width;
        v.resize(static_cast<size_t>(c) * h * w);
    }
    void zero() { std::fill(v.begin(), v.end(), 0.f); }
    void zeroLike(const Tensor& t) {
        resize(t.c, t.h, t.w);
        zero();
    }
    size_t plane() const { return static_cast<size_t>(h) * w; }
    float* ch(int i) { return v.data() + i * plane(); }
    const float* ch(int i) const { return v.data() + i * plane(); }
};

void relu(Tensor& t) {
    for (float& x : t.v) x = x > 0.f ? x : 0.f;
}

// `out` is the activation after the ReLU; out > 0 is exactly where it passed.
void reluBackward(const Tensor& out, Tensor& grad) {
    for (size_t i = 0; i < grad.v.size(); ++i)
        if (out.v[i] <= 0.f) grad.v[i] = 0.f;
}

// Edge replication by one texel: what clamping the tap coordinate does.
void padReplicate(const Tensor& in, Tensor& out) {
    out.resize(in.c, in.h + 2, in.w + 2);
    const int pw = in.w + 2;
    for (int c = 0; c < in.c; ++c) {
        const float* src = in.ch(c);
        float* dst = out.ch(c);
        for (int y = 0; y < in.h + 2; ++y) {
            const float* s = src + std::clamp(y - 1, 0, in.h - 1) * in.w;
            float* d = dst + y * pw;
            d[0] = s[0];
            std::copy(s, s + in.w, d + 1);
            d[in.w + 1] = s[in.w - 1];
        }
    }
}

void padReplicateBackward(const Tensor& dpad, Tensor& din) {
    const int pw = din.w + 2;
    for (int c = 0; c < din.c; ++c) {
        const float* g = dpad.ch(c);
        float* d = din.ch(c);
        for (int y = 0; y < din.h + 2; ++y) {
            float* row = d + std::clamp(y - 1, 0, din.h - 1) * din.w;
            const float* gr = g + y * pw;
            row[0] += gr[0];
            for (int x = 0; x < din.w; ++x) row[x] += gr[x + 1];
            row[din.w - 1] += gr[din.w + 1];
        }
    }
}

void convForward(const ConvLayer& L, const Tensor& in, Tensor& padded, Tensor& out) {
    out.resize(L.cout, in.h, in.w);
    const size_t n = in.plane();
    if (L.k == 1) {
        for (int o = 0; o < L.cout; ++o) {
            float* d = out.ch(o);
            std::fill(d, d + n, L.b[o]);
            for (int i = 0; i < L.cin; ++i) {
                const float wv = L.w[L.index(o, i, 0)];
                const float* s = in.ch(i);
                for (size_t p = 0; p < n; ++p) d[p] += wv * s[p];
            }
        }
        return;
    }
    padReplicate(in, padded);
    const int W = in.w, H = in.h, PW = W + 2;
    for (int o = 0; o < L.cout; ++o) {
        float* d = out.ch(o);
        std::fill(d, d + n, L.b[o]);
        for (int i = 0; i < L.cin; ++i) {
            const float* k = &L.w[L.index(o, i, 0)];
            const float* s = padded.ch(i);
            for (int y = 0; y < H; ++y) {
                const float* r0 = s + y * PW;
                const float* r1 = r0 + PW;
                const float* r2 = r1 + PW;
                float* dr = d + y * W;
                for (int x = 0; x < W; ++x)
                    dr[x] += k[0] * r0[x] + k[1] * r0[x + 1] + k[2] * r0[x + 2] + k[3] * r1[x] +
                             k[4] * r1[x + 1] + k[5] * r1[x + 2] + k[6] * r2[x] + k[7] * r2[x + 1] +
                             k[8] * r2[x + 2];
            }
        }
    }
}

// Accumulates into dw, db and (if given) din.
void convBackward(const ConvLayer& L, const Tensor& in, const Tensor& padded, const Tensor& dout,
                  float* dw, float* db, Tensor* din, Tensor& dpad) {
    const int H = dout.h, W = dout.w;
    const size_t n = dout.plane();
    for (int o = 0; o < L.cout; ++o) {
        const float* g = dout.ch(o);
        double sum = 0.0;
        for (size_t p = 0; p < n; ++p) sum += g[p];
        db[o] += static_cast<float>(sum);
    }
    if (L.k == 1) {
        for (int o = 0; o < L.cout; ++o) {
            const float* g = dout.ch(o);
            for (int i = 0; i < L.cin; ++i) {
                const float* s = in.ch(i);
                double sum = 0.0;
                for (size_t p = 0; p < n; ++p) sum += g[p] * s[p];
                dw[L.index(o, i, 0)] += static_cast<float>(sum);
            }
        }
        if (din) {
            for (int i = 0; i < L.cin; ++i) {
                float* d = din->ch(i);
                for (int o = 0; o < L.cout; ++o) {
                    const float wv = L.w[L.index(o, i, 0)];
                    const float* g = dout.ch(o);
                    for (size_t p = 0; p < n; ++p) d[p] += wv * g[p];
                }
            }
        }
        return;
    }
    const int PW = W + 2;
    for (int o = 0; o < L.cout; ++o) {
        const float* g = dout.ch(o);
        for (int i = 0; i < L.cin; ++i) {
            const float* s = padded.ch(i);
            for (int t = 0; t < 9; ++t) {
                const int dy = t / 3, dx = t % 3;
                double sum = 0.0;
                for (int y = 0; y < H; ++y) {
                    const float* row = s + (y + dy) * PW + dx;
                    const float* gr = g + y * W;
                    float acc = 0.f;
                    for (int x = 0; x < W; ++x) acc += gr[x] * row[x];
                    sum += acc;
                }
                dw[L.index(o, i, t)] += static_cast<float>(sum);
            }
        }
    }
    if (!din) return;
    dpad.resize(din->c, H + 2, W + 2);
    dpad.zero();
    for (int i = 0; i < L.cin; ++i) {
        float* dp = dpad.ch(i);
        for (int o = 0; o < L.cout; ++o) {
            const float* k = &L.w[L.index(o, i, 0)];
            const float* g = dout.ch(o);
            for (int t = 0; t < 9; ++t) {
                const int dy = t / 3, dx = t % 3;
                const float kv = k[t];
                for (int y = 0; y < H; ++y) {
                    float* dr = dp + (y + dy) * PW + dx;
                    const float* gr = g + y * W;
                    for (int x = 0; x < W; ++x) dr[x] += kv * gr[x];
                }
            }
        }
    }
    padReplicateBackward(dpad, *din);
}

void poolForward(const Tensor& in, Tensor& out) {
    out.resize(in.c, in.h / 2, in.w / 2);
    for (int c = 0; c < in.c; ++c) {
        const float* s = in.ch(c);
        float* d = out.ch(c);
        for (int y = 0; y < out.h; ++y) {
            const float* r0 = s + 2 * y * in.w;
            const float* r1 = r0 + in.w;
            for (int x = 0; x < out.w; ++x)
                d[y * out.w + x] = 0.25f * (r0[2 * x] + r0[2 * x + 1] + r1[2 * x] + r1[2 * x + 1]);
        }
    }
}

void poolBackward(const Tensor& dout, Tensor& din) {
    for (int c = 0; c < dout.c; ++c) {
        const float* g = dout.ch(c);
        float* d = din.ch(c);
        for (int y = 0; y < dout.h; ++y)
            for (int x = 0; x < dout.w; ++x) {
                const float q = 0.25f * g[y * dout.w + x];
                const size_t base = static_cast<size_t>(2 * y) * din.w + 2 * x;
                d[base] += q;
                d[base + 1] += q;
                d[base + din.w] += q;
                d[base + din.w + 1] += q;
            }
    }
}

// Bilinear 2x upsample with half-texel centres and clamped edges: output
// sample 2j sits at j - 0.25 in input texels, 2j + 1 at j + 0.25.
struct Tap2 {
    int a, b;
    float wa, wb;
};

void upTaps(int n, std::vector<Tap2>& taps) {
    taps.resize(2 * static_cast<size_t>(n));
    for (int j = 0; j < n; ++j) {
        taps[2 * j] = {std::max(j - 1, 0), j, 0.25f, 0.75f};
        taps[2 * j + 1] = {j, std::min(j + 1, n - 1), 0.75f, 0.25f};
    }
}

struct UpScratch {
    Tensor tmp;
    std::vector<Tap2> ty, tx;
};

void upForward(const Tensor& in, UpScratch& s, Tensor& out) {
    upTaps(in.h, s.ty);
    upTaps(in.w, s.tx);
    const int H2 = 2 * in.h, W2 = 2 * in.w;
    s.tmp.resize(in.c, H2, in.w);
    out.resize(in.c, H2, W2);
    for (int c = 0; c < in.c; ++c) {
        const float* src = in.ch(c);
        float* t = s.tmp.ch(c);
        for (int Y = 0; Y < H2; ++Y) {
            const Tap2& ty = s.ty[Y];
            const float* ra = src + ty.a * in.w;
            const float* rb = src + ty.b * in.w;
            float* tr = t + Y * in.w;
            for (int x = 0; x < in.w; ++x) tr[x] = ty.wa * ra[x] + ty.wb * rb[x];
        }
        float* d = out.ch(c);
        for (int Y = 0; Y < H2; ++Y) {
            const float* tr = t + Y * in.w;
            float* dr = d + Y * W2;
            for (int X = 0; X < W2; ++X) {
                const Tap2& tx = s.tx[X];
                dr[X] = tx.wa * tr[tx.a] + tx.wb * tr[tx.b];
            }
        }
    }
}

// Accumulates into din (input shape). Taps are the ones upForward left in `s`.
void upBackward(const Tensor& dout, UpScratch& s, Tensor& din) {
    const int H2 = dout.h, W2 = dout.w, w = din.w;
    s.tmp.resize(dout.c, H2, w);
    s.tmp.zero();
    for (int c = 0; c < dout.c; ++c) {
        const float* g = dout.ch(c);
        float* t = s.tmp.ch(c);
        for (int Y = 0; Y < H2; ++Y) {
            const float* gr = g + Y * W2;
            float* tr = t + Y * w;
            for (int X = 0; X < W2; ++X) {
                const Tap2& tx = s.tx[X];
                tr[tx.a] += tx.wa * gr[X];
                tr[tx.b] += tx.wb * gr[X];
            }
        }
        float* d = din.ch(c);
        for (int Y = 0; Y < H2; ++Y) {
            const Tap2& ty = s.ty[Y];
            const float* tr = t + Y * w;
            float* ra = d + ty.a * w;
            float* rb = d + ty.b * w;
            for (int x = 0; x < w; ++x) {
                ra[x] += ty.wa * tr[x];
                rb[x] += ty.wb * tr[x];
            }
        }
    }
}

void addInto(Tensor& dst, const Tensor& src) {
    for (size_t i = 0; i < dst.v.size(); ++i) dst.v[i] += src.v[i];
}

// ---------------------------------------------------------------------------
// The network

struct Grads {
    std::array<std::vector<float>, ml::kLayerCount> w, b;

    void shape(const BlendNet& net) {
        for (int l = 0; l < ml::kLayerCount; ++l) {
            w[l].assign(net.layers[l].w.size(), 0.f);
            b[l].assign(net.layers[l].b.size(), 0.f);
        }
    }
    void zero() {
        for (int l = 0; l < ml::kLayerCount; ++l) {
            std::fill(w[l].begin(), w[l].end(), 0.f);
            std::fill(b[l].begin(), b[l].end(), 0.f);
        }
    }
};

struct Work {
    // Inputs: normalised features, candidates (K x rgb), reference, and the
    // engine's output when the capture has it.
    Tensor x, cand, gt, engine;
    std::vector<uint8_t> mask;  // pixels the loss counts, see loadSample
    // Forward.
    Tensor pad, x0, p0, x1, p1, x2, x3, s1, u1, s0, z, weights, y;
    Tensor pad1, pad2, pad3, pad4;
    UpScratch up1, up0;
    // Backward.
    Tensor dy, dz, ds0, du1, ds1, dx3, dx2, dp1, dx1, dp0, dx0, dpad;
    Grads grads;
};

void forward(const BlendNet& net, Work& k) {
    const auto& L = net.layers;
    convForward(L[ml::kEnc0], k.x, k.pad, k.x0);
    relu(k.x0);
    poolForward(k.x0, k.p0);
    convForward(L[ml::kEnc1], k.p0, k.pad1, k.x1);
    relu(k.x1);
    poolForward(k.x1, k.p1);
    convForward(L[ml::kEnc2], k.p1, k.pad2, k.x2);
    relu(k.x2);
    convForward(L[ml::kEnc3], k.x2, k.pad3, k.x3);
    relu(k.x3);
    upForward(k.x3, k.up1, k.s1);
    addInto(k.s1, k.x1);
    convForward(L[ml::kDec1], k.s1, k.pad4, k.u1);
    relu(k.u1);
    upForward(k.u1, k.up0, k.s0);
    addInto(k.s0, k.x0);
    convForward(L[ml::kDec0], k.s0, k.pad, k.z);

    const size_t n = k.z.plane();
    k.weights.resize(K, k.z.h, k.z.w);
    k.y.resize(3, k.z.h, k.z.w);
    for (size_t p = 0; p < n; ++p) {
        float top = k.z.v[p];
        for (int j = 1; j < K; ++j) top = std::max(top, k.z.v[j * n + p]);
        float total = 0.f;
        for (int j = 0; j < K; ++j) {
            const float e = std::exp(k.z.v[j * n + p] - top);
            k.weights.v[j * n + p] = e;
            total += e;
        }
        float rgb[3] = {0.f, 0.f, 0.f};
        for (int j = 0; j < K; ++j) {
            const float wj = (k.weights.v[j * n + p] /= total);
            for (int c = 0; c < 3; ++c) rgb[c] += wj * k.cand.v[(j * 3 + c) * n + p];
        }
        for (int c = 0; c < 3; ++c) k.y.v[c * n + p] = rgb[c];
    }
}

void backward(const BlendNet& net, Work& k) {
    const auto& L = net.layers;
    Grads& g = k.grads;
    const size_t n = k.z.plane();
    k.dz.resize(K, k.z.h, k.z.w);
    for (size_t p = 0; p < n; ++p)
        for (int j = 0; j < K; ++j) {
            float dot = 0.f;
            for (int c = 0; c < 3; ++c)
                dot += k.dy.v[c * n + p] * (k.cand.v[(j * 3 + c) * n + p] - k.y.v[c * n + p]);
            k.dz.v[j * n + p] = k.weights.v[j * n + p] * dot;
        }

    k.ds0.zeroLike(k.s0);
    convBackward(L[ml::kDec0], k.s0, k.pad, k.dz, g.w[ml::kDec0].data(), g.b[ml::kDec0].data(), &k.ds0,
                 k.dpad);
    // s0 = up(u1) + x0
    k.dx0 = k.ds0;
    k.du1.zeroLike(k.u1);
    upBackward(k.ds0, k.up0, k.du1);
    reluBackward(k.u1, k.du1);
    k.ds1.zeroLike(k.s1);
    convBackward(L[ml::kDec1], k.s1, k.pad4, k.du1, g.w[ml::kDec1].data(), g.b[ml::kDec1].data(), &k.ds1,
                 k.dpad);
    // s1 = up(x3) + x1
    k.dx1 = k.ds1;
    k.dx3.zeroLike(k.x3);
    upBackward(k.ds1, k.up1, k.dx3);
    reluBackward(k.x3, k.dx3);
    k.dx2.zeroLike(k.x2);
    convBackward(L[ml::kEnc3], k.x2, k.pad3, k.dx3, g.w[ml::kEnc3].data(), g.b[ml::kEnc3].data(), &k.dx2,
                 k.dpad);
    reluBackward(k.x2, k.dx2);
    k.dp1.zeroLike(k.p1);
    convBackward(L[ml::kEnc2], k.p1, k.pad2, k.dx2, g.w[ml::kEnc2].data(), g.b[ml::kEnc2].data(), &k.dp1,
                 k.dpad);
    poolBackward(k.dp1, k.dx1);
    reluBackward(k.x1, k.dx1);
    k.dp0.zeroLike(k.p0);
    convBackward(L[ml::kEnc1], k.p0, k.pad1, k.dx1, g.w[ml::kEnc1].data(), g.b[ml::kEnc1].data(), &k.dp0,
                 k.dpad);
    poolBackward(k.dp0, k.dx0);
    reluBackward(k.x0, k.dx0);
    convBackward(L[ml::kEnc0], k.x, k.pad, k.dx0, g.w[ml::kEnc0].data(), g.b[ml::kEnc0].data(), nullptr,
                 k.dpad);
}

struct Errors {
    double loss = 0.0;      // Charbonnier, summed
    double sqNet = 0.0;     // squared error, summed
    double sqHeur = 0.0;    // candidate 0
    double sqOracle = 0.0;  // best single candidate per pixel
    double engineAbs = 0.0; // |this forward - shader|
    double engineMax = 0.0;
    double count = 0.0;     // channel samples

    void add(const Errors& e) {
        loss += e.loss;
        sqNet += e.sqNet;
        sqHeur += e.sqHeur;
        sqOracle += e.sqOracle;
        engineAbs += e.engineAbs;
        engineMax = std::max(engineMax, e.engineMax);
        count += e.count;
    }
};

// Loss over the pixels of k.mask. The gradient is the mean over this sample's
// counted channels divided by `batch`, so a batch sums to the batch mean.
Errors lossAndGradient(Work& k, double batch, bool gradient, bool detail) {
    const size_t n = k.y.plane();
    Errors e;
    if (gradient) k.dy.zeroLike(k.y);
    size_t counted = 0;
    for (uint8_t m : k.mask) counted += m;
    const double normaliser = std::max(static_cast<double>(counted) * 3.0, 1.0) * batch;
    for (size_t p = 0; p < n; ++p)
        if (k.mask[p]) {
            double best = 1e30;
            if (detail) {
                for (int j = 0; j < K; ++j) {
                    double s = 0.0;
                    for (int c = 0; c < 3; ++c) {
                        const double d = k.cand.v[(j * 3 + c) * n + p] - k.gt.v[c * n + p];
                        s += d * d;
                    }
                    best = std::min(best, s);
                }
                e.sqOracle += best;
            }
            for (int c = 0; c < 3; ++c) {
                const double d = k.y.v[c * n + p] - k.gt.v[c * n + p];
                e.sqNet += d * d;
                if (gMseLoss) {
                    e.loss += d * d;
                    if (gradient) k.dy.v[c * n + p] = static_cast<float>(2.0 * d / normaliser);
                } else {
                    const double r = std::sqrt(d * d + kCharbonnierEps2);
                    e.loss += r;
                    if (gradient) k.dy.v[c * n + p] = static_cast<float>(d / r / normaliser);
                }
                if (detail) {
                    const double dh = k.cand.v[c * n + p] - k.gt.v[c * n + p];
                    e.sqHeur += dh * dh;
                    if (!k.engine.v.empty()) {
                        const double de = std::abs(k.y.v[c * n + p] - k.engine.v[c * n + p]);
                        e.engineAbs += de;
                        e.engineMax = std::max(e.engineMax, de);
                    }
                }
                e.count += 1.0;
            }
        }
    return e;
}

// ---------------------------------------------------------------------------
// Data

struct Dataset {
    int patch = 0;
    int channels = 0;
    int extra = 0;
    std::vector<uint16_t> data;
    std::vector<ml::PatchRecord> records;
    std::vector<int> file;
    std::vector<std::string> names;
    std::vector<std::pair<int, int>> frameSize;  // per file

    size_t stride() const { return static_cast<size_t>(patch) * patch * channels; }
    const uint16_t* sample(size_t i) const { return data.data() + i * stride(); }

    bool append(const std::string& path) {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            std::fprintf(stderr, "cannot open %s\n", path.c_str());
            return false;
        }
        ml::DatasetHeader h;
        if (std::fread(&h, sizeof(h), 1, f) != 1 || h.magic != ml::kDatasetMagic ||
            h.version != ml::kDatasetVersion || h.features != F || h.candidates != K) {
            std::fprintf(stderr, "%s: not a dataset of this version\n", path.c_str());
            std::fclose(f);
            return false;
        }
        const int c = ml::recordChannels(h);
        if (patch == 0) {
            patch = h.patch;
            channels = c;
            extra = h.extra;
        } else if (patch != h.patch || channels != c) {
            std::fprintf(stderr, "%s: patch size or channels differ from the first file\n", path.c_str());
            std::fclose(f);
            return false;
        }
        const size_t before = records.size();
        size_t dropped = 0;
        ml::PatchRecord rec;
        while (std::fread(&rec, sizeof(rec), 1, f) == 1) {
            const size_t offset = data.size();
            data.resize(offset + stride());
            if (std::fread(data.data() + offset, sizeof(uint16_t), stride(), f) != stride()) {
                data.resize(offset);
                break;
            }
            // A non-finite value anywhere (half exponent all ones) would turn
            // every gradient of the batch into NaN.
            const uint16_t* begin = data.data() + offset;
            if (std::any_of(begin, begin + stride(), [](uint16_t h) { return (h & 0x7C00) == 0x7C00; })) {
                data.resize(offset);
                ++dropped;
                continue;
            }
            records.push_back(rec);
            file.push_back(static_cast<int>(names.size()));
        }
        std::fclose(f);
        names.push_back(path);
        frameSize.push_back({h.frameWidth, h.frameHeight});
        std::printf("  %s: %zu patches%s\n", path.c_str(), records.size() - before,
                    dropped ? (" (" + std::to_string(dropped) + " with NaN/inf dropped)").c_str() : "");
        return true;
    }
};

bool loadAll(const std::string& list, Dataset& ds) {
    std::stringstream ss(list);
    std::string item;
    bool any = false;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        if (!ds.append(item)) return false;
        any = true;
    }
    return any && !ds.records.empty();
}

// Dihedral augmentation: bit 0 flips x, bit 1 flips y, bit 2 transposes. Every
// feature is a magnitude or a flag, so all eight are equally valid frames, and
// with a patch size divisible by 4 the pooling grid maps onto itself.
//
// Within `margin` of a patch edge the network sees replicated padding where
// the frame had real context, so the loss does not count those pixels -- except
// on a side that is the frame's own edge, where the shader pads exactly the
// same way. The first models were trained without that exception and lost 4 dB
// on a validation path, all of it in the outermost 20-45 px of the frame.
void loadSample(const Dataset& ds, size_t index, int aug, const BlendNet& net, int margin, Work& k) {
    const int P = ds.patch;
    const ml::PatchRecord& rec = ds.records[index];
    const auto [frameW, frameH] = ds.frameSize[ds.file[index]];
    const int left = rec.x == 0 ? 0 : margin;
    const int right = rec.x + P >= frameW ? 0 : margin;
    const int bottom = rec.y == 0 ? 0 : margin;
    const int top = rec.y + P >= frameH ? 0 : margin;
    k.mask.resize(static_cast<size_t>(P) * P);
    const int C = ds.channels;
    const uint16_t* base = ds.sample(index);
    k.x.resize(F, P, P);
    k.cand.resize(3 * K, P, P);
    k.gt.resize(3, P, P);
    if (ds.extra == 3)
        k.engine.resize(3, P, P);
    else
        k.engine.v.clear();
    const size_t n = static_cast<size_t>(P) * P;
    for (int y = 0; y < P; ++y)
        for (int x = 0; x < P; ++x) {
            int sy = y, sx = x;
            if (aug & 4) std::swap(sy, sx);
            if (aug & 1) sx = P - 1 - sx;
            if (aug & 2) sy = P - 1 - sy;
            const uint16_t* px = base + (static_cast<size_t>(sy) * P + sx) * C;
            const size_t p = static_cast<size_t>(y) * P + x;
            k.mask[p] = sx >= left && sx < P - right && sy >= bottom && sy < P - top;
            for (int f = 0; f < F; ++f) k.x.v[f * n + p] = (gHalf[px[f]] - net.mean[f]) * net.scale[f];
            for (int c = 0; c < 3 * K; ++c) k.cand.v[c * n + p] = gHalf[px[F + c]];
            for (int c = 0; c < 3; ++c) k.gt.v[c * n + p] = gHalf[px[F + 3 * K + c]];
            if (ds.extra == 3)
                for (int c = 0; c < 3; ++c) k.engine.v[c * n + p] = gHalf[px[kBaseChannels + c]];
        }
}

void computeNormalisation(const Dataset& ds, BlendNet& net) {
    std::array<double, F> sum{}, sq{};
    double count = 0.0;
    const size_t pixels = static_cast<size_t>(ds.patch) * ds.patch;
    for (size_t i = 0; i < ds.records.size(); ++i) {
        const uint16_t* base = ds.sample(i);
        for (size_t p = 0; p < pixels; p += 7) {
            const uint16_t* px = base + p * ds.channels;
            for (int f = 0; f < F; ++f) {
                const double v = gHalf[px[f]];
                sum[f] += v;
                sq[f] += v * v;
            }
            count += 1.0;
        }
    }
    for (int f = 0; f < F; ++f) {
        const double mean = sum[f] / count;
        const double var = std::max(sq[f] / count - mean * mean, 0.0);
        net.mean[f] = static_cast<float>(mean);
        // A constant feature (a flag that never flips in the data) must not
        // be blown up by a tiny deviation.
        net.scale[f] = static_cast<float>(1.0 / std::max(std::sqrt(var), 0.02));
    }
}

double psnr(double sq, double count) { return 10.0 * std::log10(count / std::max(sq, 1e-12)); }

Errors evaluate(const Dataset& ds, const BlendNet& net, int margin, std::vector<Work>& work,
                std::vector<Errors>* perFile = nullptr) {
    const int threads = static_cast<int>(work.size());
    std::vector<std::vector<Errors>> local(threads, std::vector<Errors>(ds.names.size()));
    const long long n = static_cast<long long>(ds.records.size());
    #pragma omp parallel for schedule(dynamic, 4) num_threads(threads)
    for (long long i = 0; i < n; ++i) {
        Work& k = work[omp_get_thread_num()];
        loadSample(ds, static_cast<size_t>(i), 0, net, margin, k);
        forward(net, k);
        local[omp_get_thread_num()][ds.file[i]].add(lossAndGradient(k, 1.0, false, true));
    }
    Errors total;
    if (perFile) perFile->assign(ds.names.size(), Errors());
    for (const auto& t : local)
        for (size_t f = 0; f < t.size(); ++f) {
            total.add(t[f]);
            if (perFile) (*perFile)[f].add(t[f]);
        }
    return total;
}

// ---------------------------------------------------------------------------
// Commands

using Args = std::map<std::string, std::string>;

Args parseArgs(int argc, char** argv, int first) {
    Args args;
    for (int i = first; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0) continue;
        key = key.substr(2);
        args[key] = i + 1 < argc ? argv[++i] : "";
    }
    return args;
}

std::string get(const Args& a, const std::string& key, const std::string& fallback) {
    const auto it = a.find(key);
    return it == a.end() ? fallback : it->second;
}

struct Adam {
    Grads m, v;
    int t = 0;

    void step(BlendNet& net, const Grads& g, float lr) {
        ++t;
        const float b1 = 0.9f, b2 = 0.999f, eps = 1e-8f;
        const float c1 = 1.f - std::pow(b1, static_cast<float>(t));
        const float c2 = 1.f - std::pow(b2, static_cast<float>(t));
        auto update = [&](std::vector<float>& p, const std::vector<float>& grad, std::vector<float>& mm,
                          std::vector<float>& vv) {
            for (size_t i = 0; i < p.size(); ++i) {
                mm[i] = b1 * mm[i] + (1.f - b1) * grad[i];
                vv[i] = b2 * vv[i] + (1.f - b2) * grad[i] * grad[i];
                p[i] -= lr * (mm[i] / c1) / (std::sqrt(vv[i] / c2) + eps);
            }
        };
        for (int l = 0; l < ml::kLayerCount; ++l) {
            update(net.layers[l].w, g.w[l], m.w[l], v.w[l]);
            update(net.layers[l].b, g.b[l], m.b[l], v.b[l]);
        }
    }
};

void reportEval(const char* label, const Errors& e) {
    std::printf("%-10s loss %.5f   PSNR network %6.2f dB   heuristic %6.2f dB   gain %+5.2f dB"
                "   best single candidate %6.2f dB\n",
                label, e.loss / e.count, psnr(e.sqNet, e.count), psnr(e.sqHeur, e.count),
                psnr(e.sqNet, e.count) - psnr(e.sqHeur, e.count), psnr(e.sqOracle, e.count));
}

int commandTrain(const Args& args) {
    Dataset train, val;
    std::printf("training data\n");
    if (!loadAll(get(args, "data", ""), train)) return 1;
    std::printf("validation data\n");
    const bool haveVal = loadAll(get(args, "val", ""), val);
    if (haveVal && val.patch != train.patch) {
        std::fprintf(stderr, "validation patch size differs\n");
        return 1;
    }

    const int c0 = std::stoi(get(args, "c0", "8"));
    const int c1 = std::stoi(get(args, "c1", "16"));
    const int steps = std::stoi(get(args, "steps", "12000"));
    const int batch = std::stoi(get(args, "batch", "16"));
    const float lr = std::stof(get(args, "lr", "0.002"));
    const int margin = std::stoi(get(args, "margin", "16"));
    const bool augment = std::stoi(get(args, "augment", "1")) != 0;
    const int evalEvery = std::stoi(get(args, "eval-every", "1000"));
    const unsigned seed = static_cast<unsigned>(std::stoul(get(args, "seed", "1")));
    const std::string out = get(args, "out", "fg_blend.bin");
    const std::string logPath = get(args, "log", "");
    gMseLoss = get(args, "loss", "charbonnier") == "mse";

    BlendNet net;
    net.shape(c0, c1);
    net.init(seed);
    computeNormalisation(train, net);
    std::printf("network c0 %d c1 %d, %zu parameters; %zu training patches of %d px, batch %d, %d steps\n",
                c0, c1, net.parameters(), train.records.size(), train.patch, batch, steps);

    const int threads = omp_get_max_threads();
    std::vector<Work> work(threads);
    for (Work& k : work) k.grads.shape(net);
    Grads total;
    total.shape(net);
    Adam adam;
    adam.m.shape(net);
    adam.v.shape(net);

    std::ofstream log;
    if (!logPath.empty()) {
        log.open(logPath);
        log << "step,lr,train_loss,val_loss,val_psnr_net,val_psnr_heur,seconds\n";
    }

    std::mt19937 rng(seed * 7919u + 17u);
    std::uniform_int_distribution<size_t> pick(0, train.records.size() - 1);
    std::uniform_int_distribution<int> pickAug(0, 7);
    const auto start = std::chrono::steady_clock::now();
    double runningLoss = 0.0;
    int runningCount = 0;
    double bestVal = 1e30;

    auto evaluateAndSave = [&](int step, float currentLr) {
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        double valLoss = 0.0, valNet = 0.0, valHeur = 0.0;
        if (haveVal) {
            const Errors e = evaluate(val, net, margin, work);
            valLoss = e.loss / e.count;
            valNet = psnr(e.sqNet, e.count);
            valHeur = psnr(e.sqHeur, e.count);
            std::printf("  [val %6d] loss %.5f   network %6.2f dB   heuristic %6.2f dB   gain %+5.2f dB\n",
                        step, valLoss, valNet, valHeur, valNet - valHeur);
            if (e.sqNet < bestVal) {
                bestVal = e.sqNet;
                net.save(out);
            }
        } else {
            net.save(out);
        }
        if (log.is_open())
            log << step << ',' << currentLr << ',' << (runningCount ? runningLoss / runningCount : 0.0) << ','
                << valLoss << ',' << valNet << ',' << valHeur << ',' << seconds << '\n';
        std::fflush(stdout);
    };

    for (int step = 1; step <= steps; ++step) {
        const int warmup = std::min(300, steps / 10);
        float currentLr = lr;
        if (step <= warmup)
            currentLr = lr * static_cast<float>(step) / static_cast<float>(warmup);
        else {
            const double t = static_cast<double>(step - warmup) / std::max(steps - warmup, 1);
            currentLr = static_cast<float>(lr * (0.02 + 0.98 * 0.5 * (1.0 + std::cos(M_PI * t))));
        }

        std::vector<std::pair<size_t, int>> items(batch);
        for (auto& item : items) item = {pick(rng), augment ? pickAug(rng) : 0};
        for (Work& k : work) k.grads.zero();
        double batchLoss = 0.0;
        #pragma omp parallel for schedule(dynamic, 1) reduction(+ : batchLoss) num_threads(threads)
        for (int b = 0; b < batch; ++b) {
            Work& k = work[omp_get_thread_num()];
            loadSample(train, items[b].first, items[b].second, net, margin, k);
            forward(net, k);
            const Errors e = lossAndGradient(k, batch, true, false);
            backward(net, k);
            batchLoss += e.loss / std::max(e.count, 1.0);
        }
        total.zero();
        for (const Work& k : work)
            for (int l = 0; l < ml::kLayerCount; ++l) {
                for (size_t i = 0; i < total.w[l].size(); ++i) total.w[l][i] += k.grads.w[l][i];
                for (size_t i = 0; i < total.b[l].size(); ++i) total.b[l][i] += k.grads.b[l][i];
            }
        adam.step(net, total, currentLr);
        runningLoss += batchLoss / batch;
        ++runningCount;

        if (step % 100 == 0) {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::printf("step %6d  lr %.5f  loss %.5f  %.0f s\n", step, currentLr, runningLoss / runningCount,
                        seconds);
        }
        if (step % evalEvery == 0 || step == steps) {
            evaluateAndSave(step, currentLr);
            runningLoss = 0.0;
            runningCount = 0;
        }
    }
    std::printf("saved %s (%s)\n", out.c_str(), haveVal ? "best validation step" : "last step");
    return 0;
}

int commandEval(const Args& args) {
    BlendNet net;
    const std::string weights = get(args, "weights", "");
    if (!net.load(weights)) {
        std::fprintf(stderr, "cannot load %s\n", weights.c_str());
        return 1;
    }
    Dataset ds;
    if (!loadAll(get(args, "data", ""), ds)) return 1;
    const int margin = std::stoi(get(args, "margin", "16"));
    std::vector<Work> work(omp_get_max_threads());
    std::vector<Errors> perFile;
    const Errors total = evaluate(ds, net, margin, work, &perFile);
    for (size_t f = 0; f < ds.names.size(); ++f) {
        std::printf("%s\n", ds.names[f].c_str());
        reportEval("", perFile[f]);
    }
    reportEval("all", total);
    if (ds.extra == 3)
        std::printf("engine vs trainer (inner %d px margin): mean |diff| %.2e, max %.2e\n", margin,
                    total.engineAbs / total.count, total.engineMax);
    return 0;
}

// Central differences against the analytic gradient on random data. ReLU kinks
// make an occasional parameter disagree; a broken backward pass disagrees on
// all of them.
int commandGradcheck() {
    BlendNet net;
    net.shape(4, 8);
    net.init(3);
    std::mt19937 rng(5);
    std::normal_distribution<float> spread(0.f, 0.5f);
    for (float& v : net.layers[ml::kDec0].w) v = spread(rng);
    for (float& v : net.layers[ml::kDec0].b) v = spread(rng);

    const int P = 16;
    Work k;
    k.grads.shape(net);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    k.x.resize(F, P, P);
    k.cand.resize(3 * K, P, P);
    k.gt.resize(3, P, P);
    for (float& v : k.x.v) v = unit(rng) * 2.f - 1.f;
    for (float& v : k.cand.v) v = unit(rng);
    for (float& v : k.gt.v) v = unit(rng);
    k.mask.assign(static_cast<size_t>(P) * P, 1);

    const double norm = static_cast<double>(P) * P * 3.0;
    auto loss = [&]() {
        forward(net, k);
        return lossAndGradient(k, 1.0, false, false).loss / norm;
    };
    forward(net, k);
    lossAndGradient(k, 1.0, true, false);
    backward(net, k);

    std::uniform_int_distribution<size_t> any(0, 1 << 30);
    int bad = 0, total = 0;
    for (int l = 0; l < ml::kLayerCount; ++l) {
        double worst = 0.0;
        for (int trial = 0; trial < 12; ++trial) {
            const bool bias = trial % 3 == 2;
            std::vector<float>& params = bias ? net.layers[l].b : net.layers[l].w;
            const size_t i = any(rng) % params.size();
            const float analytic = bias ? k.grads.b[l][i] : k.grads.w[l][i];
            const float saved = params[i];
            const float h = 2e-3f;
            params[i] = saved + h;
            const double up = loss();
            params[i] = saved - h;
            const double down = loss();
            params[i] = saved;
            const double numeric = (up - down) / (2.0 * h);
            const double rel = std::abs(numeric - analytic) / std::max(std::abs(numeric) + std::abs(analytic), 1e-5);
            worst = std::max(worst, rel);
            ++total;
            if (rel > 0.05) ++bad;
        }
        std::printf("layer %d  worst relative error %.3e\n", l, worst);
    }
    std::printf("%d of %d sampled parameters above 5%%\n", bad, total);
    return bad * 10 > total ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 0; i < 65536; ++i) gHalf[i] = ml::fromHalf(static_cast<uint16_t>(i));
    const std::string command = argc > 1 ? argv[1] : "";
    const Args args = parseArgs(argc, argv, 2);
    if (command == "train") return commandTrain(args);
    if (command == "eval") return commandEval(args);
    if (command == "gradcheck") return commandGradcheck();
    std::fprintf(stderr, "usage: fg_train train|eval|gradcheck [options]; see the header of tools/fg_train/main.cpp\n");
    return 2;
}
