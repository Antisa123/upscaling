#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uPrevModel;
uniform mat4 uCurVPJittered;  // what we actually rasterise with
uniform mat4 uCurVP;          // unjittered, for motion vectors
uniform mat4 uPrevVP;         // unjittered, previous frame
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec2 vUV;
out vec4 vCurClip;   // unjittered
out vec4 vPrevClip;  // unjittered

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vec4 prevWorld = uPrevModel * vec4(aPosition, 1.0);

    vNormal = normalize(uNormalMatrix * aNormal);
    vUV = aUV;

    // Motion vectors are built from unjittered clip positions. Mixing the
    // jitter in here is the single most common way to ruin a temporal
    // upscaler: the history would be reprojected by the jitter pattern itself.
    vCurClip = uCurVP * world;
    vPrevClip = uPrevVP * prevWorld;

    gl_Position = uCurVPJittered * world;
}
