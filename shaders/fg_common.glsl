// M7: helpers shared by the frame generation passes.
//
// The two motion vector fields are built by *scattering*: every source pixel
// writes to wherever its surface is at t-0.5, so several pixels land on the
// same target texel and the winner has to be decided without a second pass
// over the candidates. That is what the atomic is for, and it is why the
// priority lives in the high bits of the same 32-bit word as the payload --
// imageAtomicMax then compares priority first and the payload only as a
// tie-break.
//
// One image per component, as in FfxFrameInterpolation: the vector is a 16-bit
// float in the low half, the priority a 16-bit integer in the high half.
// Splitting x and y across two images means a tie could in principle take x
// from one source pixel and y from another, but both components carry the same
// priority, so that needs two candidates that are equal by every criterion --
// and then either answer is as good as the other.
//
// Priority 0 means "nothing was scattered here". Every real candidate is
// therefore clamped to at least 1, otherwise a genuine sample with the lowest
// possible score would be indistinguishable from an empty texel.

const uint kFgPriorityShift = 16u;

uint fgPack(uint priority, float value) {
    return (min(priority, 0xFFFFu) << kFgPriorityShift) |
           (packHalf2x16(vec2(value, 0.0)) & 0xFFFFu);
}

float fgValue(uint packed) { return unpackHalf2x16(packed & 0xFFFFu).x; }
uint fgPriority(uint packed) { return packed >> kFgPriorityShift; }

// Fetch a resolved vector field, walking up the inpainting pyramid until a
// level has an answer. Point sampling at every level on purpose: the field is
// a sparse scatter with an explicit validity channel, and a linear filter
// would average valid vectors with empty texels and produce a vector that is
// neither.
// `foundLevel` reports how far up the walk had to go: 0 means the texel held
// a real scattered sample, anything higher means the vector is a guess made
// from a region 2^level texels wide, and the consumer is entitled to trust it
// less.
vec4 fgFetchField(sampler2D field, vec2 uv, ivec2 size, int levels, out int foundLevel) {
    for (int l = 0; l < levels; ++l) {
        const ivec2 s = max(size >> l, ivec2(1));
        const ivec2 t = clamp(ivec2(uv * vec2(s)), ivec2(0), s - 1);
        const vec4 v = texelFetch(field, t, l);
        if (v.w > 0.5) {
            foundLevel = l;
            return v;
        }
    }
    foundLevel = levels;
    return vec4(0.0);
}

// Mean absolute difference of two display-referred colours, in [0,1].
float fgColorDistance(vec3 a, vec3 b) { return dot(abs(a - b), vec3(1.0 / 3.0)); }
