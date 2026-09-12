#version 460 core

in vec3 vNormal;
in vec2 vUV;
in vec4 vCurClip;
in vec4 vPrevClip;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec2 oVelocity;
layout(location = 2) out float oReactive;
// Linear view-space distance, this frame and where this surface point was last
// frame. The upscaler's disocclusion test needs both: comparing the second
// against the *history* of the first is what tells it whether the surface the
// motion vector points at is the surface that was actually there.
//
// Both come free. The projection is reverse-Z with an infinite far plane, so
// clip.w = -viewZ exactly -- no reconstruction, no inverse matrix, and no
// precision loss from round-tripping through a [0,1] depth buffer whose
// resolution far from the camera is measured in whole metres.
layout(location = 3) out vec2 oDepthInfo;

uniform vec3 uAlbedo;
uniform int uPattern;
uniform float uPatternScale;
// 0 = point sampled (aliased on purpose), 1 = analytically band-limited.
// The aliased mode is the workload a temporal upscaler is supposed to resolve;
// the filtered mode is the clean reference content used when a measurement
// must not be dominated by sampling noise (motion vector validation).
uniform int uPatternFilter;

// glTF base colour. Mipmapped and anisotropically filtered, so unlike the
// procedural patterns it is already band-limited by the sampler.
uniform sampler2D uAlbedoTex;
uniform int uHasAlbedoTex;
// Texture LOD bias. A frame rendered at 1/1.5 of display resolution picks mip
// levels for 1/1.5 of the detail, so an upscaler is handed an image that has
// already had the missing frequencies filtered out of it and no amount of
// temporal accumulation can bring them back: averaging jittered samples of a
// band-limited signal returns the same band-limited signal. Biasing the mip
// selection towards the *output* resolution is what puts that detail back into
// the render target, aliased, for the upscaler to resolve. FSR2 specifies
// log2(renderRes/displayRes) - 1 for exactly this reason.
uniform float uMipBias;
// Scales the reactive mask written for alpha-tested surfaces. 0 disables it,
// which is the pre-M5 behaviour and the ablation row the mask has to beat.
uniform float uReactiveScale;

float checkerPoint(vec2 p) {
    vec2 c = floor(p);
    return mod(c.x + c.y, 2.0) < 1.0 ? 0.25 : 1.0;
}

// Box filter of the checkerboard over the pixel footprint (Inigo Quilez's
// gradient-box formulation): integrates the square wave analytically instead
// of point sampling it, so the surface fades to its mean under minification.
float checkerFiltered(vec2 p, vec2 w) {
    vec2 i = 2.0 * (abs(fract((p - 0.5 * w) * 0.5) - 0.5)
                  - abs(fract((p + 0.5 * w) * 0.5) - 0.5)) / w;
    return mix(0.25, 1.0, 0.5 - 0.5 * i.x * i.y);
}

float stripePoint(float t) {
    return step(0.5, fract(t)) * 0.7 + 0.3;
}

// Integral of step(0.5, fract(x)) with respect to x, used to box filter the
// stripes over one pixel.
float stripeIntegral(float x) {
    return floor(x) * 0.5 + max(fract(x) - 0.5, 0.0);
}

float stripeFiltered(float t, float w) {
    float h = 0.5 * w;
    return ((stripeIntegral(t + h) - stripeIntegral(t - h)) / w) * 0.7 + 0.3;
}

float patternValue(vec2 uv) {
    if (uPattern == 1) {
        vec2 p = uv * uPatternScale;
        if (uPatternFilter == 0) return checkerPoint(p);
        return checkerFiltered(p, fwidth(p) + 1e-5);
    } else if (uPattern == 2) {
        float t = uv.y * uPatternScale;
        if (uPatternFilter == 0) return stripePoint(t);
        return stripeFiltered(t, fwidth(t) + 1e-5);
    }
    return 1.0;
}

void main() {
    const vec3 lightDir = normalize(vec3(0.4, 0.9, 0.35));
    vec3 n = normalize(vNormal);
    float ndotl = max(dot(n, lightDir), 0.0);

    vec3 albedo = uAlbedo * patternValue(vUV);
    float alphaEdge = 0.0;
    if (uHasAlbedoTex != 0) {
        vec4 texel = texture(uAlbedoTex, vUV, uMipBias);
        if (texel.a < 0.5) discard;  // alpha-masked foliage and drapes
        albedo *= texel.rgb;
        // How close this fragment sits to the alpha cutoff. A pixel just above
        // it is one the test keeps this frame and may drop the next, purely
        // because the jitter moved the sample; its motion vector is correct and
        // its history is still wrong. That is exactly the condition a reactive
        // mask describes, and it is the only source of one in a scene with no
        // particles and no real transparency.
        alphaEdge = 1.0 - smoothstep(0.5, 0.85, texel.a);
    }
    // Cheap hemispheric ambient keeps unlit faces readable without a full
    // lighting pipeline; the project is about resolution, not shading.
    vec3 ambient = mix(vec3(0.05, 0.06, 0.09), vec3(0.20, 0.22, 0.26), n.y * 0.5 + 0.5);
    vec3 color = albedo * (ambient + vec3(1.9, 1.85, 1.7) * ndotl);

    oColor = vec4(color, 1.0);

    // UV-space motion vector, current -> previous: previousUV = uv + velocity.
    vec2 curUV = (vCurClip.xy / vCurClip.w) * 0.5 + 0.5;
    vec2 prevUV = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    oVelocity = prevUV - curUV;
    oDepthInfo = vec2(vCurClip.w, vPrevClip.w);

    // Opaque geometry: history is fully trusted.
    oReactive = alphaEdge * uReactiveScale;
}
