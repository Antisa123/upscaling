// M8: composition of one presented image.
//
// Included by ui_compose.comp (RGBA8 target: what the presenter shows) and by
// ui_compose_hdr.comp (RGBA16F target: the frame with the HUD baked in before
// generation, and the measurement path's reference). The includer declares
// `uOutput`.
//
// Source, then the HUD layer over it, then optionally the tear-line strip.

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D uSource;  // display resolution, display-referred
layout(binding = 1) uniform sampler2D uLayer;   // HUD, straight alpha

uniform ivec2 uDisplaySize;
uniform int uUi;
uniform int uTearIndex;  // presented-frame counter; -1 = no tear lines
uniform int uInterpolated;

const int kTearWidth = 24;
const int kTearSteps = 16;

void main() {
    const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pos, uDisplaySize))) return;

    vec3 c = texelFetch(uSource, pos, 0).rgb;
    if (uUi != 0) {
        const vec4 ui = texelFetch(uLayer, pos, 0);
        c = mix(c, ui.rgb, ui.a);
    }

    // FSR3's debug tear lines, in spirit: a strip whose colour says which kind
    // of frame this is -- green real, magenta generated -- and a white block
    // that steps one slot down per presented frame. Filmed or screenshotted,
    // a correct sequence alternates colours with the block moving evenly; a
    // frame shown twice stalls the block, a dropped one makes it skip.
    if (uTearIndex >= 0 && pos.x < kTearWidth) {
        c = uInterpolated != 0 ? vec3(1.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
        const int stepPx = max(uDisplaySize.y / kTearSteps, 1);
        const int slot = (kTearSteps - 1) - (uTearIndex % kTearSteps);
        if (pos.y / stepPx == slot) c = vec3(1.0);
    }

    imageStore(uOutput, pos, vec4(c, 1.0));
}
