#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <memory>
#include <random>
#include <stb_image_write.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "gfx/gl_common.h"
#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "metrics/image_metrics.h"
#include "framegen/frame_generator.h"
#include "opticalflow/optical_flow.h"
#include "present/frame_pacer.h"
#include "present/hud.h"
#include "upscale/upscaler.h"
#include "renderer/camera.h"
#include "renderer/gbuffer.h"
#include "renderer/scene.h"

namespace {

struct Options {
    int displayWidth = 1920;
    int displayHeight = 1080;
    float renderScale = 1.0f / 1.5f;  // FSR "Quality"
    bool vsync = false;
    bool jitter = true;
    // Headless-ish capture mode: render N frames, optionally save the last one
    // and exit. Used by the measurement scripts so runs are reproducible.
    int frameLimit = 0;
    bool autoShot = false;
    bool scripted = false;
    double fixedDeltaTime = 0.0;  // 0 = wall clock
    int debugView = 0;
    std::string outputPath = "captures/frame.png";
    bool validateMv = false;
    // -1 = decide from the mode, 0/1 = explicit --no-filter-pattern/--filter-pattern.
    int filterPattern = -1;
    std::string csvPath;
    // Empty = the built-in procedural scene.
    std::string scenePath;
    float sceneFit = 12.f;
    // Radius of the scripted orbit, in scene units.
    float pathRadius = 5.f;
    float pathPhase = 0.f;
    // Teleport the scripted camera half an orbit every N frames. A real cut is
    // the one event the scene-change detector exists for, and a smooth orbit
    // never produces one; without this the detector can only be tested for
    // false positives.
    int cutEvery = 0;
    // Saturation point of the flow debug views, in display pixels.
    float flowScale = 24.f;
    // Ground-truth capture: directory to dump reference frames into.
    std::string captureGtDir;
    // Which spatial baseline runs between the G-buffer and the screen.
    // Empty = present the G-buffer directly (the pre-M3 path).
    std::string upscaler;
    // Score the upscaled frame against a display-resolution ground-truth
    // render of the same instant.
    bool validateUpscale = false;
    // RCAS sharpening, in stops: 0 is the strongest lobe the filter allows and
    // every step up halves it. NaN = pick the default for the mode (see
    // resolveSharpness below), because the two users of RCAS want opposite
    // things: FSR1 sharpens an image EASU already pushed to the limit of the
    // information in it, while TAAU sharpens one that history resampling
    // measurably softened, so it can take -- and wants -- far less.
    float sharpness = std::numeric_limits<float>::quiet_NaN();
    // Accumulation cap for the temporal upscaler, in frames.
    float taauFrames = 8.f;
    // Width of the temporal clamp box, in standard deviations.
    float taauGamma = 2.f;
    // Decay of the accumulation cap with motion, per display pixel per frame.
    float taauMotionDecay = 1.6f;
    // Inverse variance of the reconstruction kernel, in render pixels^-2.
    float taauKernel = 6.f;
    // M5 knobs. Radial scale of the Lanczos-2 reconstruction kernel, moving
    // and still.
    float fsrLanczos = 1.f;
    float fsrLanczosStill = 2.f;
    // Relative depth mismatch tolerated before a pixel counts as disoccluded.
    float fsrDepthTolerance = 0.02f;
    // 0 turns the depth-based history rejection off, which is the M4 behaviour.
    float fsrDisocclusion = 1.f;
    // 0 collapses the dilation window to the centre pixel.
    int fsrDilate = 1;
    // Lock lifetime in frames (0 = no locking), clamp widening on a locked
    // pixel, the luminance change that invalidates a lock, and the contrast a
    // thin feature needs before it is locked.
    float fsrLockLife = 4.f;
    float fsrLockRelax = 2.f;
    float fsrLockTolerance = 0.1f;
    float fsrLockContrast = 0.5f;
    // Reactive mask strength for alpha-tested surfaces. Off by default: on a
    // scene whose only non-opaque surfaces are alpha-tested foliage the mask
    // measures as a monotone loss, because the alpha test is deterministic --
    // the same sample gives the same answer every frame, the motion vectors
    // are right, and the history it throws away was correct. The mechanism is
    // here for content that has real transparency; see docs/FSR.md.
    float fsrReactive = 0.f;
    // Supersampling factor of the reference render. 1 reproduces the aliased
    // display-resolution reference; 2 renders four samples per display pixel
    // and averages them, which is the image a reconstruction filter is
    // actually trying to converge to.
    int gtSupersample = 2;
    // Texture LOD bias for the render-resolution pass. NaN = derive it from the
    // upscale ratio (see applyMipBias below); an explicit value overrides that,
    // which is what the ablation rows need.
    float mipBias = std::numeric_limits<float>::quiet_NaN();
    // Frames dropped from the CSV before measuring. Pipeline creation, shader
    // compilation and the first texture uploads land on the first few frames
    // and are not part of any steady-state cost.
    long long warmupFrames = 16;
    // M6. Optical flow is an independent module -- the upscaler does not
    // consume it, frame generation will -- so it stays off unless asked for.
    // Leaving it on by default would quietly add its cost to every upscaler
    // measurement already in the results tables.
    bool opticalFlow = false;
    int ofLevels = opticalflow::kMaxLevels;
    int ofRadius = 4;
    float ofSmoothness = 0.0005f;
    int ofFilter = 1;
    int ofUpscale = 1;
    int ofTemporal = 1;
    float ofNovelty = 0.001f;
    int ofSceneChange = 1;
    float ofSceneThreshold = 0.25f;
    // 0 = worst section, 1 = mean, 2 = median.
    int ofSceneStatistic = 1;
    // Score the flow against the rasteriser's own motion vectors, which are
    // exact for camera motion and therefore a free ground truth.
    bool validateFlow = false;

    // M7. Frame generation consumes the pair of display-resolution frames the
    // upscaler produces, so it needs one; --fg without --upscaler is an error
    // rather than a silent fallback to the render-resolution image, which
    // would quietly measure something else.
    bool frameGen = false;
    int fgDilate = 1;
    int fgGame = 1;
    int fgFlow = 1;
    int fgMasks = 1;
    int fgLevels = framegen::kMaxLevels;
    // 0 = the pyramid keeps the nearest surface, 1 = the background.
    int fgInpaintPick = 1;
    float fgDepth = 0.02f;
    int fgColorPriority = 0;
    float fgFlowBias = 0.5f;
    float fgAgreement = 24.f;
    float fgFlowError = 0.05f;
    float fgFlowMagnitude = 64.f;
    // Score the interpolated frame against a real render of the instant it
    // approximates, and against the 50/50 blend of its own two inputs.
    bool validateFg = false;
    // M8, passes 8-9: image inpainting and the off-screen sample rejection.
    int fgInpaint = 1;
    int fgBounds = 1;
    float fgInpaintCoverage = 0.3f;
    int fgCoverageMasks = 1;
    // M9. Learned blend weights (empty = heuristic), and the dataset capture:
    // patches of the network's inputs and the reference frame, appended to a
    // file for tools/fg_train. Capture needs --validate-fg for the reference.
    std::string fgMl;
    std::string mlDump;
    int mlPatches = 8;
    int mlPatchSize = 96;
    unsigned mlSeed = 1;
    // Scene time the run starts at, so a capture can cover a different
    // stretch of the animation than the measurement window does.
    double timeOffset = 0.0;
    // M9 figures: at these frames (the CSV frame numbers) of a --validate-fg
    // run, write the reference, the 50/50 blend, the heuristic, the learned
    // blend and its weight view as PNGs into fgShots.
    std::string fgShots;
    std::vector<long long> fgShotFrames;

    // M8. How generated frames reach the screen. "paced" holds the real frame
    // back half a render period behind the generated one; "immediate" shows
    // them back to back, which is what naive frame generation does.
    std::string pacing = "paced";
    // 0 runs frame generation (and pays for it) but presents real frames only.
    int presentFg = 1;
    // One row per presented image: when its frame was sampled, handed over,
    // and put on screen. See scripts/run_pacing.py.
    std::string presentCsv;
    // Realtime run length, wall clock. The counterpart of --frames for runs
    // whose point is timing rather than bit-identical images.
    double runSeconds = 0.0;
    // Extra G-buffer renders per frame: a stand-in for a heavier game, so the
    // render cost frame generation has to beat can be dialled in.
    int load = 0;
    // none | composite | baked. Empty = composite interactively, none in a
    // measurement run (the HUD is not free, and it is not part of any earlier
    // result table).
    std::string ui;
    bool tearLines = false;
    // Sync the render thread's GPU work after every stage: 1 = glFlush,
    // 2 = glFinish. Off by default -- the presenter stages its frames on an
    // idle GPU instead (frame_pacer.h), and both of these were measured as
    // the alternatives: a flush does not help, and a finish paces well but
    // costs 30% of the frame rate in GPU idle time. See docs/PACING.md.
    int gpuFlush = 0;

};

enum class UiMode { None, Composite, Baked };

const char* kScaleNames[] = {"Native", "NativeAA 1.0x", "Quality 1.5x", "Balanced 1.7x",
                             "Performance 2.0x", "Ultra Performance 3.0x"};
const float kScaleFactors[] = {1.0f, 1.0f, 1.0f / 1.5f, 1.0f / 1.7f, 0.5f, 1.0f / 3.0f};

// Reads back a framebuffer at full display resolution. The window is never
// the source: under a tiling compositor it is whatever size the WM decided,
// while captures must match the configured display resolution exactly or the
// quality metrics compare images of different sizes.
void writeImage(GLuint framebuffer, int width, int height, const std::string& path) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glReadBuffer(framebuffer == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3))
        std::printf("[app] wrote %s (%dx%d)\n", path.c_str(), width, height);
    else
        std::fprintf(stderr, "[app] failed to write %s\n", path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc) options.displayWidth = std::atoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc) options.displayHeight = std::atoi(argv[++i]);
        else if (arg == "--scale" && i + 1 < argc) options.renderScale = 1.0f / std::atof(argv[++i]);
        else if (arg == "--vsync") options.vsync = true;
        else if (arg == "--no-jitter") options.jitter = false;
        // Explicit opposite of --no-jitter, so a capture script can turn it
        // back on for the temporal rows without rebuilding its shared argument
        // list. Later arguments win, so --no-jitter --jitter leaves it on.
        else if (arg == "--jitter") options.jitter = true;
        else if (arg == "--frames" && i + 1 < argc) options.frameLimit = std::atoi(argv[++i]);
        else if (arg == "--shot") options.autoShot = true;
        else if (arg == "--scripted") options.scripted = true;
        else if (arg == "--fixed-dt" && i + 1 < argc) options.fixedDeltaTime = std::atof(argv[++i]);
        else if (arg == "--debug-view" && i + 1 < argc) options.debugView = std::atoi(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) options.outputPath = argv[++i];
        else if (arg == "--validate-mv") options.validateMv = true;
        else if (arg == "--csv" && i + 1 < argc) options.csvPath = argv[++i];
        else if (arg == "--scene" && i + 1 < argc) options.scenePath = argv[++i];
        else if (arg == "--scene-fit" && i + 1 < argc) options.sceneFit = std::atof(argv[++i]);
        else if (arg == "--path-radius" && i + 1 < argc) options.pathRadius = std::atof(argv[++i]);
        else if (arg == "--path-phase" && i + 1 < argc) options.pathPhase = std::atof(argv[++i]);
        else if (arg == "--cut-every" && i + 1 < argc) options.cutEvery = std::atoi(argv[++i]);
        else if (arg == "--flow-scale" && i + 1 < argc) options.flowScale = std::atof(argv[++i]);
        else if (arg == "--capture-gt" && i + 1 < argc) options.captureGtDir = argv[++i];
        else if (arg == "--upscaler" && i + 1 < argc) options.upscaler = argv[++i];
        else if (arg == "--validate-upscale") options.validateUpscale = true;
        else if (arg == "--sharpness" && i + 1 < argc) options.sharpness = std::atof(argv[++i]);
        else if (arg == "--warmup" && i + 1 < argc) options.warmupFrames = std::atoll(argv[++i]);
        else if (arg == "--taau-frames" && i + 1 < argc) options.taauFrames = std::atof(argv[++i]);
        else if (arg == "--taau-gamma" && i + 1 < argc) options.taauGamma = std::atof(argv[++i]);
        else if (arg == "--taau-motion" && i + 1 < argc) options.taauMotionDecay = std::atof(argv[++i]);
        else if (arg == "--taau-kernel" && i + 1 < argc) options.taauKernel = std::atof(argv[++i]);
        else if (arg == "--fsr-lanczos" && i + 1 < argc) options.fsrLanczos = std::atof(argv[++i]);
        else if (arg == "--fsr-lanczos-still" && i + 1 < argc)
            options.fsrLanczosStill = std::atof(argv[++i]);
        else if (arg == "--fsr-depth" && i + 1 < argc)
            options.fsrDepthTolerance = std::atof(argv[++i]);
        else if (arg == "--fsr-disocclusion" && i + 1 < argc)
            options.fsrDisocclusion = std::atof(argv[++i]);
        else if (arg == "--fsr-lock-life" && i + 1 < argc)
            options.fsrLockLife = std::atof(argv[++i]);
        else if (arg == "--fsr-lock-relax" && i + 1 < argc)
            options.fsrLockRelax = std::atof(argv[++i]);
        else if (arg == "--fsr-lock-tolerance" && i + 1 < argc)
            options.fsrLockTolerance = std::atof(argv[++i]);
        else if (arg == "--fsr-lock-contrast" && i + 1 < argc)
            options.fsrLockContrast = std::atof(argv[++i]);
        else if (arg == "--fsr-reactive" && i + 1 < argc)
            options.fsrReactive = std::atof(argv[++i]);
        else if (arg == "--fsr-dilate" && i + 1 < argc) options.fsrDilate = std::atoi(argv[++i]);
        else if (arg == "--gt-ss" && i + 1 < argc) options.gtSupersample = std::atoi(argv[++i]);
        else if (arg == "--mip-bias" && i + 1 < argc) options.mipBias = std::atof(argv[++i]);
        else if (arg == "--optical-flow") options.opticalFlow = true;
        else if (arg == "--of-levels" && i + 1 < argc) options.ofLevels = std::atoi(argv[++i]);
        else if (arg == "--of-radius" && i + 1 < argc) options.ofRadius = std::atoi(argv[++i]);
        else if (arg == "--of-smoothness" && i + 1 < argc)
            options.ofSmoothness = std::atof(argv[++i]);
        else if (arg == "--of-filter" && i + 1 < argc) options.ofFilter = std::atoi(argv[++i]);
        else if (arg == "--of-upscale" && i + 1 < argc) options.ofUpscale = std::atoi(argv[++i]);
        else if (arg == "--of-temporal" && i + 1 < argc)
            options.ofTemporal = std::atoi(argv[++i]);
        else if (arg == "--of-novelty" && i + 1 < argc) options.ofNovelty = std::atof(argv[++i]);
        else if (arg == "--of-scene-change" && i + 1 < argc)
            options.ofSceneChange = std::atoi(argv[++i]);
        else if (arg == "--of-scene-threshold" && i + 1 < argc)
            options.ofSceneThreshold = std::atof(argv[++i]);
        else if (arg == "--of-scene-statistic" && i + 1 < argc)
            options.ofSceneStatistic = std::atoi(argv[++i]);
        else if (arg == "--validate-flow") {
            options.validateFlow = true;
            options.opticalFlow = true;
        }
        else if (arg == "--fg") options.frameGen = true;
        else if (arg == "--fg-dilate" && i + 1 < argc) options.fgDilate = std::atoi(argv[++i]);
        else if (arg == "--fg-game" && i + 1 < argc) options.fgGame = std::atoi(argv[++i]);
        else if (arg == "--fg-flow" && i + 1 < argc) options.fgFlow = std::atoi(argv[++i]);
        else if (arg == "--fg-masks" && i + 1 < argc) options.fgMasks = std::atoi(argv[++i]);
        else if (arg == "--fg-levels" && i + 1 < argc) options.fgLevels = std::atoi(argv[++i]);
        else if (arg == "--fg-inpaint-pick" && i + 1 < argc)
            options.fgInpaintPick = std::atoi(argv[++i]);
        else if (arg == "--fg-depth" && i + 1 < argc) options.fgDepth = std::atof(argv[++i]);
        else if (arg == "--fg-flow-bias" && i + 1 < argc)
            options.fgFlowBias = std::atof(argv[++i]);
        else if (arg == "--fg-color-priority" && i + 1 < argc)
            options.fgColorPriority = std::atoi(argv[++i]);
        else if (arg == "--fg-agreement" && i + 1 < argc)
            options.fgAgreement = std::atof(argv[++i]);
        else if (arg == "--fg-flow-error" && i + 1 < argc)
            options.fgFlowError = std::atof(argv[++i]);
        else if (arg == "--fg-flow-magnitude" && i + 1 < argc)
            options.fgFlowMagnitude = std::atof(argv[++i]);
        else if (arg == "--validate-fg") {
            options.validateFg = true;
            options.frameGen = true;
        }
        else if (arg == "--fg-inpaint" && i + 1 < argc) options.fgInpaint = std::atoi(argv[++i]);
        else if (arg == "--fg-bounds" && i + 1 < argc) options.fgBounds = std::atoi(argv[++i]);
        else if (arg == "--fg-inpaint-coverage" && i + 1 < argc)
            options.fgInpaintCoverage = std::atof(argv[++i]);
        else if (arg == "--fg-coverage-masks" && i + 1 < argc)
            options.fgCoverageMasks = std::atoi(argv[++i]);
        else if (arg == "--pacing" && i + 1 < argc) options.pacing = argv[++i];
        else if (arg == "--present-fg" && i + 1 < argc) options.presentFg = std::atoi(argv[++i]);
        else if (arg == "--present-csv" && i + 1 < argc) options.presentCsv = argv[++i];
        else if (arg == "--run-seconds" && i + 1 < argc) options.runSeconds = std::atof(argv[++i]);
        else if (arg == "--load" && i + 1 < argc) options.load = std::atoi(argv[++i]);
        else if (arg == "--ui" && i + 1 < argc) options.ui = argv[++i];
        else if (arg == "--tear-lines") options.tearLines = true;
        else if (arg == "--gpu-flush" && i + 1 < argc) options.gpuFlush = std::atoi(argv[++i]);
        else if (arg == "--fg-ml" && i + 1 < argc) options.fgMl = argv[++i];
        else if (arg == "--ml-dump" && i + 1 < argc) options.mlDump = argv[++i];
        else if (arg == "--ml-patches" && i + 1 < argc) options.mlPatches = std::atoi(argv[++i]);
        else if (arg == "--ml-patch-size" && i + 1 < argc) options.mlPatchSize = std::atoi(argv[++i]);
        else if (arg == "--ml-seed" && i + 1 < argc) options.mlSeed = static_cast<unsigned>(std::atoll(argv[++i]));
        else if (arg == "--time-offset" && i + 1 < argc) options.timeOffset = std::atof(argv[++i]);
        else if (arg == "--fg-shots" && i + 1 < argc) options.fgShots = argv[++i];
        else if (arg == "--fg-shot-frames" && i + 1 < argc) {
            std::stringstream list(argv[++i]);
            for (std::string item; std::getline(list, item, ',');)
                if (!item.empty()) options.fgShotFrames.push_back(std::atoll(item.c_str()));
        }
        else if (arg == "--filter-pattern") options.filterPattern = 1;
        else if (arg == "--no-filter-pattern") options.filterPattern = 0;
        else {
            // Silently ignoring an unknown flag is how a measurement run ends
            // up not measuring what its command line says it does: a missing
            // value makes the next flag disappear into it.
            std::fprintf(stderr, "[app] unknown or incomplete argument: %s\n", arg.c_str());
            return 1;
        }
    }

    // Frame generation without the flow field is a supported ablation, and in
    // that configuration the optical flow module is pure cost -- so it only
    // gets switched on when something actually reads it.
    if (options.frameGen && options.fgFlow != 0) options.opticalFlow = true;
    if (options.frameGen && options.upscaler.empty()) {
        std::fprintf(stderr, "[app] --fg needs an upscaler to interpolate between "
                             "(try --upscaler fsr --scale 1.5)\n");
        return 1;
    }
    std::unique_ptr<std::FILE, int (*)(std::FILE*)> mlDumpFile(nullptr, std::fclose);
    if (!options.mlDump.empty()) {
        if (!options.validateFg || !options.frameGen) {
            std::fprintf(stderr, "--ml-dump needs --validate-fg: the reference frame is the label\n");
            return 1;
        }
        // The header goes in with the first patches, once the frame size is known.
        mlDumpFile.reset(std::fopen(options.mlDump.c_str(), "wb"));
        if (!mlDumpFile) {
            std::fprintf(stderr, "cannot write %s\n", options.mlDump.c_str());
            return 1;
        }
    }
    std::mt19937 mlRng(options.mlSeed);

    if (options.validateFg && !options.scripted) {
        std::fprintf(stderr, "[app] --validate-fg needs --scripted: the reference frame is "
                             "rendered at t-dt/2 along the scripted path\n");
        return 1;
    }

    const bool measurementRun = options.frameLimit > 0 || options.runSeconds > 0.0;
    UiMode uiMode = measurementRun ? UiMode::None : UiMode::Composite;
    if (options.ui == "none") uiMode = UiMode::None;
    else if (options.ui == "composite") uiMode = UiMode::Composite;
    else if (options.ui == "baked") uiMode = UiMode::Baked;
    else if (!options.ui.empty()) {
        std::fprintf(stderr, "[app] unknown --ui '%s' (none|composite|baked)\n", options.ui.c_str());
        return 1;
    }
    if (options.pacing != "paced" && options.pacing != "immediate") {
        std::fprintf(stderr, "[app] unknown --pacing '%s' (paced|immediate)\n",
                     options.pacing.c_str());
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "[app] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);  // we render into our own targets

    SDL_Window* window = SDL_CreateWindow(
        "fsr3lite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, options.displayWidth,
        options.displayHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "[app] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    // M8: two contexts sharing objects. The render context never touches the
    // window: it lives on a hidden 1x1 surface, because a context has to be
    // current on *some* surface on this driver stack (a surfaceless
    // MakeCurrent reports success and leaves nothing current). The present
    // context owns the window and belongs to the presenter thread; see
    // present/frame_pacer.h for why presentation has a thread of its own.
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(stderr, "[app] GL 4.6 core context unavailable: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GLContext presentContext = SDL_GL_CreateContext(window);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    SDL_Window* renderWindow = SDL_CreateWindow("fsr3lite render", 0, 0, 1, 1,
                                                SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!presentContext || !renderWindow || SDL_GL_MakeCurrent(renderWindow, context) != 0) {
        std::fprintf(stderr, "[app] shared present context unavailable: %s\n", SDL_GetError());
        return 1;
    }

    std::printf("[gl] %s | %s | GLSL %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION),
                glGetString(GL_SHADING_LANGUAGE_VERSION));
    gfx::installDebugCallback();

    // Reverse-Z needs a [0,1] clip range; without this the depth buffer keeps
    // the OpenGL [-1,1] convention and precision collapses in the distance.
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

    gfx::Program::setShaderDirectory(FSR3LITE_SHADER_DIR);
    gfx::Program presentProgram = gfx::Program::graphics("fullscreen.vert", "present.frag");
    gfx::Program helloCompute = gfx::Program::compute("hello.comp");
    if (!presentProgram.valid() || !helloCompute.valid()) {
        std::fprintf(stderr, "[app] shader compilation failed at startup\n");
        return 1;
    }

    GLuint emptyVao = 0;
    glCreateVertexArrays(1, &emptyVao);

    renderer::Scene scene;
    // A glTF scene that fails to load must not leave an empty world: fall back
    // to the procedural one so a bad path is a warning, not a black window.
    if (options.scenePath.empty() || !scene.loadGltf(options.scenePath, options.sceneFit))
        scene.createProcedural();

    int displayWidth = options.displayWidth;
    int displayHeight = options.displayHeight;

    // The scale factor is what actually sizes the render targets; scaleMode
    // only names it for the HUD and the F1-F5 presets. --scale may ask for a
    // ratio no preset covers, so the two are tracked separately.
    int scaleMode = 2;  // Quality
    float scaleFactor = options.renderScale;
    char scaleLabel[32] = {};
    auto refreshScaleLabel = [&] {
        for (int i = 0; i < static_cast<int>(std::size(kScaleFactors)); ++i) {
            if (std::fabs(kScaleFactors[i] - scaleFactor) < 1e-4f) {
                scaleMode = i;
                std::snprintf(scaleLabel, sizeof(scaleLabel), "%s", kScaleNames[i]);
                return;
            }
        }
        std::snprintf(scaleLabel, sizeof(scaleLabel), "Custom %.2fx", 1.0f / scaleFactor);
    };
    refreshScaleLabel();
    auto renderSize = [&](int& w, int& h) {
        w = std::max(16, static_cast<int>(displayWidth * scaleFactor));
        h = std::max(16, static_cast<int>(displayHeight * scaleFactor));
    };
    int renderWidth = 0, renderHeight = 0;
    renderSize(renderWidth, renderHeight);

    renderer::GBuffer gbuffer;
    gbuffer.create(renderWidth, renderHeight);
    // Validating motion vectors against an aliased surface measures the
    // aliasing, not the vectors, so that mode band-limits the content unless
    // the caller asked otherwise.
    gbuffer.setPatternFilter(options.filterPattern >= 0 ? options.filterPattern == 1
                                                        : options.validateMv);

    // Output surface at display resolution, independent of the window.
    gfx::Texture2D presentTex;
    gfx::Framebuffer presentFbo;
    presentFbo.create("present");
    presentTex.ensure(displayWidth, displayHeight, GL_RGBA8, 1, "present.color");
    presentFbo.attachColor(0, presentTex);
    presentFbo.finalizeAttachments();

    // Compose passes: frame + HUD layer + tear lines into a presenter slot
    // (RGBA8), or into a display-referred RGBA16F image for the baked HUD and
    // the validation references.
    gfx::Program composeProgram = gfx::Program::compute("ui_compose.comp");
    gfx::Program composeHdrProgram = gfx::Program::compute("ui_compose_hdr.comp");
    present::Hud hud;
    if (!composeProgram.valid() || !composeHdrProgram.valid() || !hud.create()) {
        std::fprintf(stderr, "[app] HUD / compose setup failed\n");
        return 1;
    }
    bool hudVisible = true;
    bool tearLines = options.tearLines;

    present::FramePacer pacer;
    if (!pacer.start(window, presentContext, displayWidth, displayHeight, options.vsync)) return 1;
    pacer.setPacing(options.pacing == "immediate" ? present::PacingMode::Immediate
                                                  : present::PacingMode::Paced);
    const double runStartMs = present::FramePacer::nowMs();
    std::ofstream presentCsv;
    if (!options.presentCsv.empty()) {
        presentCsv.open(options.presentCsv);
        presentCsv << "frame,interpolated,sample_ms,ready_ms,present_ms,gpu_wait_ms,caught_up\n";
        presentCsv << std::fixed << std::setprecision(4);
    }
    std::vector<present::PresentRecord> presentHistory;
    auto drainPresents = [&] {
        for (const present::PresentRecord& r : pacer.takeRecords()) {
            if (presentCsv.is_open())
                presentCsv << r.frame << ',' << r.interpolated << ',' << r.sampleMs - runStartMs
                           << ',' << r.readyMs - runStartMs << ',' << r.presentMs - runStartMs
                           << ',' << r.gpuWaitMs << ',' << r.caughtUp << '\n';
            if (measurementRun && r.frame >= options.warmupFrames) presentHistory.push_back(r);
        }
    };

    renderer::Camera camera;
    camera.setPerspective(glm::radians(60.f), 0.05f);
    camera.setResolution(renderWidth, renderHeight, displayWidth, displayHeight);

    // Ground-truth capture renders the same scene into a second G-buffer at
    // gtSs x display resolution, which is then box-averaged down. Only created
    // when asked for: at 1080p and gtSs=2 it is an order of magnitude more
    // memory than the render-resolution one.
    //
    // The supersampling is not a refinement, it is what makes the metric mean
    // anything for a temporal upscaler. A single display-resolution reference
    // render is aliased, and scoring against an aliased reference rewards
    // blur: whatever the reconstruction does with an edge, it will not have
    // the reference's stair-steps, and the blurrier candidate sits closer to
    // their average. TAAU converges towards the antialiased image, so that is
    // the image it has to be measured against.
    const bool captureGt = !options.captureGtDir.empty();
    // Scoring an upscaled frame needs the same reference the ground-truth
    // capture writes out, so both modes share one buffer.
    const bool needGtBuffer = captureGt || options.validateUpscale || options.validateFg;
    const int gtSs = std::clamp(options.gtSupersample, 1, 4);
    renderer::GBuffer gtBuffer;
    std::filesystem::path gtDir;
    std::ofstream gtManifest;
    // Two frames of warm-up: the midpoint reference needs a previous frame to
    // sit between, and the first frame's history is a jump cut.
    constexpr long long kGtWarmup = 2;
    if (needGtBuffer) {
        gtBuffer.create(displayWidth * gtSs, displayHeight * gtSs);
        gtBuffer.setPatternFilter(gbuffer.patternFilter());
    }
    if (captureGt) {
        gtDir = options.captureGtDir;
        std::filesystem::create_directories(gtDir);
        gtManifest.open(gtDir / "manifest.csv");
        gtManifest << "index,time_s,mid_time_s,render_res,display_res\n";
        gtManifest << std::fixed << std::setprecision(6);
    }

    int debugMode = options.debugView;

    // M3: spatial baselines. The G-buffer is rendered at render resolution,
    // tone mapped to display-referred [0,1] (FSR1 is defined there, not in
    // linear HDR), upscaled, and presented without a second tone map.
    const bool useUpscaler = !options.upscaler.empty() || options.validateUpscale;
    upscale::Mode upscaleMode = upscale::Mode::None;
    if (!options.upscaler.empty()) {
        upscaleMode = upscale::modeFromName(options.upscaler);
        if (upscaleMode == upscale::Mode::Count) {
            std::fprintf(stderr,
                         "[app] unknown upscaler '%s' "
                         "(nearest|bilinear|bicubic|fsr1|taau|taau-rcas)\n",
                         options.upscaler.c_str());
            return 1;
        }
    }
    upscale::Upscaler upscaler;
    upscaler.create();
    upscaler.setMaxAccumFrames(options.taauFrames);
    upscaler.setClampGamma(options.taauGamma);
    upscaler.setMotionDecay(options.taauMotionDecay);
    upscaler.setKernel(options.taauKernel);
    upscaler.setLanczosScale(options.fsrLanczos, options.fsrLanczosStill);
    upscaler.setDepthTolerance(options.fsrDepthTolerance);
    upscaler.setDisocclusionStrength(options.fsrDisocclusion);
    upscaler.setDilate(options.fsrDilate != 0);
    upscaler.setLocks(options.fsrLockLife, options.fsrLockRelax, options.fsrLockTolerance,
                      options.fsrLockContrast);
    // M6. Runs on whatever display-referred image the pipeline ends with: the
    // upscaled output when there is one, the tone-mapped render-resolution
    // frame otherwise. FSR3 estimates flow on the presented image for the same
    // reason -- the motion it exists to catch (shadows, reflections, shading,
    // animated textures) is only visible after shading, and the game's own
    // vectors describe none of it.
    opticalflow::OpticalFlow flow;
    if (options.opticalFlow) {
        opticalflow::Options flowOptions;
        flowOptions.levels = options.ofLevels;
        flowOptions.radius = options.ofRadius;
        flowOptions.smoothness = options.ofSmoothness;
        flowOptions.filter = options.ofFilter != 0;
        flowOptions.upscale = options.ofUpscale != 0;
        flowOptions.temporal = options.ofTemporal != 0;
        flowOptions.novelty = options.ofNovelty;
        flowOptions.sceneChange = options.ofSceneChange != 0;
        flowOptions.sceneChangeThreshold = options.ofSceneThreshold;
        flowOptions.sceneChangeStatistic = options.ofSceneStatistic;
        flow.setOptions(flowOptions);
        flow.create();
    }

    // M7. Sits at the end of the chain: it interpolates between two frames the
    // upscaler already produced, so everything the upscaler does to an image --
    // reconstruction, accumulation, sharpening -- is inside the frames it is
    // given, and none of it has to be repeated.
    framegen::FrameGenerator frameGen;
    if (options.frameGen) {
        framegen::Options fgOptions;
        fgOptions.depthTolerance = options.fgDepth;
        fgOptions.dilate = options.fgDilate != 0;
        fgOptions.gameField = options.fgGame != 0;
        fgOptions.flowField = options.fgFlow != 0;
        fgOptions.masks = options.fgMasks != 0;
        fgOptions.levels = options.fgLevels;
        fgOptions.inpaintPick = options.fgInpaintPick;
        fgOptions.agreement = options.fgAgreement;
        fgOptions.colorPriority = options.fgColorPriority != 0;
        fgOptions.flowBias = options.fgFlowBias;
        fgOptions.flowErrorThreshold = options.fgFlowError;
        fgOptions.flowMagnitudeScale = options.fgFlowMagnitude;
        fgOptions.measureBlend = options.validateFg;
        fgOptions.imageInpaint = options.fgInpaint != 0;
        fgOptions.boundsCheck = options.fgBounds != 0;
        fgOptions.inpaintMinCoverage = options.fgInpaintCoverage;
        fgOptions.coverageMasks = options.fgCoverageMasks != 0;
        fgOptions.mlWeights = options.fgMl;
        fgOptions.mlDump = !options.mlDump.empty();
        frameGen.setOptions(fgOptions);
        frameGen.create();
    }

    const bool temporalUpscaler = upscale::isTemporal(upscaleMode);
    // Mode-dependent sharpening default; an explicit --sharpness overrides it.
    // The three paths want three different things, and the reason is the same
    // one each time -- how much real detail the pass before RCAS left out:
    //   FSR1  0.25  EASU's own recommendation.
    //   TAAU  1.2   four times gentler, and still worth +1 dB, because RCAS is
    //               putting back the Nyquist the history resample removed.
    //   FSR   2.0   gentler again. M5 fixed the deficiency TAAU's RCAS was
    //               compensating for, so sharpening now only adds error;
    //               measured, its PSNR optimum is "off". See docs/FSR.md.
    {
        float sharpness = options.sharpness;
        if (std::isnan(sharpness))
            sharpness = upscale::isFullUpscaler(upscaleMode) ? 2.0f
                        : temporalUpscaler                   ? 1.2f
                                                             : 0.25f;
        upscaler.setSharpness(sharpness);
    }

    // Texture LOD bias. Mip selection is driven by the *render* resolution, so a
    // 720p pass feeding a 1080p output picks mips band-limited to 720p and the
    // upscaler is handed an image the missing frequencies were already filtered
    // out of. No amount of temporal accumulation brings them back: averaging
    // jittered samples of a band-limited signal returns the same band-limited
    // signal. Biasing selection towards the output resolution puts that detail
    // back into the render target, aliased, for the upscaler to resolve.
    //
    // log2(render/display) is what FSR1 asks for - enough to sharpen the input
    // to what the output can show. FSR2 takes one extra level on top, because a
    // temporal upscaler can resolve aliasing a spatial one can only smear, and
    // trading a little more of it for detail is a win it alone can cash in.
    // Each mode therefore gets its vendor-specified value; --mip-bias overrides.
    auto applyMipBias = [&]() {
        float bias = options.mipBias;
        if (std::isnan(bias)) {
            bias = 0.f;
            if (useUpscaler && renderWidth < displayWidth) {
                bias = std::log2(static_cast<float>(renderWidth) /
                                 static_cast<float>(displayWidth));
                if (temporalUpscaler) bias -= 1.f;
            }
        }
        gbuffer.setMipBias(bias);
    };
    applyMipBias();
    // The reference render is at its own native resolution: biasing it would
    // move the target instead of the candidate.
    if (needGtBuffer) gtBuffer.setMipBias(0.f);
    gbuffer.setReactiveScale(upscale::isFullUpscaler(upscaleMode) ? options.fsrReactive : 0.f);
    gfx::Texture2D ldrInput;   // render resolution, display-referred
    gfx::Texture2D gtLinear;   // display resolution reference, linear HDR
    gfx::Texture2D gtLdr;      // ... and the same reference, display-referred
    gfx::Texture2D gtMidLdr;   // the real frame at t-dt/2, display-referred
    // M8. The upscaled frame with the HUD baked in (--ui baked), and the
    // HUD-composed images the validation compares when a HUD is on screen.
    gfx::Texture2D bakedFrame;
    gfx::Texture2D gtMidUi, fgUi, blendUi;
    gfx::Texture2D hudCandidate, hudReference, hudBlend;
    // Synthetic load: a second render-resolution G-buffer, so the extra
    // renders never touch the real one's history.
    renderer::GBuffer loadBuffer;
    bool loadCreated = false;
    int loadCount = std::max(options.load, 0);
    auto ensureLoadBuffer = [&] {
        if (loadCount <= 0) return;
        if (!loadCreated) {
            loadBuffer.create(renderWidth, renderHeight);
            loadCreated = true;
        } else {
            loadBuffer.resize(renderWidth, renderHeight);
        }
    };
    ensureLoadBuffer();
    // Last frame's upscaled output and last frame's reference, kept only so the
    // temporal stability metric has something to reproject. See the block that
    // fills them for what the number means.
    gfx::Texture2D prevOutput;
    gfx::Texture2D prevGtLdr;
    bool stabilityHistoryValid = false;

    // Tone maps and debug-visualises one G-buffer into the display-resolution
    // present target. `srcWidth`/`srcHeight` are the source's own resolution,
    // which the debug views need to size their texel steps.
    auto presentTo = [&](const renderer::GBuffer& src, int srcWidth, int srcHeight) {
        presentFbo.bind();
        glViewport(0, 0, displayWidth, displayHeight);
        glDisable(GL_DEPTH_TEST);
        presentProgram.bind();
        src.color().bindTexture(0);
        src.depth().bindTexture(1);
        src.velocity().bindTexture(2);
        src.prevColor().bindTexture(3);
        if (upscaler.dilated().valid()) upscaler.dilated().bindTexture(4);
        if (flow.valid()) {
            flow.flow().bindTexture(5);
            presentProgram.set("uFlowGrid", static_cast<float>(flow.blockGridWidth()),
                               static_cast<float>(flow.blockGridHeight()));
        }
        // A vector of this many display pixels saturates the debug colour.
        // Displacement, in display pixels, that saturates the HSV value
        // channel in the flow debug views. The right number depends entirely on
        // how fast the camera is moving, so it is an argument rather than a
        // constant.
        presentProgram.set("uFlowScale", options.flowScale);
        presentProgram.set("uDisplaySize", static_cast<float>(displayWidth),
                           static_cast<float>(displayHeight));
        presentProgram.set("uMode", debugMode);
        presentProgram.set("uNearPlane", camera.nearPlane());
        presentProgram.set("uRenderSize", static_cast<float>(srcWidth),
                           static_cast<float>(srcHeight));
        presentProgram.set("uExposure", 1.0f);
        presentProgram.set("uMvScale", 8.0f);
        // The G-buffer is linear HDR, so this path still tone maps.
        presentProgram.set("uToneMap", 1);
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    };

    // Blits one display-resolution texture to the present target. Upscaler
    // output is already display-referred and must not be tone mapped a second
    // time; the downsampled reference is still linear HDR and must be.
    auto presentTexture = [&](const gfx::Texture2D& tex, int toneMap) {
        presentFbo.bind();
        glViewport(0, 0, displayWidth, displayHeight);
        glDisable(GL_DEPTH_TEST);
        presentProgram.bind();
        tex.bindTexture(0);
        presentProgram.set("uMode", 0);
        presentProgram.set("uToneMap", toneMap);
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    };

    // Source -> target, with the HUD layer over it and optionally the tear-line
    // strip. The program picks the target format.
    auto compose = [&](gfx::Program& program, const gfx::Texture2D& source,
                       const gfx::Texture2D& target, bool ui, int tearIndex, bool interpolated) {
        program.bind();
        source.bindTexture(0);
        program.set("uSource", 0);
        if (hud.layer().valid()) hud.layer().bindTexture(1);
        program.set("uLayer", 1);
        target.bindImage(0, GL_WRITE_ONLY);
        program.set("uDisplaySize", displayWidth, displayHeight);
        program.set("uUi", ui && hud.layer().valid() ? 1 : 0);
        program.set("uTearIndex", tearIndex);
        program.set("uInterpolated", interpolated ? 1 : 0);
        program.dispatch(displayWidth, displayHeight);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    };
    // Crops the HUD panel out of a display-resolution RGBA16F image.
    auto cropHud = [&](const gfx::Texture2D& source, gfx::Texture2D& crop, const char* label) {
        crop.ensure(hud.width(), hud.height(), GL_RGBA16F, 1, label);
        glCopyImageSubData(source.id, GL_TEXTURE_2D, 0, hud.rectX(), hud.rectY(), 0, crop.id,
                           GL_TEXTURE_2D, 0, 0, 0, 0, hud.width(), hud.height(), 1);
    };
    auto writeTexture = [&](const gfx::Texture2D& tex, const std::string& path) {
        GLuint fbo = 0;
        glCreateFramebuffers(1, &fbo);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex.id, 0);
        writeImage(fbo, tex.width, tex.height, path);
        glDeleteFramebuffers(1, &fbo);
    };

    // A flush only hands the work to the kernel, which queues GPU jobs in
    // submission order: the render thread submits a whole frame in about a
    // millisecond and a blit from the presenter still waits behind all of it.
    // Finishing each stage keeps the render thread at most one stage ahead of
    // the GPU -- what a fence would do, which this driver does not have.
    auto stageFlush = [&] {
        if (options.gpuFlush == 1) glFlush();
        else if (options.gpuFlush >= 2) glFinish();
    };

    gfx::GpuTimer timer;
    // Per-frame CSV so a results table for the thesis is one command away
    // instead of a manual transcription of console output.
    // The first frames carry no usable numbers: the timestamp ring needs three
    // frames before totalMs() is anything but zero, and reprojection needs a
    // previous frame to reproject. Rows before that would be fabricated zeros,
    // so the table starts once every column is real.
    const long long csvFirstFrame =
        std::max<long long>(options.warmupFrames, options.validateMv ? 3 : 2);
    std::ofstream csv;
    if (!options.csvPath.empty()) {
        csv.open(options.csvPath);
        csv << "frame,time_s,cpu_ms,gpu_ms";
        if (options.validateMv)
            csv << ",psnr_reproj,ssim_reproj,psnr_direct,ssim_direct,coverage";
        if (options.validateUpscale)
            csv << ",psnr_upscale,ssim_upscale,tstab_upscale,tstab_ref";
        if (options.validateFlow)
            csv << ",epe_px,epe_within1,epe_within2,scene_change,sc_max,sc_mean,sc_median,cut";
        if (options.validateFg)
            csv << ",psnr_fg,ssim_fg,psnr_blend,ssim_blend";
        if (options.validateFg && uiMode != UiMode::None)
            csv << ",psnr_hud,ssim_hud,psnr_hud_blend,ssim_hud_blend";
        csv << '\n';
        csv << std::fixed << std::setprecision(6);
    }

    metrics::ImageMetrics imageMetrics;
    imageMetrics.create();
    imageMetrics.setComputeSsim(options.validateMv || options.validateUpscale ||
                                options.validateFg);
    double mvReprojectedSsimSum = 0.0, mvStaticSsimSum = 0.0;
    double mvWorstSsim = 1.0;
    metrics::CompareResult lastReprojected, lastDirect;
    double mvReprojectedPsnrSum = 0.0, mvStaticPsnrSum = 0.0;
    int mvSamples = 0;
    metrics::CompareResult lastUpscale;
    double upscalePsnrSum = 0.0, upscaleSsimSum = 0.0, upscaleWorstSsim = 1.0;
    int upscaleSamples = 0;
    double lastStability = 0.0, lastStabilityRef = 0.0;
    double stabilitySum = 0.0, stabilityRefSum = 0.0;
    int stabilitySamples = 0;
    metrics::CompareResult lastFg, lastFgBlend;
    double fgPsnrSum = 0.0, fgSsimSum = 0.0, fgBlendPsnrSum = 0.0, fgBlendSsimSum = 0.0;
    double fgWorstPsnr = 1e9;
    int fgSamples = 0;
    metrics::CompareResult lastHud, lastHudBlend;
    double hudPsnrSum = 0.0, hudSsimSum = 0.0, hudBlendPsnrSum = 0.0, hudBlendSsimSum = 0.0;
    opticalflow::Accuracy lastFlow;
    double flowEpeSum = 0.0, flowWithin1Sum = 0.0, flowWithin2Sum = 0.0;
    double flowWorstEpe = 0.0;
    int flowSamples = 0;
    camera.setPathRadius(options.pathRadius);
    camera.setPathPhase(options.pathPhase);
    camera.setScripted(options.scripted);

    bool running = true;
    long long frameCounter = 0;
    bool cutHistory[3] = {false, false, false};
    bool paused = false;
    double sceneTime = options.timeOffset;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    double statTimer = 0.0;
    double reloadTimer = 0.0;
    int framesSinceStat = 0;
    double cpuMsAccum = 0.0;
    // Runtime switch for frame generation. Turning it back on restarts the
    // module's history, which by then is a stale frame.
    bool fgActive = options.frameGen;
    bool fgResume = false;
    long long presentedCounter = 0;
    int shotCounter = 0;
    bool shotRequested = false;

    std::printf(
        "[app] keys: 1-9 debug views | 0 interpolated frame | J jitter | P scripted path | V vsync | "
        "F1-F5 scaling mode | SPACE pause | F12 screenshot | ESC quit\n"
        "[app]       G frame generation | Y pacing | H HUD | T tear lines | [ ] synthetic load\n");

    while (running) {
        const Uint64 nowCounter = SDL_GetPerformanceCounter();
        const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
        double dt = static_cast<double>(nowCounter - previousCounter) / frequency;
        previousCounter = nowCounter;
        dt = std::min(dt, 0.1);  // clamp after a stall so nothing teleports
        // Lockstep mode: scene time advances by a fixed step regardless of how
        // long the frame took, so a capture is bit-identical across runs.
        if (options.fixedDeltaTime > 0.0) dt = options.fixedDeltaTime;

        SDL_Event event;
        // A measurement run (--frames) takes no input. The window still gets
        // focus like any other, and a stray key reaching it is not harmless: a
        // space bar pauses scene time, and every frame after that measures a
        // frozen camera under a configuration name that says otherwise. That
        // happened once, silently, in an ablation row; this is why.
        const bool acceptInput = !measurementRun;
        const bool lastFrame =
            (options.frameLimit > 0 && frameCounter + 1 >= options.frameLimit) ||
            (options.runSeconds > 0.0 &&
             present::FramePacer::nowMs() - runStartMs >= options.runSeconds * 1000.0);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (!acceptInput) continue;
            camera.handleEvent(event);
            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                 event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
                // Display resolution is a configuration value, not a window
                // property: resizing only changes how the result is scaled.
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_1: debugMode = 0; break;
                    case SDLK_2: debugMode = 1; break;
                    case SDLK_3: debugMode = 2; break;
                    case SDLK_4: debugMode = 3; break;
                    case SDLK_5: debugMode = 4; break;
                    case SDLK_6: debugMode = 5; break;
                    case SDLK_7: debugMode = 6; break;
                    case SDLK_8: debugMode = 7; break;
                    case SDLK_9: debugMode = 8; break;
                    // M7 views: the interpolated frame itself, and what the
                    // module thought while making it.
                    // 9 the interpolated frame, 10 its masks, 11 the learned
                    // blend's weight view when a network is loaded.
                    case SDLK_0: {
                        const int last = frameGen.ml().networkLoaded() ? 11 : 10;
                        debugMode = debugMode >= 9 && debugMode < last ? debugMode + 1 : 9;
                        break;
                    }
                    case SDLK_m:
                        if (frameGen.ml().networkLoaded()) {
                            frameGen.setMlEnabled(!frameGen.mlEnabled());
                            std::printf("[app] blend: %s\n", frameGen.mlEnabled() ? "learned" : "heuristic");
                        }
                        break;
                    case SDLK_f:
                        gbuffer.setPatternFilter(!gbuffer.patternFilter());
                        std::printf("[app] pattern filter %s\n", gbuffer.patternFilter() ? "on" : "off");
                        break;
                    case SDLK_j:
                        options.jitter = !options.jitter;
                        std::printf("[app] jitter %s\n", options.jitter ? "on" : "off");
                        break;
                    case SDLK_p:
                        camera.setScripted(!camera.scripted());
                        camera.requestJumpCut();
                        std::printf("[app] camera: %s\n", camera.scripted() ? "scripted" : "free");
                        break;
                    case SDLK_v:
                        options.vsync = !options.vsync;
                        pacer.setVsync(options.vsync);
                        break;
                    case SDLK_g:
                        if (options.frameGen) {
                            fgActive = !fgActive;
                            fgResume = fgActive;
                            std::printf("[app] frame generation %s\n", fgActive ? "on" : "off");
                        }
                        break;
                    case SDLK_y:
                        pacer.setPacing(pacer.pacing() == present::PacingMode::Paced
                                            ? present::PacingMode::Immediate
                                            : present::PacingMode::Paced);
                        break;
                    case SDLK_h: hudVisible = !hudVisible; break;
                    case SDLK_t: tearLines = !tearLines; break;
                    case SDLK_LEFTBRACKET:
                        loadCount = std::max(loadCount - 1, 0);
                        std::printf("[app] synthetic load x%d\n", loadCount);
                        break;
                    case SDLK_RIGHTBRACKET:
                        ++loadCount;
                        ensureLoadBuffer();
                        std::printf("[app] synthetic load x%d\n", loadCount);
                        break;
                    case SDLK_SPACE: paused = !paused; break;
                    case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4: case SDLK_F5: {
                        scaleMode = 1 + (event.key.keysym.sym - SDLK_F1);
                        scaleFactor = kScaleFactors[scaleMode];
                        refreshScaleLabel();
                        renderSize(renderWidth, renderHeight);
                        gbuffer.resize(renderWidth, renderHeight);
                        if (loadCreated) loadBuffer.resize(renderWidth, renderHeight);
                        applyMipBias();
                        camera.setResolution(renderWidth, renderHeight, displayWidth, displayHeight);
                        camera.requestJumpCut();
                        std::printf("[app] %s -> %dx%d\n", scaleLabel, renderWidth,
                                    renderHeight);
                        break;
                    }
                    case SDLK_F12: shotRequested = true; break;
                    default: break;
                }
            }
        }

        if (!paused) sceneTime += dt;

        // Shader hot reload, throttled so we do not stat files every frame.
        reloadTimer += dt;
        if (reloadTimer > 0.5) {
            reloadTimer = 0.0;
            presentProgram.reloadIfChanged();
            gbuffer.reloadShaders();
            upscaler.reloadShaders();
            if (options.opticalFlow) flow.reloadShaders();
            if (options.frameGen) frameGen.reloadShaders();
            hud.reloadShaders();
            composeProgram.reloadIfChanged();
            composeHdrProgram.reloadIfChanged();
        }

        const Uint64 cpuStart = SDL_GetPerformanceCounter();

        // The scene-change readback is two frames behind the GPU, so the CSV
        // has to say whether a cut happened on the frame the readback is
        // describing, not on this one.
        cutHistory[2] = cutHistory[1];
        cutHistory[1] = cutHistory[0];

        if (options.cutEvery > 0 && frameCounter > 0 &&
            frameCounter % options.cutEvery == 0) {
            // Half an orbit away, looking at the other side of the atrium:
            // nothing in the new image was in the old one.
            options.pathPhase += 3.14159265f;
            camera.setPathPhase(options.pathPhase);
            camera.requestJumpCut();
        }
        cutHistory[0] = camera.jumpCut();

        // Read before the camera update clears it: a jump cut is the one
        // event that makes every pixel of history worthless.
        const bool cameraCut = camera.jumpCut();

        // Jitter is only useful to something that can accumulate it. A
        // spatial upscaler has no history to put the sub-pixel offsets into,
        // so for it the pattern is pure noise; for TAAU it is the entire
        // source of the extra samples.
        camera.beginFrame(options.jitter && (!useUpscaler || temporalUpscaler));
        if (camera.scripted())
            camera.applyScriptedPath(sceneTime);
        else
            camera.update(static_cast<float>(dt), SDL_GetKeyboardState(nullptr));
        scene.update(sceneTime);
        // Latency is measured from here: the moment this frame's input and
        // simulation state are fixed. What the display and the compositor add
        // after the swap is not visible to the application and is not in it.
        const double sampleMs = present::FramePacer::nowMs();

        timer.beginFrame();
        gbuffer.render(scene, camera, timer);
        stageFlush();
        if (loadCount > 0 && loadCreated) {
            gfx::GpuScope scope(timer, "Synthetic load");
            for (int i = 0; i < loadCount; ++i) {
                loadBuffer.render(scene, camera, timer, false, "load G-buffer");
                stageFlush();
            }
        }

        // M8: the HUD. Rendered before anything reads it, because in the baked
        // mode frame generation does. In a validation run it is static -- a
        // counter that changes between the two frames would be a legitimate
        // difference the interpolation is then scored against.
        const bool uiOn = uiMode != UiMode::None && hudVisible;
        if (uiOn) {
            if (options.validateFg) {
                if (frameCounter == 0) {
                    hud.setLines({"fsr3lite M8 | HUD validation (static)",
                                  "FG on | pacing paced | vsync off | ui fixed",
                                  "presented  120.0 fps   rendered   60.0 fps",
                                  "frame  16.67 ms   GPU   4.20 ms   load x0",
                                  "latency  21.3 ms  sample -> real present",
                                  "graph: present intervals, line = target",
                                  "H hud  T tear lines  G frame gen  Y pacing"});
                    std::vector<float> graph(present::Hud::kGraphSamples);
                    for (size_t i = 0; i < graph.size(); ++i)
                        graph[i] = 8.333f + 3.0f * std::sin(0.37f * static_cast<float>(i)) +
                                   (i % 37 == 0 ? 12.0f : 0.0f);
                    hud.setGraph(graph, 8.333f);
                }
            } else {
                const bool showingFg = fgActive && options.presentFg != 0 && useUpscaler;
                const double renderFps = pacer.renderFps();
                const double frameMs = renderFps > 0.0 ? 1000.0 / renderFps : 0.0;
                char lines[present::Hud::kRows][96];
                std::snprintf(lines[0], 96, "fsr3lite | %s %dx%d -> %dx%d", scaleLabel, renderWidth,
                              renderHeight, displayWidth, displayHeight);
                std::snprintf(lines[1], 96, "FG %s | pacing %s | vsync %s | ui %s",
                              !options.frameGen ? "n/a" : showingFg ? "on" : "off",
                              pacer.pacing() == present::PacingMode::Paced ? "paced" : "immediate",
                              options.vsync ? "on" : "off",
                              uiMode == UiMode::Baked ? "baked" : "composite");
                std::snprintf(lines[2], 96, "presented %6.1f fps   rendered %6.1f fps",
                              pacer.presentedFps(), renderFps);
                std::snprintf(lines[3], 96, "frame %6.2f ms   GPU %6.2f ms   load x%d", frameMs,
                              timer.totalMs(), loadCount);
                std::snprintf(lines[4], 96, "latency %5.1f ms  sample -> real present",
                              pacer.latencyMs());
                if (frameGen.ml().networkLoaded())
                    std::snprintf(lines[5], 96, "blend %s (c%d-c%d)  0: views 9-11",
                                  frameGen.mlEnabled() ? "learned" : "heuristic", frameGen.ml().net().c0,
                                  frameGen.ml().net().c1);
                else
                    std::snprintf(lines[5], 96, "graph: present intervals, line = target");
                std::snprintf(lines[6], 96, "H hud  T tear  G frame gen  Y pacing  M blend");
                hud.setLines(std::vector<std::string>(lines, lines + present::Hud::kRows));
                static std::vector<float> intervals;
                pacer.recentIntervals(intervals);
                hud.setGraph(intervals, static_cast<float>(showingFg ? 0.5 * frameMs : frameMs));
            }
            hud.render(displayWidth, displayHeight, timer);
        }

        // Motion vector validation: reprojecting the previous frame by the
        // velocity buffer must match the current frame far better than simply
        // sampling the same pixel. If it does not, the velocity buffer is
        // wrong and every temporal pass built on it will be too.
        if (options.validateMv && frameCounter > 2) {
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
            const metrics::CompareResult reprojected =
                imageMetrics.compare(gbuffer.color(), gbuffer.prevColor(),
                                     metrics::ImageMetrics::Mode::Reprojected, &gbuffer.velocity());
            const metrics::CompareResult direct =
                imageMetrics.compare(gbuffer.color(), gbuffer.prevColor(),
                                     metrics::ImageMetrics::Mode::Direct);
            mvReprojectedPsnrSum += reprojected.psnr;
            mvStaticPsnrSum += direct.psnr;
            mvReprojectedSsimSum += reprojected.ssim;
            mvStaticSsimSum += direct.ssim;
            mvWorstSsim = std::min(mvWorstSsim, reprojected.minSsim);
            ++mvSamples;
            lastReprojected = reprojected;
            lastDirect = direct;
        }

        // M3: tone map to display-referred space, then upscale. Doing it in
        // this order is not a detail: EASU's edge detector and RCAS' limiter
        // both assume values in [0,1] with perceptual spacing.
        if (useUpscaler) {
            upscaler.tonemap(gbuffer.color(), ldrInput, timer, "Tonemap");
            upscale::TemporalInputs temporal;
            temporal.velocity = &gbuffer.velocity();
            temporal.depthInfo = &gbuffer.depthInfo();
            temporal.prevDepthInfo = &gbuffer.prevDepthInfo();
            temporal.reactive = &gbuffer.reactive();
            temporal.jitterX = camera.jitter().x;
            temporal.jitterY = camera.jitter().y;
            temporal.reset = cameraCut;
            upscaler.dispatch(ldrInput, displayWidth, displayHeight, upscaleMode, timer, temporal);
            stageFlush();
            // The baked HUD goes into a copy: the upscaler's history must stay
            // clean, exactly as a game that draws its UI after upscaling keeps
            // it -- the question here is only what frame generation sees.
            if (uiMode == UiMode::Baked) {
                gfx::GpuScope scope(timer, "HUD bake");
                bakedFrame.ensure(displayWidth, displayHeight, GL_RGBA16F, 1, "ui.baked");
                compose(composeHdrProgram, upscaler.output(), bakedFrame, uiOn, -1, false);
            }
        } else if (options.opticalFlow) {
            // No upscaler, but the flow still needs a display-referred image.
            upscaler.tonemap(gbuffer.color(), ldrInput, timer, "Tonemap");
        }

        // What reaches the screen as the real frame, and what frame generation
        // interpolates between.
        const gfx::Texture2D& finalFrame =
            uiMode == UiMode::Baked && useUpscaler ? bakedFrame : upscaler.output();
        const bool fgReset = cameraCut || fgResume;
        fgResume = false;

        if (options.opticalFlow && (fgActive || !options.frameGen)) {
            const gfx::Texture2D& flowInput = useUpscaler ? finalFrame : ldrInput;
            flow.dispatch(flowInput, timer, fgReset);
            stageFlush();
        }

        // M7: the frame between this one and the last one. It is produced every
        // frame here, measured, and then thrown away -- putting it on the
        // screen at the right moment is frame pacing, which is M8.
        if (options.frameGen && fgActive) {
            framegen::Inputs fgInputs;
            fgInputs.current = &finalFrame;
            // The dilated buffer only exists for the M5 modes. Without it the
            // scatter falls back to the raw velocity buffer, which is the
            // ablation that shows what dilation is worth on a silhouette.
            fgInputs.dilated = upscaler.dilated().valid() ? &upscaler.dilated() : nullptr;
            fgInputs.velocity = &gbuffer.velocity();
            fgInputs.depthInfo = &gbuffer.depthInfo();
            fgInputs.prevDepthInfo = &gbuffer.prevDepthInfo();
            fgInputs.renderWidth = renderWidth;
            fgInputs.renderHeight = renderHeight;
            fgInputs.flow = flow.valid() ? &flow.flow() : nullptr;
            fgInputs.reset = fgReset;
            const bool fgShot = !options.fgShots.empty() &&
                                std::find(options.fgShotFrames.begin(), options.fgShotFrames.end(),
                                          frameCounter) != options.fgShotFrames.end();
            frameGen.ml().setWeightView(debugMode == 11 || fgShot);
            frameGen.dispatch(fgInputs, timer);
            stageFlush();
        }

        // Score the interpolated frame against the frame that instant really
        // looks like. The reference is rendered the same way the ground-truth
        // capture renders its midpoints -- scene and camera moved to
        // sceneTime - dt/2 with history recording off, no jitter, supersampled
        // and resolved -- so the number is comparable with the ones in
        // docs/GROUND_TRUTH.md.
        //
        // The second number is the point of the first: the 50/50 blend of the
        // same two frames, scored against the same reference. Frame generation
        // is only worth its cost if it beats the image you get for free, and
        // on slow content that image is not bad at all.
        //
        // Cuts are excluded for the same reason the flow validation excludes
        // them: across a cut there is no motion to interpolate along, the
        // module says so by falling back to a copy, and scoring that copy
        // against a frame from a different part of the scene measures the
        // scene, not the module.
        if (options.validateFg && frameCounter >= kGtWarmup && !cameraCut && fgActive &&
            frameGen.valid()) {
            const double midTime = sceneTime - 0.5 * dt;
            renderer::Camera midCamera = camera;
            midCamera.applyScriptedPath(midTime);
            scene.update(midTime, false);
            gtBuffer.render(scene, midCamera, timer, false, "ref: GT midpoint");
            upscaler.downsample(gtBuffer.color(), gtLinear, gtSs, timer, "ref: GT downsample");
            upscaler.tonemap(gtLinear, gtMidLdr, timer, "ref: GT tonemap");
            scene.update(sceneTime, false);
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

            // With a HUD on screen the reference is the real midpoint frame
            // with the HUD composed over it: that is what a correct pipeline
            // shows. Composite mode puts the same HUD over the interpolated
            // frame; baked mode has it inside already, warped.
            const gfx::Texture2D* candidate = &frameGen.output();
            const gfx::Texture2D* blendCandidate = &frameGen.blend();
            const gfx::Texture2D* reference = &gtMidLdr;
            if (uiOn) {
                gfx::GpuScope scope(timer, "ref: HUD compose");
                gtMidUi.ensure(displayWidth, displayHeight, GL_RGBA16F, 1, "ui.gtMid");
                compose(composeHdrProgram, gtMidLdr, gtMidUi, true, -1, false);
                reference = &gtMidUi;
                if (uiMode == UiMode::Composite) {
                    fgUi.ensure(displayWidth, displayHeight, GL_RGBA16F, 1, "ui.fg");
                    blendUi.ensure(displayWidth, displayHeight, GL_RGBA16F, 1, "ui.blend");
                    compose(composeHdrProgram, frameGen.output(), fgUi, true, -1, false);
                    compose(composeHdrProgram, frameGen.blend(), blendUi, true, -1, false);
                    candidate = &fgUi;
                    blendCandidate = &blendUi;
                }
            }
            lastFg = imageMetrics.compare(*candidate, *reference,
                                          metrics::ImageMetrics::Mode::Direct, nullptr,
                                          /*tonemap=*/false);
            lastFgBlend = imageMetrics.compare(*blendCandidate, *reference,
                                               metrics::ImageMetrics::Mode::Direct, nullptr,
                                               /*tonemap=*/false);
            if (uiOn) {
                cropHud(*candidate, hudCandidate, "ui.hudCandidate");
                cropHud(*blendCandidate, hudBlend, "ui.hudBlend");
                cropHud(*reference, hudReference, "ui.hudReference");
                lastHud = imageMetrics.compare(hudCandidate, hudReference,
                                               metrics::ImageMetrics::Mode::Direct, nullptr, false);
                lastHudBlend = imageMetrics.compare(hudBlend, hudReference,
                                                    metrics::ImageMetrics::Mode::Direct, nullptr,
                                                    false);
                hudPsnrSum += lastHud.psnr;
                hudSsimSum += lastHud.ssim;
                hudBlendPsnrSum += lastHudBlend.psnr;
                hudBlendSsimSum += lastHudBlend.ssim;
            }
            fgPsnrSum += lastFg.psnr;
            fgSsimSum += lastFg.ssim;
            fgBlendPsnrSum += lastFgBlend.psnr;
            fgBlendSsimSum += lastFgBlend.ssim;
            fgWorstPsnr = std::min(fgWorstPsnr, lastFg.psnr);
            ++fgSamples;
            if (mlDumpFile && uiMode == UiMode::None && frameCounter >= options.warmupFrames)
                frameGen.ml().dumpPatches(mlDumpFile.get(), frameCounter, frameGen.heuristicOutput(),
                                          gtMidLdr, options.mlPatchSize, options.mlPatches, mlRng,
                                          !options.fgMl.empty());
            if (!options.fgShots.empty() &&
                std::find(options.fgShotFrames.begin(), options.fgShotFrames.end(), frameCounter) !=
                    options.fgShotFrames.end()) {
                const std::string base = options.fgShots + "/frame" + std::to_string(frameCounter);
                writeTexture(*reference, base + "_reference.png");
                writeTexture(frameGen.blend(), base + "_blend.png");
                writeTexture(frameGen.heuristicOutput(), base + "_heuristic.png");
                if (frameGen.ml().networkLoaded() && frameGen.mlEnabled()) {
                    writeTexture(frameGen.ml().output(), base + "_learned.png");
                    writeTexture(frameGen.ml().weights(), base + "_weights.png");
                }
            }
        }

        // Score the upscaled frame against the same instant rendered natively
        // at display resolution. Both sides go through the identical tone map
        // shader, so the only difference the metric can see is resampling.
        if (options.validateUpscale && frameCounter >= kGtWarmup) {
            gtBuffer.render(scene, camera, timer, false, "ref: GT native");
            // Average in linear HDR, then tone map: that is the order an SSAA
            // renderer resolves in, and the only radiometrically correct one.
            upscaler.downsample(gtBuffer.color(), gtLinear, gtSs, timer, "ref: GT downsample");
            upscaler.tonemap(gtLinear, gtLdr, timer, "ref: GT tonemap");
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
            lastUpscale = imageMetrics.compare(upscaler.output(), gtLdr,
                                               metrics::ImageMetrics::Mode::Direct, nullptr,
                                               /*tonemap=*/false);
            upscalePsnrSum += lastUpscale.psnr;
            upscaleSsimSum += lastUpscale.ssim;
            upscaleWorstSsim = std::min(upscaleWorstSsim, lastUpscale.minSsim);
            ++upscaleSamples;

            // Temporal stability. PSNR against the reference says how close a
            // single frame is; it says nothing about whether the *sequence*
            // crawls, and a crawling edge is the artefact a temporal upscaler
            // exists to remove. So: reproject the previous output by this
            // frame's motion vectors and measure how much it still differs.
            // Everything motion explains cancels, and what is left is flicker.
            //
            // The number is only meaningful next to the same measurement on the
            // reference sequence, which is not zero either -- shading changes,
            // disocclusions and the reference's own resampling all survive
            // reprojection. A candidate near the reference is as stable as the
            // content allows; far above it is over-smoothed, far below it
            // shimmers.
            //
            // The velocity buffer is at render resolution and gets filtered on
            // the way up, which is wrong at silhouettes -- but wrong identically
            // for every candidate, so the comparison stays fair.
            if (stabilityHistoryValid) {
                const bool ssimWas = true;
                imageMetrics.setComputeSsim(false);
                lastStability = imageMetrics.compare(upscaler.output(), prevOutput,
                                                     metrics::ImageMetrics::Mode::Reprojected,
                                                     &gbuffer.velocity(), /*tonemap=*/false).psnr;
                lastStabilityRef = imageMetrics.compare(gtLdr, prevGtLdr,
                                                        metrics::ImageMetrics::Mode::Reprojected,
                                                        &gbuffer.velocity(), /*tonemap=*/false).psnr;
                imageMetrics.setComputeSsim(ssimWas);
                stabilitySum += lastStability;
                stabilityRefSum += lastStabilityRef;
                ++stabilitySamples;
            }
            prevOutput.ensure(displayWidth, displayHeight, GL_RGBA16F, 1, "metrics.prevOutput");
            prevGtLdr.ensure(displayWidth, displayHeight, GL_RGBA16F, 1, "metrics.prevGtLdr");
            glCopyImageSubData(upscaler.output().id, GL_TEXTURE_2D, 0, 0, 0, 0,
                               prevOutput.id, GL_TEXTURE_2D, 0, 0, 0, 0,
                               displayWidth, displayHeight, 1);
            glCopyImageSubData(gtLdr.id, GL_TEXTURE_2D, 0, 0, 0, 0,
                               prevGtLdr.id, GL_TEXTURE_2D, 0, 0, 0, 0,
                               displayWidth, displayHeight, 1);
            // A cut invalidates the pair: there is no motion vector that maps
            // across it, so the next frame's number would be noise.
            stabilityHistoryValid = !cameraCut;
        }

        // Endpoint error against the rasteriser's motion vectors. Both are UV
        // displacements pointing current -> previous, so the comparison needs
        // no conversion beyond scaling to display pixels; see of_epe.comp for
        // what the number does and does not prove.
        // A cut is excluded, and not because the estimator does badly on it:
        // across a cut the *reference* is undefined. The rasteriser emits
        // motion vectors for a reprojection that never happened, so the score
        // would be measuring the distance to a number that means nothing.
        if (options.validateFlow && frameCounter >= kGtWarmup && !cameraCut) {
            lastFlow = flow.validate(gbuffer.velocity(), displayWidth, displayHeight);
            flowEpeSum += lastFlow.meanEpe;
            flowWithin1Sum += lastFlow.within1;
            flowWithin2Sum += lastFlow.within2;
            flowWorstEpe = std::max(flowWorstEpe, lastFlow.meanEpe);
            ++flowSamples;
        }

        if (captureGt && frameCounter >= kGtWarmup) {
            // Reference frames for both halves of the thesis, rendered from
            // the same scene at display resolution with no jitter:
            //   gt_*  - what the upscaler should have produced this frame;
            //   mid_* - the true frame halfway to the previous one, which is
            //           what frame generation has to approximate.
            // The scene is re-evaluated at the midpoint time with history
            // recording off, then put back, so the live frame sequence is
            // untouched.
            const int index = static_cast<int>(frameCounter - kGtWarmup);
            char name[64];

            gtBuffer.render(scene, camera, timer, false, "ref: GT full");
            upscaler.downsample(gtBuffer.color(), gtLinear, gtSs, timer, "ref: GT downsample");
            presentTexture(gtLinear, 1);
            std::snprintf(name, sizeof(name), "gt_%04d.png", index);
            writeImage(presentFbo.id, displayWidth, displayHeight, (gtDir / name).string());

            const double midTime = sceneTime - 0.5 * dt;
            renderer::Camera midCamera = camera;
            midCamera.applyScriptedPath(midTime);
            scene.update(midTime, false);
            gtBuffer.render(scene, midCamera, timer, false, "ref: GT midpoint");
            upscaler.downsample(gtBuffer.color(), gtLinear, gtSs, timer, "ref: GT downsample");
            presentTexture(gtLinear, 1);
            std::snprintf(name, sizeof(name), "mid_%04d.png", index);
            writeImage(presentFbo.id, displayWidth, displayHeight, (gtDir / name).string());
            scene.update(sceneTime, false);

            // The low-resolution frame the upscaler will actually be fed.
            presentTo(gbuffer, renderWidth, renderHeight);
            std::snprintf(name, sizeof(name), "lr_%04d.png", index);
            writeImage(presentFbo.id, displayWidth, displayHeight, (gtDir / name).string());

            gtManifest << index << ',' << sceneTime << ',' << midTime << ','
                       << renderWidth << 'x' << renderHeight << ','
                       << displayWidth << 'x' << displayHeight << '\n';
        }

        // M8: hand the frame to the presenter. Blocks while every slot is still
        // in flight -- the presenter is behind, and rendering further ahead
        // would only add latency.
        const int slotIndex = pacer.acquire();
        present::Slot& slot = pacer.slot(slotIndex);
        // The generated frame is shown only on the plain view, and not on a
        // reset, where the module hands back a copy of the current frame: that
        // would be the same image twice, counted as two frames.
        const bool showInterpolated = options.frameGen && fgActive && options.presentFg != 0 &&
                                      useUpscaler && debugMode == 0 && frameGen.valid() &&
                                      !fgReset;
        {
            gfx::GpuScope scope(timer, "Present");
            // Debug views still need the G-buffer; only the plain colour view
            // can be replaced by the upscaled image.
            const gfx::Texture2D* source = &presentTex;
            if (options.frameGen && debugMode == 9 && frameGen.valid())
                source = &frameGen.output();
            else if (options.frameGen && debugMode == 10 && frameGen.valid())
                source = &frameGen.debug();
            else if (options.frameGen && debugMode == 11 && frameGen.valid() &&
                     frameGen.ml().weights().valid())
                source = &frameGen.ml().weights();
            else if (useUpscaler && debugMode == 0)
                source = &finalFrame;
            else
                presentTo(gbuffer, renderWidth, renderHeight);

            const bool composeUi = uiOn && uiMode == UiMode::Composite;
            if (showInterpolated) {
                compose(composeProgram, frameGen.output(), slot.interpolated, composeUi,
                        tearLines ? static_cast<int>(presentedCounter % 1000000) : -1, true);
                ++presentedCounter;
            }
            compose(composeProgram, *source, slot.real, composeUi,
                    tearLines ? static_cast<int>(presentedCounter % 1000000) : -1, false);
            ++presentedCounter;
        }
        timer.endFrame();

        if (shotRequested || (lastFrame && options.autoShot)) {
            const std::string path = shotRequested
                                         ? "captures/shot_" + std::to_string(shotCounter++) + ".png"
                                         : options.outputPath;
            writeTexture(slot.real, path);
            if (showInterpolated) {
                const std::filesystem::path p(path);
                writeTexture(slot.interpolated,
                             (p.parent_path() / (p.stem().string() + "_interp" + p.extension().string()))
                                 .string());
            }
            shotRequested = false;
        }

        const double cpuMs =
            static_cast<double>(SDL_GetPerformanceCounter() - cpuStart) / frequency * 1000.0;
        cpuMsAccum += cpuMs;

        if (csv.is_open() && frameCounter >= csvFirstFrame) {
            csv << frameCounter << ',' << sceneTime << ',' << cpuMs << ',' << timer.lastTotalMs();
            if (options.validateMv) {
                csv << ',' << lastReprojected.psnr << ',' << lastReprojected.ssim
                    << ',' << lastDirect.psnr << ',' << lastDirect.ssim
                    << ',' << lastReprojected.coverage;
            }
            if (options.validateUpscale)
                csv << ',' << lastUpscale.psnr << ',' << lastUpscale.ssim
                    << ',' << lastStability << ',' << lastStabilityRef;
            if (options.validateFlow)
                csv << ',' << lastFlow.meanEpe << ',' << lastFlow.within1 << ','
                    << lastFlow.within2 << ',' << flow.sceneChangeDifference()
                    << ',' << flow.sceneChangeMax() << ',' << flow.sceneChangeMean()
                    << ',' << flow.sceneChangeMedian() << ',' << (cutHistory[2] ? 1 : 0);
            if (options.validateFg)
                csv << ',' << lastFg.psnr << ',' << lastFg.ssim << ',' << lastFgBlend.psnr
                    << ',' << lastFgBlend.ssim;
            if (options.validateFg && uiMode != UiMode::None)
                csv << ',' << lastHud.psnr << ',' << lastHud.ssim << ',' << lastHudBlend.psnr
                    << ',' << lastHudBlend.ssim;
            csv << '\n';
        }

        // No fence: see frame_pacer.h. The render thread waits for its own GPU
        // work here, so "ready" in the present log is when the frame was done.
        glFinish();
        pacer.submit(slotIndex, frameCounter, showInterpolated, sampleMs);
        drainPresents();

        ++frameCounter;
        if (lastFrame) {
            if (options.validateMv && mvSamples > 0) {
                const double n = mvSamples;
                std::printf("[validate-mv] frames=%d\n", mvSamples);
                std::printf("              reprojected   PSNR %6.2f dB   SSIM %.4f\n",
                            mvReprojectedPsnrSum / n, mvReprojectedSsimSum / n);
                std::printf("              no reprojection PSNR %6.2f dB  SSIM %.4f\n",
                            mvStaticPsnrSum / n, mvStaticSsimSum / n);
                std::printf("              gain          %+6.2f dB   %+.4f   worst window SSIM %.4f\n",
                            (mvReprojectedPsnrSum - mvStaticPsnrSum) / n,
                            (mvReprojectedSsimSum - mvStaticSsimSum) / n, mvWorstSsim);
            }
            if (options.validateUpscale && upscaleSamples > 0) {
                const double n = upscaleSamples;
                std::printf("[validate-upscale] %s  %dx%d -> %dx%d  frames=%d\n",
                            upscale::modeName(upscaleMode), renderWidth, renderHeight,
                            displayWidth, displayHeight, upscaleSamples);
                std::printf("                   PSNR %6.2f dB   SSIM %.4f   worst window %.4f\n",
                            upscalePsnrSum / n, upscaleSsimSum / n, upscaleWorstSsim);
                if (stabilitySamples > 0) {
                    const double sn = stabilitySamples;
                    std::printf("                   stability %6.2f dB   reference %6.2f dB"
                                "   delta %+.2f\n",
                                stabilitySum / sn, stabilityRefSum / sn,
                                (stabilitySum - stabilityRefSum) / sn);
                }
            }
            if (options.validateFlow && flowSamples > 0) {
                const double n = flowSamples;
                std::printf("[validate-flow] %d levels  radius %d  blocks %dx%d  frames=%d\n",
                            flow.levels(), options.ofRadius, flow.blockGridWidth(),
                            flow.blockGridHeight(), flowSamples);
                std::printf("                EPE %6.3f px   within 1 px %5.1f%%   "
                            "within 2 px %5.1f%%   worst frame %6.3f px\n",
                            flowEpeSum / n, 100.0 * flowWithin1Sum / n,
                            100.0 * flowWithin2Sum / n, flowWorstEpe);
            }
            if (options.validateFg && fgSamples > 0) {
                const double n = fgSamples;
                std::printf("[validate-fg] %dx%d  field %dx%d  frames=%d\n", displayWidth,
                            displayHeight, frameGen.fieldWidth(), frameGen.fieldHeight(),
                            fgSamples);
                std::printf("              interpolated  PSNR %6.2f dB   SSIM %.4f"
                            "   worst frame %6.2f dB\n",
                            fgPsnrSum / n, fgSsimSum / n, fgWorstPsnr);
                std::printf("              50/50 blend   PSNR %6.2f dB   SSIM %.4f\n",
                            fgBlendPsnrSum / n, fgBlendSsimSum / n);
                std::printf("              gain          %+6.2f dB   %+.4f\n",
                            (fgPsnrSum - fgBlendPsnrSum) / n, (fgSsimSum - fgBlendSsimSum) / n);
                if (uiMode != UiMode::None) {
                    std::printf("              HUD region (%s)  PSNR %6.2f dB   SSIM %.4f"
                                "   blend %6.2f dB / %.4f\n",
                                uiMode == UiMode::Baked ? "baked" : "composite", hudPsnrSum / n,
                                hudSsimSum / n, hudBlendPsnrSum / n, hudBlendSsimSum / n);
                }
            }
            for (const gfx::GpuTimer::Result& r : timer.results())
                std::printf("[gpu] %-24s %7.3f ms (last %7.3f)\n", r.name.c_str(), r.ms, r.lastMs);
            std::printf("[gpu] %-24s %7.3f ms (last %7.3f)\n", "TOTAL", timer.totalMs(),
                        timer.lastTotalMs());
            running = false;
        }

        ++framesSinceStat;
        statTimer += dt;
        if (statTimer >= 0.5) {
            const double fps = framesSinceStat / statTimer;
            const double cpuMs = cpuMsAccum / framesSinceStat;
            char title[256];
            std::snprintf(title, sizeof(title),
                          "fsr3lite | %s %dx%d -> %dx%d | %.1f FPS rendered, %.1f presented | "
                          "GPU %.2f ms | CPU %.2f ms",
                          scaleLabel, renderWidth, renderHeight, displayWidth, displayHeight, fps,
                          pacer.presentedFps(), timer.totalMs(), cpuMs);
            SDL_SetWindowTitle(window, title);
            statTimer = 0.0;
            framesSinceStat = 0;
            cpuMsAccum = 0.0;
        }
    }

    pacer.stop();
    drainPresents();
    if (!presentHistory.empty()) {
        // Summary of what reached the screen; scripts/run_pacing.py does the
        // full analysis from --present-csv.
        std::vector<double> intervals;
        double realLatency = 0.0, firstLatency = 0.0;
        int reals = 0, generated = 0;
        for (size_t i = 0; i < presentHistory.size(); ++i) {
            const present::PresentRecord& r = presentHistory[i];
            if (i > 0) intervals.push_back(r.presentMs - presentHistory[i - 1].presentMs);
            if (r.interpolated) {
                firstLatency += r.presentMs - r.sampleMs;
                ++generated;
            } else {
                realLatency += r.presentMs - r.sampleMs;
                ++reals;
            }
        }
        if (!intervals.empty()) {
            double mean = 0.0, var = 0.0;
            for (double v : intervals) mean += v;
            mean /= intervals.size();
            for (double v : intervals) var += (v - mean) * (v - mean);
            var /= intervals.size();
            std::vector<double> sorted = intervals;
            std::sort(sorted.begin(), sorted.end());
            const double p99 = sorted[static_cast<size_t>(0.99 * (sorted.size() - 1))];
            std::printf("[present] %zu presents (%d real, %d generated)  interval mean %.3f ms "
                        "(%.1f fps)  std %.3f ms  p99 %.3f ms\n",
                        presentHistory.size(), reals, generated, mean, 1000.0 / mean,
                        std::sqrt(var), p99);
            std::printf("[present] latency sample -> real frame %.2f ms", reals ? realLatency / reals : 0.0);
            if (generated) std::printf("   sample -> generated frame %.2f ms", firstLatency / generated);
            std::printf("\n");
        }
    }
    pacer.destroy();
    hud.destroy();
    composeProgram.destroy();
    composeHdrProgram.destroy();
    bakedFrame.destroy();
    gtMidUi.destroy();
    fgUi.destroy();
    blendUi.destroy();
    hudCandidate.destroy();
    hudReference.destroy();
    hudBlend.destroy();
    if (loadCreated) loadBuffer.destroy();
    timer.destroy();
    imageMetrics.destroy();
    frameGen.destroy();
    flow.destroy();
    upscaler.destroy();
    ldrInput.destroy();
    gtLinear.destroy();
    gtLdr.destroy();
    gtMidLdr.destroy();
    prevOutput.destroy();
    prevGtLdr.destroy();
    gtBuffer.destroy();
    presentProgram.destroy();
    presentTex.destroy();
    presentFbo.destroy();
    helloCompute.destroy();
    gbuffer.destroy();
    scene.destroy();
    glDeleteVertexArrays(1, &emptyVao);
    SDL_GL_DeleteContext(context);
    SDL_GL_DeleteContext(presentContext);
    SDL_DestroyWindow(renderWindow);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
