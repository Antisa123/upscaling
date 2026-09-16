#pragma once

#include <cstdio>
#include <random>
#include <string>

#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "ml/blend_net.h"

namespace framegen {

// What passes 10-12 read: the frame generator's own intermediates.
struct MlInputs {
    const gfx::Texture2D* current = nullptr;
    const gfx::Texture2D* previous = nullptr;
    const gfx::Texture2D* gameField = nullptr;
    const gfx::Texture2D* flowField = nullptr;
    const gfx::Texture2D* masks = nullptr;
    const gfx::Texture2D* heuristic = nullptr;
    int fieldWidth = 0;
    int fieldHeight = 0;
    int levels = 0;
    bool game = true;
    bool flow = true;
    bool masksEnabled = true;
};

// M9, passes 10-12: the learned blend (src/ml/blend_net.h for the model).
//
// Two independent jobs. With weights, it runs the network and produces a frame
// that replaces the heuristic's. With dumping on, it writes the features and
// candidates the network would see, which is how the training set is captured
// -- from the same shader code the network runs on, so there is no second
// implementation of the inputs to drift out of step with the first.
class MlBlend {
public:
    // `weights` may be empty (features only, for dumping).
    bool create(const std::string& weights, bool dump);
    void destroy();
    void reloadShaders();

    bool networkLoaded() const { return loaded_; }
    bool enabled() const { return loaded_ || dump_; }
    const ml::BlendNet& net() const { return net_; }

    // Pass 10, and 11-12 when the network is loaded and fits the resolution.
    // Returns true if output() holds this frame's ML image.
    bool dispatch(const MlInputs& in, gfx::GpuTimer& timer);
    const gfx::Texture2D& output() const { return output_; }
    // The weight view (fg_ml_blend.comp, uWriteWeights): filled while on.
    void setWeightView(bool on) { weightView_ = on; }
    const gfx::Texture2D& weights() const { return weights_; }

    // Dataset capture: `count` patches of this frame, half uniformly random
    // and half drawn in proportion to the heuristic's error, appended to
    // `file` (see ml::DatasetHeader). `reference` is the real midpoint frame.
    void dumpPatches(std::FILE* file, long long frame, const gfx::Texture2D& heuristic,
                     const gfx::Texture2D& reference, int patch, int count, std::mt19937& rng,
                     bool withMlOutput);
    static bool writeDatasetHeader(std::FILE* file, int patch, bool withMlOutput, int width, int height);

private:
    void ensure(int width, int height);
    void bindCommon(const gfx::Program& program, const MlInputs& in) const;

    ml::BlendNet net_;
    bool loaded_ = false;
    bool dump_ = false;
    bool fits_ = false;
    bool lastValid_ = false;
    // The dataset header carries the frame size, so it goes in with the
    // first patches rather than when the file is opened.
    bool headerWritten_ = false;

    gfx::Program features_;         // pass 10, with enc0 folded in
    gfx::Program conv_[ml::kDec0];  // enc1..dec1 (enc0 is unused)
    gfx::Program pool0_, pool1_, upSum1_;
    gfx::Program blend_;
    GLuint buffers_[ml::kLayerCount] = {};

    gfx::Texture2DArray featureMaps_;  // capture only
    gfx::Texture2DArray candidates_;   // capture only
    gfx::Texture2DArray enc0_, pooled0_, enc1_, pooled1_, enc2_, enc3_, sum1_, dec1_;
    gfx::Texture2D output_;
    gfx::Texture2D weights_;
    bool weightView_ = false;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace framegen
