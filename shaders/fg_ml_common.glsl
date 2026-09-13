// M9: what the learned blend sees and what it chooses from.
//
// Shared by the feature pass (fg_ml_features.comp) and the blend pass
// (fg_ml_blend.comp): the candidates the network was trained to weight are, by
// construction, the ones it weights at run time. The layout of both lists is
// documented once, in src/ml/blend_net.h.

#include "fg_common.glsl"

layout(binding = 0) uniform sampler2D uCurrent;    // display res, display-referred
layout(binding = 1) uniform sampler2D uPrevious;
layout(binding = 2) uniform sampler2D uGameField;  // resolved + inpainting pyramid
layout(binding = 3) uniform sampler2D uFlowField;
layout(binding = 4) uniform sampler2D uMasks;      // x = occluded vs prev, y = vs cur
layout(binding = 5) uniform sampler2D uHeuristic;  // passes 7-9, a = coverage

uniform ivec2 uDisplaySize;
uniform ivec2 uFieldSize;
uniform int uLevels;
uniform int uGameEnabled;
uniform int uFlowEnabled;
uniform int uMaskEnabled;

const int kMlCandidates = 7;
const int kMlFeatureLayers = 5;

struct MlSample {
    vec3 c[kMlCandidates];
    vec4 heuristic;
    vec2 occ;
    vec4 game;
    vec4 flow;
    int gameLevel;
    int flowLevel;
    vec2 uv;
};

float mlInside(vec2 uv) {
    return all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0))) ? 1.0 : 0.0;
}

MlSample mlGather(ivec2 pos) {
    MlSample s;
    s.uv = (vec2(pos) + 0.5) / vec2(uDisplaySize);
    s.heuristic = texelFetch(uHeuristic, pos, 0);
    s.occ = uMaskEnabled != 0 ? texture(uMasks, s.uv).rg : vec2(0.0);
    s.gameLevel = uLevels;
    s.flowLevel = uLevels;
    s.game = vec4(0.0);
    s.flow = vec4(0.0);
    if (uGameEnabled != 0) s.game = fgFetchField(uGameField, s.uv, uFieldSize, uLevels, s.gameLevel);
    if (uFlowEnabled != 0) s.flow = fgFetchField(uFlowField, s.uv, uFieldSize, uLevels, s.flowLevel);

    // A field with no vector here offers the heuristic's colour in its slots:
    // picking an absent candidate then costs nothing, and the network does not
    // have to learn to avoid slots that hold garbage.
    const vec3 h = s.heuristic.rgb;
    s.c[0] = h;
    s.c[1] = s.game.w > 0.5 ? texture(uPrevious, s.uv + 0.5 * s.game.xy).rgb : h;
    s.c[2] = s.game.w > 0.5 ? texture(uCurrent, s.uv - 0.5 * s.game.xy).rgb : h;
    s.c[3] = s.flow.w > 0.5 ? texture(uPrevious, s.uv + 0.5 * s.flow.xy).rgb : h;
    s.c[4] = s.flow.w > 0.5 ? texture(uCurrent, s.uv - 0.5 * s.flow.xy).rgb : h;
    s.c[5] = texture(uCurrent, s.uv).rgb;
    s.c[6] = texture(uPrevious, s.uv).rgb;
    return s;
}

// Square root of the colour distance: the interesting differences are the
// small ones, and the root spreads them over more of the range.
float mlDistance(vec3 a, vec3 b) { return sqrt(fgColorDistance(a, b)); }
float mlLuma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
// Display pixels, log-compressed: 0 px -> 0, 255 px -> 1.
float mlMagnitude(vec2 mv) { return log2(1.0 + length(mv * vec2(uDisplaySize))) / 8.0; }

void mlFeatures(MlSample s, out vec4 f[kMlFeatureLayers]) {
    const float gv = s.game.w > 0.5 ? 1.0 : 0.0;
    const float fv = s.flow.w > 0.5 ? 1.0 : 0.0;
    const float levels = float(max(uLevels, 1));
    const vec3 gameMean = 0.5 * (s.c[1] + s.c[2]);
    const vec3 flowMean = 0.5 * (s.c[3] + s.c[4]);

    f[0] = vec4(gv, fv, s.occ.x, s.occ.y);
    f[1] = vec4(gv * mlInside(s.uv + 0.5 * s.game.xy), gv * mlInside(s.uv - 0.5 * s.game.xy),
                fv * mlInside(s.uv + 0.5 * s.flow.xy), fv * mlInside(s.uv - 0.5 * s.flow.xy));
    f[2] = vec4(gv * mlDistance(s.c[1], s.c[2]), fv * mlDistance(s.c[3], s.c[4]),
                gv * fv * mlDistance(gameMean, flowMean), mlDistance(s.c[5], s.c[6]));
    f[3] = vec4(gv * mlDistance(s.c[0], gameMean), fv * mlDistance(s.c[0], flowMean),
                s.heuristic.a, float(s.gameLevel) / levels);
    f[4] = vec4(float(s.flowLevel) / levels, gv * mlMagnitude(s.game.xy),
                gv * fv * mlMagnitude(s.game.xy - s.flow.xy), mlLuma(s.c[0]));
}
