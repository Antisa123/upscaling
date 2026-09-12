#version 460 core

in vec2 vUV;
out vec4 oColor;

layout(binding = 0) uniform sampler2D uColor;
layout(binding = 1) uniform sampler2D uDepth;
layout(binding = 2) uniform sampler2D uVelocity;
layout(binding = 3) uniform sampler2D uPrevColor;
// M5 dilate output: xy = dilated motion vector, z = disocclusion, w = depth.
layout(binding = 4) uniform sampler2D uDilated;
// M6 optical flow: one RGBA16F texel per 8x8 luma block. xy = UV displacement
// current -> previous, z = the winning block's mean absolute luma difference.
layout(binding = 5) uniform sampler2D uFlow;

uniform int uMode;  // 0 colour, 1 depth, 2 motion vectors, 3 |mv|,
                    // 4 reprojection error, 5 disocclusion mask,
                    // 6 optical flow, 7 game vectors on the flow's scale,
                    // 8 optical flow match error
uniform float uMvScale;    // pixels mapped to full intensity in modes 2/3
uniform float uNearPlane;
uniform vec2 uRenderSize;
uniform float uExposure;
// The spatial upscalers output display-referred colour already; tone mapping it
// a second time would darken the image and invalidate every comparison.
uniform int uToneMap;
uniform vec2 uFlowGrid;    // blocks across and down
uniform float uFlowScale;  // display pixels mapped to full saturation
uniform vec2 uDisplaySize;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    if (uMode == 1) {
        // Reverse-Z with infinite far plane: depth = near / distance.
        float d = texture(uDepth, vUV).r;
        float linearDepth = d > 0.0 ? uNearPlane / d : 1e6;
        float v = 1.0 - exp(-linearDepth * 0.08);
        oColor = vec4(vec3(v), 1.0);
        return;
    }
    if (uMode == 2 || uMode == 3) {
        vec2 mv = texture(uVelocity, vUV).rg;
        // Express motion in pixels so the visualisation is resolution
        // independent.
        vec2 mvPixels = mv * uRenderSize;
        float mag = length(mvPixels);
        if (uMode == 3) {
            float t = clamp(mag / uMvScale, 0.0, 1.0);
            oColor = vec4(t, t * t, 1.0 - t, 1.0);
            return;
        }
        // Hue encodes direction, saturation encodes magnitude: a static scene
        // is white, and any coloured region means the pixel is moving.
        float angle = atan(mvPixels.y, mvPixels.x) / 6.2831853 + 0.5;
        oColor = vec4(hsv2rgb(vec3(angle, clamp(mag / uMvScale, 0.0, 1.0), 1.0)), 1.0);
        return;
    }
    if (uMode == 4) {
        // Correctness check for the velocity buffer: fetch the previous frame
        // at the reprojected position and compare. With jitter disabled and a
        // static shading model the result must be black everywhere except at
        // disocclusions and at pixels that leave the screen.
        vec2 mv = texture(uVelocity, vUV).rg;
        vec2 prevUV = vUV + mv;
        vec3 current = texture(uColor, vUV).rgb;
        vec3 history = texture(uPrevColor, prevUV).rgb;
        float err = length(current - history);
        bool offscreen = any(lessThan(prevUV, vec2(0.0))) || any(greaterThan(prevUV, vec2(1.0)));
        if (offscreen) {
            oColor = vec4(0.0, 0.0, 0.35, 1.0);  // blue: no history available
            return;
        }
        float t = clamp(err * 4.0, 0.0, 1.0);
        oColor = vec4(t, 1.0 - t, 0.0, 1.0);     // green = match, red = mismatch
        return;
    }

    if (uMode == 5) {
        // What fraction of its history each pixel is about to lose, from the
        // depth test rather than from colour. Should light up along the
        // trailing edge of every silhouette the camera moves past, and nowhere
        // on flat surfaces -- a lit-up floor means the tolerance is too tight
        // and the pass is throwing away history it should keep.
        float d = texture(uDilated, vUV).b;
        oColor = vec4(d, 1.0 - d, 0.0, 1.0);
        return;
    }

    if (uMode == 6 || uMode == 7 || uMode == 8) {
        // Modes 6 and 7 share a scale and an encoding on purpose: the only way
        // to judge a flow field by eye is against the one field that is known
        // to be right. Flipping between them, the block structure should be the
        // only visible difference -- same hues in the same places. Where the
        // hues disagree the estimator is wrong, and where the game vectors are
        // undefined (shadows moving over a static floor, reflections) only the
        // flow has an answer at all.
        vec2 mvPixels;
        if (uMode == 7) {
            mvPixels = texture(uVelocity, vUV).rg * uDisplaySize;
        } else {
            const ivec2 block = ivec2(clamp(vUV, vec2(0.0), vec2(0.9999)) * uFlowGrid);
            const vec4 f = texelFetch(uFlow, block, 0);
            if (uMode == 8) {
                // Mean absolute luminance difference of the winning match.
                // Flat where the match is good; bright at disocclusions, on
                // repeating texture, and everywhere the search simply failed.
                const float e = clamp(f.z * 8.0, 0.0, 1.0);
                oColor = vec4(e, 1.0 - e, 0.0, 1.0);
                return;
            }
            mvPixels = f.xy * uDisplaySize;
        }
        const float mag = length(mvPixels);
        const float angle = atan(mvPixels.y, mvPixels.x) / 6.2831853 + 0.5;
        oColor = vec4(hsv2rgb(vec3(angle, clamp(mag / uFlowScale, 0.0, 1.0), 1.0)), 1.0);
        return;
    }

    vec3 c = texture(uColor, vUV).rgb;
    if (uToneMap == 0) {
        oColor = vec4(c, 1.0);
        return;
    }
    // Reinhard + sRGB-ish encode; the internal pipeline stays linear HDR.
    vec3 hdr = c * uExposure;
    vec3 mapped = hdr / (1.0 + hdr);
    oColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
}
