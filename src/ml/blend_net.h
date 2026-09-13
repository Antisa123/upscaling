#pragma once

// M9: the learned blend network, shared by the trainer (tools/fg_train) and the
// engine (framegen::MlBlend). No GL here: this is the one definition of the
// architecture, the weight file and the dataset record, and both sides reading
// the same header is what keeps the shader and the trainer computing the same
// function.
//
// The network does not paint pixels. It predicts, per pixel, softmax weights
// over K candidate colours the classical pipeline already produced, and the
// output is their weighted sum. That keeps the experiment a clean comparison
// with the heuristic -- same inputs, same candidates, only the rule that mixes
// them is learned -- and it keeps the model small enough to run in a compute
// shader on hardware without matrix units: it cannot hallucinate detail, so it
// does not need the capacity to.
//
// Candidates (fg_ml_common.glsl):
//   0  the heuristic's own frame (passes 7-9, inpainting included)
//   1  game vector, warped from the previous frame   2  ... from the current
//   3  flow vector, warped from the previous frame   4  ... from the current
//   5  current frame, unwarped                       6  previous frame, unwarped
//
// Features (20, four per RGBA16F layer, fg_ml_common.glsl): field validity,
// both disocclusion masks, on-screen flags of the four warps, colour agreement
// of each warp pair and between the fields, current/previous difference,
// distance of the heuristic from each field's warp, heuristic coverage,
// pyramid levels the vectors came from, vector magnitude, field disagreement,
// luminance.
//
// Architecture (U-Net-lite at display resolution W x H, both divisible by 4):
//
//   features F   W    --1x1-->  enc0 c0  W        (kEnc0)
//   pool 2x2          --3x3-->  enc1 c1  W/2      (kEnc1)
//   pool 2x2          --3x3-->  enc2 c1  W/4      (kEnc2)
//                     --3x3-->  enc3 c1  W/4      (kEnc3)
//   up(enc3) + enc1   --3x3-->  dec1 c0  W/2      (kDec1)
//   up(dec1) + enc0   --1x1-->  logits K W        (kDec0), softmax, blend
//
// ReLU after every layer but the last. Padding replicates the edge, pooling is
// the 2x2 mean, upsampling is bilinear with half-texel centres and clamped
// edges -- what a single GL_LINEAR fetch computes. The shader passes do not
// follow the layer list one to one (framegen/ml_blend.cpp): enc0 runs inside
// the feature pass, pooling and the skip sum are passes of their own, and
// kDec0 runs inside the blend pass.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace ml {

constexpr int kFeatures = 20;
constexpr int kCandidates = 7;
constexpr uint32_t kWeightsMagic = 0x4C4D4746;  // "FGML"
constexpr uint32_t kDatasetMagic = 0x53444746;  // "FGDS"
constexpr uint32_t kFormatVersion = 1;   // weights
constexpr uint32_t kDatasetVersion = 2;  // 2: frame size in the header

enum LayerIndex : int { kEnc0 = 0, kEnc1, kEnc2, kEnc3, kDec1, kDec0, kLayerCount };

// Channels are stored four to an RGBA layer.
inline int packedLayers(int channels) { return (channels + 3) / 4; }

struct ConvLayer {
    int cin = 0;
    int cout = 0;
    int k = 1;
    std::vector<float> w;  // [cout][cin][tap], taps row-major, row = texel y
    std::vector<float> b;  // [cout]

    void shape(int in, int out, int kernel) {
        cin = in;
        cout = out;
        k = kernel;
        w.assign(static_cast<size_t>(out) * in * kernel * kernel, 0.f);
        b.assign(static_cast<size_t>(out), 0.f);
    }
    int taps() const { return k * k; }
    size_t index(int o, int i, int t) const {
        return (static_cast<size_t>(o) * cin + i) * taps() + t;
    }
};

struct BlendNet {
    int c0 = 8;
    int c1 = 16;
    std::array<ConvLayer, kLayerCount> layers;
    // Input normalisation, x' = (x - mean) * scale. Kept separate in the file
    // so the trainer can report it; the engine folds it into kEnc0.
    std::array<float, kFeatures> mean{};
    std::array<float, kFeatures> scale{};

    void shape(int hidden0, int hidden1) {
        c0 = hidden0;
        c1 = hidden1;
        layers[kEnc0].shape(kFeatures, c0, 1);
        layers[kEnc1].shape(c0, c1, 3);
        layers[kEnc2].shape(c1, c1, 3);
        layers[kEnc3].shape(c1, c1, 3);
        layers[kDec1].shape(c1, c0, 3);
        layers[kDec0].shape(c0, kCandidates, 1);
        mean.fill(0.f);
        scale.fill(1.f);
    }

    void init(uint32_t seed) {
        std::mt19937 rng(seed);
        for (int l = 0; l < kLayerCount; ++l) {
            ConvLayer& layer = layers[l];
            const float std = l == kDec0 ? 0.01f : std::sqrt(2.f / static_cast<float>(layer.cin * layer.taps()));
            std::normal_distribution<float> dist(0.f, std);
            for (float& v : layer.w) v = dist(rng);
            std::fill(layer.b.begin(), layer.b.end(), 0.f);
        }
        // Start at the heuristic: candidate 0 gets about 90% of the weight, so
        // training begins from the classical result and has to earn every
        // departure from it.
        layers[kDec0].b[0] = 4.f;
    }

    size_t parameters() const {
        size_t n = 0;
        for (const ConvLayer& layer : layers) n += layer.w.size() + layer.b.size();
        return n;
    }

    bool save(const std::string& path) const {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;
        const int32_t header[6] = {static_cast<int32_t>(kWeightsMagic), static_cast<int32_t>(kFormatVersion),
                                   c0, c1, kFeatures, kCandidates};
        bool ok = std::fwrite(header, sizeof(header), 1, f) == 1;
        ok = ok && std::fwrite(mean.data(), sizeof(float), kFeatures, f) == kFeatures;
        ok = ok && std::fwrite(scale.data(), sizeof(float), kFeatures, f) == kFeatures;
        for (const ConvLayer& layer : layers) {
            ok = ok && std::fwrite(layer.w.data(), sizeof(float), layer.w.size(), f) == layer.w.size();
            ok = ok && std::fwrite(layer.b.data(), sizeof(float), layer.b.size(), f) == layer.b.size();
        }
        std::fclose(f);
        return ok;
    }

    bool load(const std::string& path) {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        int32_t header[6] = {};
        bool ok = std::fread(header, sizeof(header), 1, f) == 1 &&
                  header[0] == static_cast<int32_t>(kWeightsMagic) &&
                  header[1] == static_cast<int32_t>(kFormatVersion) && header[4] == kFeatures &&
                  header[5] == kCandidates && header[2] > 0 && header[3] > 0;
        if (ok) {
            shape(header[2], header[3]);
            ok = std::fread(mean.data(), sizeof(float), kFeatures, f) == kFeatures &&
                 std::fread(scale.data(), sizeof(float), kFeatures, f) == kFeatures;
            for (ConvLayer& layer : layers) {
                ok = ok && std::fread(layer.w.data(), sizeof(float), layer.w.size(), f) == layer.w.size();
                ok = ok && std::fread(layer.b.data(), sizeof(float), layer.b.size(), f) == layer.b.size();
            }
        }
        std::fclose(f);
        return ok;
    }
};

// Dataset file: one DatasetHeader, then records of PatchRecord followed by
// patch * patch * recordChannels() half floats, pixel-interleaved, rows in GL
// order (row 0 at the bottom). Per pixel: features, candidates (rgb each),
// ground truth rgb, and -- when `extra` is 3 -- the engine's own ML output,
// which is how the trainer checks the shader computes what it trained.
//
// The frame size tells the trainer which patch sides are the frame's own edge.
// There the replicated padding it applies is exactly what the shader does, so
// those pixels are valid targets -- and the strip along the frame edge is where
// camera motion uncovers the scene, which a model never trained on it gets
// badly wrong.
struct DatasetHeader {
    uint32_t magic = kDatasetMagic;
    uint32_t version = kDatasetVersion;
    int32_t patch = 0;
    int32_t features = kFeatures;
    int32_t candidates = kCandidates;
    int32_t extra = 0;
    int32_t frameWidth = 0;
    int32_t frameHeight = 0;
};

struct PatchRecord {
    int32_t frame = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t selection = 0;  // 0 uniformly random, 1 drawn by heuristic error
};

inline int recordChannels(const DatasetHeader& h) {
    return h.features + 3 * h.candidates + 3 + h.extra;
}

inline uint16_t toHalf(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16) & 0x8000u;
    const int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFFu) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFFu;
    if (exponent <= 0) {
        if (exponent < -10) return static_cast<uint16_t>(sign);
        mantissa |= 0x800000u;
        const int shift = 14 - exponent;
        uint32_t half = mantissa >> shift;
        if ((mantissa >> (shift - 1)) & 1u) ++half;
        return static_cast<uint16_t>(sign | half);
    }
    if (exponent >= 31) return static_cast<uint16_t>(sign | 0x7C00u);
    uint32_t half = sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
    if (mantissa & 0x1000u) ++half;
    return static_cast<uint16_t>(half);
}

inline float fromHalf(uint16_t h) {
    const int e = (h >> 10) & 0x1F;
    const int m = h & 0x3FF;
    float v = 0.f;
    if (e == 0)
        v = std::ldexp(static_cast<float>(m), -24);
    else if (e == 31)
        v = m ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
    else
        v = std::ldexp(static_cast<float>(m + 1024), e - 25);
    return (h & 0x8000) ? -v : v;
}

}  // namespace ml
